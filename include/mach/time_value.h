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

#ifndef	_MACH_TIME_VALUE_H_
#define	_MACH_TIME_VALUE_H_

#include <mach/machine/vm_types.h>

/*
 *	Time value returned by kernel.
 */

struct rpc_time_value {
	/* TODO: this should be 64 bits regardless of the arch to be Y2038 proof. */
	rpc_long_integer_t seconds;
	integer_t microseconds;
};
typedef struct rpc_time_value rpc_time_value_t;

/*
 *	Time value used by kernel interfaces. Ideally they should be migrated
 *	to use time_value64 below.
 */
struct time_value {
	long_integer_t	seconds;
	integer_t	microseconds;
};
typedef	struct time_value	time_value_t;

/*
 * Time value used internally by the kernel that uses 64 bits to track seconds
 * and nanoseconds. Note that the current resolution is only microseconds.
 */
struct time_value64 {
	int64_t seconds;
	int64_t nanoseconds;
};
typedef struct time_value64 time_value64_t;

/**
 * Functions used by Mig to perform user to kernel conversion and vice-versa.
 * We only do this because we may run a 64 bit kernel with a 32 bit user space.
 */
static __inline__ rpc_time_value_t convert_time_value_to_user(time_value_t tv)
{
	rpc_time_value_t user = {.seconds = tv.seconds, .microseconds = tv.microseconds};
	return user;
}
static __inline__ time_value_t convert_time_value_from_user(rpc_time_value_t tv)
{
	time_value_t kernel = {.seconds = tv.seconds, .microseconds = tv.microseconds};
	return kernel;
}

/*
 *	Macros to manipulate time values.  Assume that time values
 *	are normalized (microseconds <= 999999).
 */
#define	TIME_MICROS_MAX	(1000000)
#define	TIME_NANOS_MAX	(1000000000)

#define time_value_assert(val)			\
  assert(0 <= (val)->microseconds && (val)->microseconds < TIME_MICROS_MAX);

#define time_value64_assert(val)			\
  assert(0 <= (val)->nanoseconds && (val)->nanoseconds < TIME_NANOS_MAX);

#define	time_value_add_usec(val, micros)	{	\
	time_value_assert(val);				\
	if (((val)->microseconds += (micros))		\
		>= TIME_MICROS_MAX) {			\
	    (val)->microseconds -= TIME_MICROS_MAX;	\
	    (val)->seconds++;				\
	}						\
	time_value_assert(val);				\
}

#define	time_value64_add_usec(val, micros)	{	\
	time_value64_assert(val);				\
	if (((val)->nanoseconds += (micros) * 1000)		\
		>= TIME_NANOS_MAX) {			\
	    (val)->nanoseconds -= TIME_NANOS_MAX;	\
	    (val)->seconds++;				\
	}						\
	time_value64_assert(val);				\
}

#define	time_value_sub_usec(val, micros)	{	\
	time_value_assert(val);				\
	if (((val)->microseconds -= (micros)) < 0) {	\
	    (val)->microseconds += TIME_MICROS_MAX;	\
	    (val)->seconds--;				\
	}						\
	time_value_assert(val);				\
}

#define	time_value_add(result, addend) {			\
    time_value_assert(addend);					\
    (result)->seconds += (addend)->seconds;			\
    time_value_add_usec(result, (addend)->microseconds);	\
  }

#define	time_value_sub(result, subtrahend) {			\
    time_value_assert(subtrahend);				\
    (result)->seconds -= (subtrahend)->seconds;			\
    time_value_sub_usec(result, (subtrahend)->microseconds);	\
  }

/*
 *	Time value available through the mapped-time interface.
 *	Read this mapped value with
 *		do {
 *			secs = mtime->seconds;
 *			__sync_synchronize();
 *			usecs = mtime->microseconds;
 *			__sync_synchronize();
 *		} while (secs != mtime->check_seconds);
 */

typedef struct mapped_time_value {
	integer_t seconds;
	integer_t microseconds;
	integer_t check_seconds;
} mapped_time_value_t;

/* Macros for converting between struct timespec and time_value_t. */

#define TIME_VALUE_TO_TIMESPEC(tv, ts) do {                             \
        (ts)->tv_sec = (tv)->seconds;                                   \
        (ts)->tv_nsec = (tv)->microseconds * 1000;                      \
} while(0)

#define TIMESPEC_TO_TIME_VALUE(tv, ts) do {                             \
        (tv)->seconds = (ts)->tv_sec;                                   \
        (tv)->microseconds = (ts)->tv_nsec / 1000;                      \
} while(0)

#endif	/* _MACH_TIME_VALUE_H_ */
