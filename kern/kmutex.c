/* Copyright (C) 2017 Free Software Foundation, Inc.
   Contributed by Agustina Arzille <avarzille@riseup.net>, 2017.

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

#include <kern/kmutex.h>
#include <kern/atomic.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>

void kmutex_init (struct kmutex *mtxp)
{
  mtxp->state = KMUTEX_AVAIL;
  simple_lock_init (&mtxp->lock);
}

kern_return_t kmutex_lock (struct kmutex *mtxp, boolean_t interruptible)
{
  check_simple_locks ();

  if (atomic_cas_acq (&mtxp->state, KMUTEX_AVAIL, KMUTEX_LOCKED))
    /* Unowned mutex - We're done. */
    return (KERN_SUCCESS);

  /* The mutex is locked. We may have to sleep. */
  simple_lock (&mtxp->lock);
  if (atomic_swap_acq (&mtxp->state, KMUTEX_CONTENDED) == KMUTEX_AVAIL)
    {
      /* The mutex was released in-between. */
      simple_unlock (&mtxp->lock);
      return (KERN_SUCCESS);
    }

  /* Sleep and check the result value of the waiting, in order to
   * inform our caller if we were interrupted or not. Note that
   * we don't need to set again the mutex state. The owner will
   * handle that in every case. */
  thread_sleep ((event_t)mtxp, (simple_lock_t)&mtxp->lock, interruptible);
  return (current_thread()->wait_result == THREAD_AWAKENED ?
    KERN_SUCCESS : KERN_INTERRUPTED);
}

kern_return_t kmutex_trylock (struct kmutex *mtxp)
{
  return (atomic_cas_acq (&mtxp->state, KMUTEX_AVAIL, KMUTEX_LOCKED) ?
    KERN_SUCCESS : KERN_FAILURE);
}

void kmutex_unlock (struct kmutex *mtxp)
{
  if (atomic_cas_rel (&mtxp->state, KMUTEX_LOCKED, KMUTEX_AVAIL))
    /* No waiters - We're done. */
    return;

  simple_lock (&mtxp->lock);

  if (!thread_wakeup_one ((event_t)mtxp))
    /* Any threads that were waiting on this mutex were
     * interrupted and left - Reset the mutex state. */
    mtxp->state = KMUTEX_AVAIL;

  simple_unlock (&mtxp->lock);
}
