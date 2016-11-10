/* Copyright (C) 2016 Free Software Foundation, Inc.
   Contributed by Agustina Arzille <avarzille@riseup.net>, 2016.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either
   version 2 of the license, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, see
   <http://www.gnu.org/licenses/>.
*/

#include <kern/gsync.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>
#include <kern/lock.h>
#include <kern/list.h>
#include <vm/vm_map.h>

/* An entry in the global hash table. */
struct gsync_hbucket
{
  struct list entries;
  decl_simple_lock_data (, lock)
};

/* A key used to uniquely identify an address that a thread is
 * waiting on. Its members' values depend on whether said
 * address is shared or task-local. */
struct gsync_key
{
  unsigned long u;
  unsigned long v;
};

/* A thread that is blocked on an address with 'gsync_wait'. */
struct gsync_waiter
{
  struct list link;
  struct gsync_key key;
  thread_t waiter;
};

#define GSYNC_NBUCKETS   512
static struct gsync_hbucket gsync_buckets[GSYNC_NBUCKETS];

void gsync_setup (void)
{
  int i;
  for (i = 0; i < GSYNC_NBUCKETS; ++i)
    {
      list_init (&gsync_buckets[i].entries);
      simple_lock_init (&gsync_buckets[i].lock);
    }
}

/* Convenience comparison functions for gsync_key's. */

static inline int
gsync_key_eq (const struct gsync_key *lp,
  const struct gsync_key *rp)
{
  return (lp->u == rp->u && lp->v == rp->v);
}

static inline int
gsync_key_lt (const struct gsync_key *lp,
  const struct gsync_key *rp)
{
  return (lp->u < rp->u || (lp->u == rp->u && lp->v < rp->v));
}

#define MIX2_LL(x, y)   ((((x) << 5) | ((x) >> 27)) ^ (y))

static inline unsigned int
gsync_key_hash (const struct gsync_key *keyp)
{
  unsigned int ret = sizeof (void *);
#ifndef __LP64__
  ret = MIX2_LL (ret, keyp->u);
  ret = MIX2_LL (ret, keyp->v);
#else
  ret = MIX2_LL (ret, keyp->u & ~0U);
  ret = MIX2_LL (ret, keyp->u >> 32);
  ret = MIX2_LL (ret, keyp->v & ~0U);
  ret = MIX2_LL (ret, keyp->v >> 32);
#endif
  return (ret);
}

/* Test if the passed VM Map can access the address ADDR. The
 * parameter FLAGS is used to specify the width and protection
 * of the address. */
static int
valid_access_p (vm_map_t map, vm_offset_t addr, int flags)
{
  vm_prot_t prot = VM_PROT_READ |
    ((flags & GSYNC_MUTATE) ? VM_PROT_WRITE : 0);
  vm_offset_t size = sizeof (unsigned int) *
    ((flags & GSYNC_QUAD) ? 2 : 1);

  vm_map_entry_t entry;
  return (vm_map_lookup_entry (map, addr, &entry) &&
    entry->vme_end >= addr + size &&
    (prot & entry->protection) == prot);
}

/* Given a task and an address, initialize the key at *KEYP and
 * return the corresponding bucket in the global hash table. */
static int
gsync_fill_key (task_t task, vm_offset_t addr,
  int flags, struct gsync_key *keyp)
{
  if (flags & GSYNC_SHARED)
    {
      /* For a shared address, we need the VM object
       * and offset as the keys. */
      vm_map_t map = task->map;
      vm_prot_t prot = VM_PROT_READ |
        ((flags & GSYNC_MUTATE) ? VM_PROT_WRITE : 0);
      vm_map_version_t ver;
      vm_prot_t rpr;
      vm_object_t obj;
      vm_offset_t off;
      boolean_t wired_p;

      if (unlikely (vm_map_lookup (&map, addr, prot, &ver,
          &obj, &off, &rpr, &wired_p) != KERN_SUCCESS))
        return (-1);

      /* The VM object is returned locked. However, we check the
       * address' accessibility later, so we can release it. */
      vm_object_unlock (obj);

      keyp->u = (unsigned long)obj;
      keyp->v = (unsigned long)off;
    }
  else
    {
      /* Task-local address. The keys are the task's map and
       * the virtual address itself. */
      keyp->u = (unsigned long)task->map;
      keyp->v = (unsigned long)addr;
    }

  return ((int)(gsync_key_hash (keyp) % GSYNC_NBUCKETS));
}

