/*
 * Linux scheduling support.
 *
 * Copyright (C) 1996 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *      Author: Shantanu Goel, University of Utah CSL
 */

/*
 *  linux/kernel/sched.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <sys/types.h>
#include <machine/spl.h>

#include <mach/boolean.h>

#include <kern/thread.h>
#include <kern/sched_prim.h>

#define MACH_INCLUDE
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/atomic.h>

int securelevel = 0;

extern void *alloc_contig_mem (unsigned, unsigned, unsigned, vm_page_t *);
extern void free_contig_mem (vm_page_t);
extern spl_t splhigh (void);
extern spl_t splx (spl_t);
extern void linux_soft_intr (void);
extern int issig (void);
extern int printf (const char *, ...);
extern int linux_auto_config;

static void timer_bh (void);

DECLARE_TASK_QUEUE (tq_timer);
DECLARE_TASK_QUEUE (tq_immediate);
DECLARE_TASK_QUEUE (tq_scheduler);

static struct wait_queue **auto_config_queue;

static inline void
handle_soft_intr (void)
{
  if (bh_active & bh_mask)
    {
      intr_count = 1;
      linux_soft_intr ();
      intr_count = 0;
    }
}

static void
tqueue_bh (void)
{
  run_task_queue(&tq_timer);
}

static void
immediate_bh (void)
{
  run_task_queue (&tq_immediate);
}

void
add_wait_queue (struct wait_queue **q, struct wait_queue *wait)
{
  unsigned long flags;

  if (! linux_auto_config)
    {
      save_flags (flags);
      cli ();
      assert_wait ((event_t) q, FALSE);
      restore_flags (flags);
      return;
    }

  if (auto_config_queue)
    printf ("add_wait_queue: queue not empty\n");
  auto_config_queue = q;
}

void
remove_wait_queue (struct wait_queue **q, struct wait_queue *wait)
{
  unsigned long flags;

  if (! linux_auto_config)
    {
      save_flags (flags);
      thread_wakeup ((event_t) q);
      restore_flags (flags);
      return;
    }

  auto_config_queue = NULL;
}

static inline int
waking_non_zero (struct semaphore *sem)
{
  int ret;
  unsigned long flags;

  get_buzz_lock (&sem->lock);
  save_flags (flags);
  cli ();

  if ((ret = (sem->waking > 0)))
    sem->waking--;

  restore_flags (flags);
  give_buzz_lock (&sem->lock);
  return ret;
}

void
__up (struct semaphore *sem)
{
  atomic_inc (&sem->waking);
  wake_up (&sem->wait);
}

int
__do_down (struct semaphore *sem, int task_state)
{
  unsigned long flags;
  int ret = 0;
  int s;
  
  if (!linux_auto_config)
    {
      save_flags (flags);
      s = splhigh ();
      for (;;)
	{
	  if (waking_non_zero (sem))
	    break;

	  if (task_state == TASK_INTERRUPTIBLE && issig ())
	    {
	      ret = -LINUX_EINTR;
	      atomic_inc (&sem->count);
	      break;
	    }

	  assert_wait ((event_t) &sem->wait,
		       task_state == TASK_INTERRUPTIBLE ? TRUE : FALSE);
	  splx (s);
	  schedule ();
	  s = splhigh ();
	}
      splx (s);
      restore_flags (flags);
      return ret;
    }

  while (!waking_non_zero (sem))
    {
      if (task_state == TASK_INTERRUPTIBLE && issig ())
	{
	  ret = -LINUX_EINTR;
	  atomic_inc (&sem->count);
	  break;
	}
      schedule ();
    }

  return ret;
}

void
__down (struct semaphore *sem)
{
  __do_down(sem, TASK_UNINTERRUPTIBLE);
}

int
__down_interruptible (struct semaphore *sem)
{
  return __do_down (sem, TASK_INTERRUPTIBLE); 
}

void
__sleep_on (struct wait_queue **q, int state)
{
  unsigned long flags;

  if (!q)
    return;
  save_flags (flags);
  if (!linux_auto_config)
    {
      assert_wait ((event_t) q, state == TASK_INTERRUPTIBLE ? TRUE : FALSE);
      sti ();
      schedule ();
      restore_flags (flags);
      return;
    }
  
  add_wait_queue (q, NULL);
  sti ();
  while (auto_config_queue)
    schedule ();
  restore_flags (flags);
}

void
sleep_on (struct wait_queue **q)
{
  __sleep_on (q, TASK_UNINTERRUPTIBLE);
}

void
interruptible_sleep_on (struct wait_queue **q)
{
  __sleep_on (q, TASK_INTERRUPTIBLE);
}

void
wake_up (struct wait_queue **q)
{
  unsigned long flags;

  if (! linux_auto_config)
    {
      if (q != &wait_for_request)      /* ??? by OKUJI Yoshinori. */
	{
	  save_flags (flags);
	  thread_wakeup ((event_t) q);
	  restore_flags (flags);
	}
      return;
    }

  if (auto_config_queue == q)
    auto_config_queue = NULL;
}

