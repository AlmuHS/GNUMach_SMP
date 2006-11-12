/*
 *
 * Copyright (C) 2006 Free Software Foundation, Inc.
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
 * Author: Barry deFreese.
 */
/*
 *
 */

#ifndef _KERN_MACH_CLOCK_H_
#define _KERN_MACH_CLOCK_H_

#include <sys/types.h>
#include <kern/time_out.h>
#include <mach/machine/kern_return.h>

extern void clock_interrupt(
   int usec,
   boolean_t usermode,
   boolean_t basepri);

extern void softclock();

extern void set_timeout(
   timer_elt_t telt,
   unsigned int interval);

extern boolean_t reset_timeout(timer_elt_t telt);

extern void init_timeout();

extern void record_time_stamp (time_value_t *stamp);

extern kern_return_t host_get_time(
   host_t host,
   time_value_t *current_time);

extern kern_return_t host_set_time(
   host_t host,
   time_value_t new_time);

extern kern_return_t host_adjust_time(
   host_t host,
   time_value_t new_adjustment,
   time_value_t *old_adjustment);

extern void mapable_time_init();

extern void timeout(int (*fcn)(), char *param, int interval);

extern boolean_t untimeout(int (*fcn)(), char *param);

#endif /* _KERN_MACH_CLOCK_H_ */