static inline struct gsync_waiter*
node_to_waiter (struct list *nodep)
{
  return (list_entry (nodep, struct gsync_waiter, link));
}

static inline struct list*
gsync_find_key (const struct list *entries,
  const struct gsync_key *keyp, int *exactp)
{
  /* Look for a key that matches. We take advantage of the fact
   * that the entries are sorted to break out of the loop as
   * early as possible. */
  struct list *runp;
  list_for_each (entries, runp)
    {
      struct gsync_waiter *p = node_to_waiter (runp);
      if (gsync_key_lt (keyp, &p->key))
        break;
      else if (gsync_key_eq (keyp, &p->key))
        {
          if (exactp != 0)
            *exactp = 1;
          break;
        }
    }

  return (runp);
}

kern_return_t gsync_wait (task_t task, vm_offset_t addr,
  unsigned int lo, unsigned int hi, natural_t msec, int flags)
{
  if (unlikely (task != current_task()))
    /* Not implemented yet.  */
    return (KERN_FAILURE);

  struct gsync_waiter w;
  int bucket = gsync_fill_key (task, addr, flags, &w.key);

  if (unlikely (bucket < 0))
    return (KERN_INVALID_ADDRESS);

  /* Test that the address is actually valid for the
   * given task. Do so with the read-lock held in order
   * to prevent memory deallocations. */
  vm_map_lock_read (task->map);

  struct gsync_hbucket *hbp = gsync_buckets + bucket;
  simple_lock (&hbp->lock);

  if (unlikely (!valid_access_p (task->map, addr, flags)))
    {
      simple_unlock (&hbp->lock);
      vm_map_unlock_read (task->map);
      return (KERN_INVALID_ADDRESS);
    }

  /* Before doing any work, check that the expected value(s)
   * match the contents of the address. Otherwise, the waiting
   * thread could potentially miss a wakeup. */
  if (((unsigned int *)addr)[0] != lo ||
      ((flags & GSYNC_QUAD) &&
        ((unsigned int *)addr)[1] != hi))
    {
      simple_unlock (&hbp->lock);
      vm_map_unlock_read (task->map);
      return (KERN_INVALID_ARGUMENT);
    }

  vm_map_unlock_read (task->map);

  /* Look for the first entry in the hash bucket that
   * compares strictly greater than this waiter. */
  struct list *runp;
  list_for_each (&hbp->entries, runp)
    {
      struct gsync_waiter *p = node_to_waiter (runp);
      if (gsync_key_lt (&w.key, &p->key))
        break;
    }

  /* Finally, add ourselves to the list and go to sleep. */
  list_add (runp->prev, runp, &w.link);
  w.waiter = current_thread ();

  if (flags & GSYNC_TIMED)
    thread_will_wait_with_timeout (w.waiter, msec);
  else
    thread_will_wait (w.waiter);

  thread_sleep (0, (simple_lock_t)&hbp->lock, TRUE);

  /* We're back. */
  kern_return_t ret = current_thread()->wait_result;
  if (ret != THREAD_AWAKENED)
    {
      /* We were interrupted or timed out. */
      simple_lock (&hbp->lock);
      if (w.link.next != 0)
        list_remove (&w.link);
      simple_unlock (&hbp->lock);

      /* Map the error code. */
      ret = ret == THREAD_INTERRUPTED ?
        KERN_INTERRUPTED : KERN_TIMEDOUT;
    }
  else
    ret = KERN_SUCCESS;

  return (ret);
}

/* Remove a waiter from the queue, wake it up, and
 * return the next node. */
static inline struct list*
dequeue_waiter (struct list *nodep)
{
  struct list *nextp = list_next (nodep);
  list_remove (nodep);
  list_node_init (nodep);
  clear_wait (node_to_waiter(nodep)->waiter,
    THREAD_AWAKENED, FALSE);
  return (nextp);
}

