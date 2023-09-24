/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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

#ifndef _KERN_CPU_NUMBER_H_
#define _KERN_CPU_NUMBER_H_

#include <machine/cpu_number.h>

/*
 *	Definitions for cpu identification in multi-processors.
 */

extern int	master_cpu;	/* 'master' processor - keeps time */

#if	(NCPUS == 1)
	/* cpu number is always 0 on a single processor system */
#define	cpu_number()		(0)
#define	cpu_number_slow()	(0)

#endif	/* NCPUS == 1 */

#define CPU_L1_SIZE (1 << CPU_L1_SHIFT)

#endif /* _KERN_CPU_NUMBER_H_ */
