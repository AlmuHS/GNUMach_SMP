/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
/*
 */

#include <i386/i386/loose_ends.h>

#ifndef NDEBUG
#define MACH_ASSERT 1
#else
#define MACH_ASSERT 0
#endif /* NDEBUG */

	/*
	 * For now we will always go to single user mode, since there is
	 * no way pass this request through the boot.
	 */

/* Someone with time should write code to set cpuspeed automagically */
int cpuspeed = 4;
#define	DELAY(n)	{ volatile int N = cpuspeed * (n); while (--N > 0); }
void
delay(int n)
{
	DELAY(n);
}