kern_return_t gsync_wake (task_t task,
  vm_offset_t addr, unsigned int val, int flags)
{
  if (unlikely (task != current_task()))
    /* Not implemented yet.  */
    return (KERN_FAILURE);

  struct gsync_key key;
  int bucket = gsync_fill_key (task, addr, flags, &key);

  if (unlikely (bucket < 0))
    return (KERN_INVALID_ADDRESS);

  kern_return_t ret = KERN_INVALID_ARGUMENT;

  vm_map_lock_read (task->map);
  struct gsync_hbucket *hbp = gsync_buckets + bucket;
  simple_lock (&hbp->lock);

  if (unlikely (!valid_access_p (task->map, addr, flags)))
    {
      simple_unlock (&hbp->lock);
      vm_map_unlock_read (task->map);
      return (KERN_INVALID_ADDRESS);
    }

  if (flags & GSYNC_MUTATE)
    /* Set the contents of the address to the specified value,
     * even if we don't end up waking any threads. Note that
     * the buckets' simple locks give us atomicity. */
    *(unsigned int *)addr = val;

  vm_map_unlock_read (task->map);

  int found = 0;
  struct list *runp = gsync_find_key (&hbp->entries, &key, &found);
  if (found)
    {
      do
        runp = dequeue_waiter (runp);
      while ((flags & GSYNC_BROADCAST) &&
        !list_end (&hbp->entries, runp) &&
        gsync_key_eq (&node_to_waiter(runp)->key, &key));

      ret = KERN_SUCCESS;
    }

  simple_unlock (&hbp->lock);
  return (ret);
}

kern_return_t gsync_requeue (task_t task, vm_offset_t src,
  vm_offset_t dst, boolean_t wake_one, int flags)
{
  if (unlikely (task != current_task()))
    /* Not implemented yet.  */
    return (KERN_FAILURE);

  struct gsync_key src_k, dst_k;
  int src_bkt = gsync_fill_key (task, src, flags, &src_k);
  int dst_bkt = gsync_fill_key (task, dst, flags, &dst_k);

  if ((src_bkt | dst_bkt) < 0)
    return (KERN_INVALID_ADDRESS);

  vm_map_lock_read (task->map);

  /* We don't actually dereference or modify the contents
   * of the addresses, but we still check that they can
   * be accessed by the task. */
  if (unlikely (!valid_access_p (task->map, src, flags) ||
      !valid_access_p (task->map, dst, flags)))
    {
      vm_map_unlock_read (task->map);
      return (KERN_INVALID_ADDRESS);
    }

  vm_map_unlock_read (task->map);

  /* If we're asked to unconditionally wake up a waiter, then
   * we need to remove a maximum of two threads from the queue. */
  unsigned int nw = 1 + wake_one;
  struct gsync_hbucket *bp1 = gsync_buckets + src_bkt;
  struct gsync_hbucket *bp2 = gsync_buckets + dst_bkt;

  /* Acquire the locks in order, to prevent any potential deadlock. */
  if (bp1 == bp2)
    simple_lock (&bp1->lock);
  else if ((unsigned long)bp1 < (unsigned long)bp2)
    {
      simple_lock (&bp1->lock);
      simple_lock (&bp2->lock);
    }
  else
    {
      simple_lock (&bp2->lock);
      simple_lock (&bp1->lock);
    }

  kern_return_t ret = KERN_SUCCESS;
  int exact;
  struct list *inp = gsync_find_key (&bp1->entries, &src_k, &exact);

  if (!exact)
    /* There are no waiters in the source queue. */
    ret = KERN_INVALID_ARGUMENT;
  else
    {
      struct list *outp = gsync_find_key (&bp2->entries, &dst_k, 0);

      /* We're going to need a node that points one past the
       * end of the waiters in the source queue. */
      struct list *endp = inp;

      do
        {
          /* Modify the keys while iterating. */
          node_to_waiter(endp)->key = dst_k;
          endp = list_next (endp);
        }
      while (((flags & GSYNC_BROADCAST) || --nw != 0) &&
        !list_end (&bp1->entries, endp) &&
        gsync_key_eq (&node_to_waiter(endp)->key, &src_k));

      /* Splice the list by removing waiters from the source queue
       * and inserting them into the destination queue. */
      inp->prev->next = endp;
      endp->prev->next = outp->next;
      endp->prev = inp->prev;

      outp->next = inp;
      inp->prev = outp;

      if (wake_one)
        (void)dequeue_waiter (inp);
    }

  /* Release the locks and we're done.*/
  simple_unlock (&bp1->lock);
  if (bp1 != bp2)
    simple_unlock (&bp2->lock);

  return (ret);
}