void
__wait_on_buffer (struct buffer_head *bh)
{
  unsigned long flags;

  save_flags (flags);
  if (! linux_auto_config)
    {
      while (1)
	{
	  cli ();
	  run_task_queue (&tq_disk);
	  if (! buffer_locked (bh))
	    break;
	  bh->b_wait = (struct wait_queue *) 1;
	  assert_wait ((event_t) bh, FALSE);
	  sti ();
	  schedule ();
	}
      restore_flags (flags);
      return;
    }

  sti ();
  while (buffer_locked (bh))
    {
      run_task_queue (&tq_disk);
      schedule ();
    }
  restore_flags (flags);
}

void
unlock_buffer (struct buffer_head *bh)
{
  unsigned long flags;

  save_flags (flags);
  cli ();
  clear_bit (BH_Lock, &bh->b_state);
  if (bh->b_wait && ! linux_auto_config)
    {
      bh->b_wait = NULL;
      thread_wakeup ((event_t) bh);
    }
  restore_flags (flags);
}

void
schedule (void)
{
  if (intr_count)
    printk ("Aiee: scheduling in interrupt %p\n",
	    __builtin_return_address (0));
  
  handle_soft_intr ();
  run_task_queue (&tq_scheduler);

  if (!linux_auto_config)
    thread_block (0);
}

void
cdrom_sleep (int t)
{
  int xxx;

  assert_wait ((event_t) &xxx, TRUE);
  thread_set_timeout (t);
  schedule ();
}

void
linux_sched_init (void)
{
  /*
   * Install software interrupt handlers.
   */
  init_bh (TIMER_BH, timer_bh);
  init_bh (TQUEUE_BH, tqueue_bh);
  init_bh (IMMEDIATE_BH, immediate_bh);
}

/*
 * Linux timers.
 *
 * Copyright (C) 1996 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *      Author: Shantanu Goel, University of Utah CSL
 */

unsigned long volatile jiffies = 0;

/*
 * Mask of active timers.
 */
unsigned long timer_active = 0;

/*
 * List of timeout routines.
 */
struct timer_struct timer_table[32];

#define TVN_BITS 6
#define TVR_BITS 8
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)

#define SLOW_BUT_DEBUGGING_TIMERS 0

struct timer_vec
  {
    int index;
    struct timer_list *vec[TVN_SIZE];
  };

struct timer_vec_root
  {
    int index;
    struct timer_list *vec[TVR_SIZE];
  };

static struct timer_vec tv5 =
{0};
static struct timer_vec tv4 =
{0};
static struct timer_vec tv3 =
{0};
static struct timer_vec tv2 =
{0};
static struct timer_vec_root tv1 =
{0};

static struct timer_vec *const tvecs[] =
{
  (struct timer_vec *) &tv1, &tv2, &tv3, &tv4, &tv5
};

#define NOOF_TVECS (sizeof(tvecs) / sizeof(tvecs[0]))

static unsigned long timer_jiffies = 0;

static inline void
insert_timer (struct timer_list *timer, struct timer_list **vec, int idx)
{
  if ((timer->next = vec[idx]))
    vec[idx]->prev = timer;
  vec[idx] = timer;
  timer->prev = (struct timer_list *) &vec[idx];
}

