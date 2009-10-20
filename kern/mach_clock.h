/*
 * Copyright (C) 2006, 2007 Free Software Foundation, Inc.
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
 * Author: Barry deFreese and others.
 */

#ifndef _KERN_MACH_CLOCK_H_
#define _KERN_MACH_CLOCK_H_

/*
 * Mach time-out and time-of-day facility.
 */

#include <mach/machine/kern_return.h>
#include <mach/time_value.h>
#include <kern/host.h>
#include <kern/queue.h>


/* Timers in kernel.  */
extern unsigned long	elapsed_ticks;	/* number of ticks elapsed since bootup */
extern int		hz;		/* number of ticks per second */
extern int		tick;		/* number of usec per tick */


/* Time-out element.  */
struct timer_elt {
	queue_chain_t	chain;		/* chain in order of expiration */
	void		(*fcn)();	/* function to call */
	void *		param;		/* with this parameter */
	unsigned long	ticks;		/* expiration time, in ticks */
	int		set;		/* unset | set | allocated */
};
#define	TELT_UNSET	0		/* timer not set */
#define	TELT_SET	1		/* timer set */
#define	TELT_ALLOC	2		/* timer allocated from pool */

typedef	struct timer_elt	timer_elt_data_t;
typedef	struct timer_elt	*timer_elt_t;


extern void clock_interrupt(
   int usec,
   boolean_t usermode,
   boolean_t basepri);

extern void softclock (void);

/* For `private' timer elements.  */
extern void set_timeout(
   timer_elt_t telt,
   unsigned int interval);
extern boolean_t reset_timeout(timer_elt_t telt);

#define	set_timeout_setup(telt,fcn,param,interval)	\
	((telt)->fcn = (fcn),				\
	 (telt)->param = (param),			\
	 (telt)->private = TRUE,			\
	set_timeout((telt), (interval)))

#define	reset_timeout_check(t)				\
	MACRO_BEGIN					\
	if ((t)->set)					\
	    reset_timeout((t));				\
	MACRO_END

extern void init_timeout (void);

/* Read the current time into STAMP.  */
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

extern void mapable_time_init (void);

/* For public timer elements.  */
extern void timeout(void (*fcn)(void *), void *param, int interval);
extern boolean_t untimeout(void (*fcn)(void *), void *param);

#endif /* _KERN_MACH_CLOCK_H_ */
