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

#ifndef _KERN_KMUTEX_H_
#define _KERN_KMUTEX_H_   1

#include <kern/lock.h>
#include <mach/kern_return.h>

struct kmutex
{
  unsigned int state;
  decl_simple_lock_data (, lock)
};

/* Possible values for the mutex state. */
#define KMUTEX_AVAIL       0
#define KMUTEX_LOCKED      1
#define KMUTEX_CONTENDED   2

/* Initialize mutex in *MTXP. */
extern void kmutex_init (struct kmutex *mtxp);

/* Acquire lock MTXP. If INTERRUPTIBLE is true, the sleep may be
 * prematurely terminated, in which case the function returns
 * KERN_INTERRUPTED. Otherwise, KERN_SUCCESS is returned. */
extern kern_return_t kmutex_lock (struct kmutex *mtxp,
  boolean_t interruptible);

/* Try to acquire the lock MTXP without sleeping.
 * Returns KERN_SUCCESS if successful, KERN_FAILURE otherwise. */
extern kern_return_t kmutex_trylock (struct kmutex *mtxp);

/* Unlock the mutex MTXP. */
extern void kmutex_unlock (struct kmutex *mtxp);

#endif
