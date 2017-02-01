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

#ifndef _KERN_ATOMIC_H_
#define _KERN_ATOMIC_H_   1

/* Atomically compare *PTR with EXP and set it to NVAL if they're equal.
 * Evaluates to a boolean, indicating whether the comparison was successful.*/
#define __atomic_cas_helper(ptr, exp, nval, mo)   \
  ({   \
     typeof(exp) __e = (exp);   \
     __atomic_compare_exchange_n ((ptr), &__e, (nval), 0,   \
       __ATOMIC_##mo, __ATOMIC_RELAXED);   \
   })

#define atomic_cas_acq(ptr, exp, nval)   \
  __atomic_cas_helper (ptr, exp, nval, ACQUIRE)

#define atomic_cas_rel(ptr, exp, nval)   \
  __atomic_cas_helper (ptr, exp, nval, RELEASE)

#define atomic_cas_seq(ptr, exp, nval)   \
  __atomic_cas_helper (ptr, exp, nval, SEQ_CST)

/* Atomically exchange the value of *PTR with VAL, evaluating to
 * its previous value. */
#define __atomic_swap_helper(ptr, val, mo)   \
  __atomic_exchange_n ((ptr), (val), __ATOMIC_##mo)

#define atomic_swap_acq(ptr, val)   \
  __atomic_swap_helper (ptr, val, ACQUIRE)

#define atomic_swap_rel(ptr, val)   \
  __atomic_swap_helper (ptr, val, RELEASE)

#define atomic_swap_seq(ptr, val)   \
  __atomic_swap_helper (ptr, val, SEQ_CST)

#endif