static inline void
internal_add_timer (struct timer_list *timer)
{
  /*
   * must be cli-ed when calling this
   */
  unsigned long expires = timer->expires;
  unsigned long idx = expires - timer_jiffies;

  if (idx < TVR_SIZE)
    {
      int i = expires & TVR_MASK;
      insert_timer (timer, tv1.vec, i);
    }
  else if (idx < 1 << (TVR_BITS + TVN_BITS))
    {
      int i = (expires >> TVR_BITS) & TVN_MASK;
      insert_timer (timer, tv2.vec, i);
    }
  else if (idx < 1 << (TVR_BITS + 2 * TVN_BITS))
    {
      int i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
      insert_timer (timer, tv3.vec, i);
    }
  else if (idx < 1 << (TVR_BITS + 3 * TVN_BITS))
    {
      int i = (expires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
      insert_timer (timer, tv4.vec, i);
    }
  else if (expires < timer_jiffies)
    {
      /* can happen if you add a timer with expires == jiffies,
       * or you set a timer to go off in the past
       */
      insert_timer (timer, tv1.vec, tv1.index);
    }
  else if (idx < 0xffffffffUL)
    {
      int i = (expires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
      insert_timer (timer, tv5.vec, i);
    }
  else
    {
      /* Can only get here on architectures with 64-bit jiffies */
      timer->next = timer->prev = timer;
    }
}

void
add_timer (struct timer_list *timer)
{
  unsigned long flags;

  save_flags (flags);
  cli ();
#if SLOW_BUT_DEBUGGING_TIMERS
  if (timer->next || timer->prev)
    {
      printk ("add_timer() called with non-zero list from %p\n",
	      __builtin_return_address (0));
      goto out;
    }
#endif
  internal_add_timer (timer);
#if SLOW_BUT_DEBUGGING_TIMERS
out:
#endif
  restore_flags (flags);
}

static inline int
detach_timer (struct timer_list *timer)
{
  int ret = 0;
  struct timer_list *next, *prev;

  next = timer->next;
  prev = timer->prev;
  if (next)
    {
      next->prev = prev;
    }
  if (prev)
    {
      ret = 1;
      prev->next = next;
    }
  return ret;
}

int
del_timer (struct timer_list *timer)
{
  int ret;
  unsigned long flags;

  save_flags (flags);
  cli ();
  ret = detach_timer (timer);
  timer->next = timer->prev = 0;
  restore_flags (flags);
  return ret;
}

static inline void
run_old_timers (void)
{
  struct timer_struct *tp;
  unsigned long mask;

  for (mask = 1, tp = timer_table + 0; mask; tp++, mask += mask)
    {
      if (mask > timer_active)
	break;
      if (!(mask & timer_active))
	continue;
      if (tp->expires > jiffies)
	continue;
      timer_active &= ~mask;
      tp->fn ();
      sti ();
    }
}

static inline void
cascade_timers (struct timer_vec *tv)
{
  /* cascade all the timers from tv up one level */
  struct timer_list *timer;

  timer = tv->vec[tv->index];
  /*
   * We are removing _all_ timers from the list, so we don't  have to
   * detach them individually, just clear the list afterwards.
   */
  while (timer)
    {
      struct timer_list *tmp = timer;
      timer = timer->next;
      internal_add_timer (tmp);
    }
  tv->vec[tv->index] = NULL;
  tv->index = (tv->index + 1) & TVN_MASK;
}

static inline void
run_timer_list (void)
{
  cli ();
  while ((long) (jiffies - timer_jiffies) >= 0)
    {
      struct timer_list *timer;

      if (!tv1.index)
	{
	  int n = 1;

	  do
	    {
	      cascade_timers (tvecs[n]);
	    }
	  while (tvecs[n]->index == 1 && ++n < NOOF_TVECS);
	}
      while ((timer = tv1.vec[tv1.index]))
	{
	  void (*fn) (unsigned long) = timer->function;
	  unsigned long data = timer->data;

	  detach_timer (timer);
	  timer->next = timer->prev = NULL;
	  sti ();
	  fn (data);
	  cli ();
	}
      ++timer_jiffies;
      tv1.index = (tv1.index + 1) & TVR_MASK;
    }
  sti ();
}

/*
 * Timer software interrupt handler.
 */
static void
timer_bh (void)
{
  run_old_timers ();
  run_timer_list ();
}

#if 0
int linux_timer_print = 0;
#endif

/*
 * Timer interrupt handler.
 */
void
linux_timer_intr (void)
{
  (*(unsigned long *) &jiffies)++;
  mark_bh (TIMER_BH);
  if (tq_timer)
    mark_bh (TQUEUE_BH);
#if 0
  if (linux_timer_print)
    printf ("linux_timer_intr: pic_mask[0] %x\n", pic_mask[0]);
#endif
}
