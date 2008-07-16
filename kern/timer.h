/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef	_KERN_TIMER_H_
#define _KERN_TIMER_H_

#include <kern/macro_help.h>

#if	STAT_TIME
/*
 *	Statistical timer definitions - use microseconds in timer, seconds
 *	in high unit field.  No adjustment needed to convert to time_value_t
 *	as a result.  Service timers once an hour.
 */

/*
 *	TIMER_MAX is needed if a 32-bit rollover timer needs to be adjusted for
 *	maximum value.
 */
#undef TIMER_MAX

/*
 *	TIMER_RATE is the rate of the timer in ticks per second.  It is used to
 *	calculate percent cpu usage.
 */
#define TIMER_RATE	1000000

/*
 *	TIMER_HIGH_UNIT is the unit for high_bits in terms of low_bits.
 *	Setting it to TIMER_RATE makes the high unit seconds.
 */
#define TIMER_HIGH_UNIT	TIMER_RATE

/*
 *	TIMER_ADJUST is used to adjust the value of a timer after it has been
 *	copied into a time_value_t.  No adjustment is needed if high_bits is in
 *	seconds.
 */
#undef	TIMER_ADJUST

/*
 *	MACHINE_TIMER_ROUTINES should defined if the timer routines are
 *	implemented in machine-dependent code (e.g. assembly language).
 */
#undef	MACHINE_TIMER_ROUTINES

#else	/* STAT_TIME */
/*
 *	Machine dependent definitions based on hardware support.
 */

#include <machine/timer.h>

#endif	/* STAT_TIME */

/*
 *	Definitions for accurate timers.  high_bits_check is a copy of
 *	high_bits that allows reader to verify that values read are ok.
 */

struct timer {
	unsigned	low_bits;
	unsigned	high_bits;
	unsigned	high_bits_check;
	unsigned	tstamp;
};

typedef struct timer		timer_data_t;
typedef	struct timer		*timer_t;

/*
 *	Mask to check if low_bits is in danger of overflowing
 */

#define	TIMER_LOW_FULL	0x80000000U

/*
 *	Kernel timers and current timer array.  [Exported]
 */

extern timer_t		current_timer[NCPUS];
extern timer_data_t	kernel_timer[NCPUS];

/*
 *	save structure for timer readings.  This is used to save timer
 *	readings for elapsed time computations.
 */

struct timer_save {
	unsigned	low;
	unsigned	high;
};

typedef struct timer_save	timer_save_data_t, *timer_save_t;

/*
 *	Exported kernel interface to timers
 */

#if	STAT_TIME
#define start_timer(timer)
#define timer_switch(timer)
#else	/* STAT_TIME */
extern void	start_timer(timer_t);
extern void	timer_switch(timer_t);
#endif	/* STAT_TIME */

extern void		timer_read(timer_t, time_value_t *);
extern void		thread_read_times(thread_t, time_value_t *, time_value_t *);
extern unsigned		timer_delta(timer_t, timer_save_t);
extern void		timer_normalize(timer_t);
extern void		timer_init(timer_t);

#if	STAT_TIME
/*
 *	Macro to bump timer values.
 */
#define timer_bump(timer, usec)					\
MACRO_BEGIN							\
	(timer)->low_bits += usec;				\
	if ((timer)->low_bits & TIMER_LOW_FULL) {		\
		timer_normalize(timer);				\
	}							\
MACRO_END

#else	/* STAT_TIME */
/*
 *	Exported hardware interface to timers
 */
extern void	time_trap_uentry(unsigned);
extern void	time_trap_uexit(int);
extern timer_t	time_int_entry(unsigned, timer_t);
extern void	time_int_exit(unsigned, timer_t);
#endif	/* STAT_TIME */

/*
 *	TIMER_DELTA finds the difference between a timer and a saved value,
 *	and updates the saved value.  Look at high_bits check field after
 *	reading low because that's the first written by a normalize
 *	operation; this isn't necessary for current usage because
 *	this macro is only used when the timer can't be normalized:
 *	thread is not running, or running thread calls it on itself at
 *	splsched().
 */

#define TIMER_DELTA(timer, save, result)			\
MACRO_BEGIN							\
	register unsigned	temp;				\
								\
	temp = (timer).low_bits;				\
	if ((save).high != (timer).high_bits_check) {		\
		result += timer_delta(&(timer), &(save));	\
	}							\
	else {							\
		result += temp - (save).low;			\
		(save).low = temp;				\
	}							\
MACRO_END

extern void init_timers(void);

#endif	/* _KERN_TIMER_H_ */
