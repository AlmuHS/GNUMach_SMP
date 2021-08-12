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

#ifndef	_KERN_ASSERT_H_
#define	_KERN_ASSERT_H_

/*	assert.h	4.2	85/01/21	*/

#include <kern/macros.h>

#ifndef NDEBUG
#define MACH_ASSERT 1
#endif

#if	MACH_ASSERT
extern void Assert(const char *exp, const char *filename, int line,
		   const char *fun) __attribute__ ((noreturn));

#define assert(ex)							\
	(likely(ex)							\
	 ? (void) (0)							\
	 : Assert (#ex, __FILE__, __LINE__, __FUNCTION__))

#define	assert_static(x)	assert(x)

#else	/* MACH_ASSERT */
#define assert(ex)
#define assert_static(ex)
#endif	/* MACH_ASSERT */

#endif	/* _KERN_ASSERT_H_ */
