/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity 
 * pertaining to distribution of the software without specific, written
 * prior permission.
 * 
 * CARNEGIE MELLON, IBM, AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON, IBM, AND CSL DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

#ifndef _I386_GDT_
#define _I386_GDT_

#include "seg.h"

/*
 * Kernel descriptors for Mach - 32-bit flat address space.
 */
#define	KERNEL_CS	(0x08 | KERNEL_RING)		/* kernel code */
#define	KERNEL_DS	(0x10 | KERNEL_RING)		/* kernel data */


#ifndef	MACH_PV_DESCRIPTORS
#define	KERNEL_LDT	0x18		/* master LDT */
#endif	/* MACH_PV_DESCRIPTORS */

#ifdef __x86_64__
/* LDT needs two entries */
#define	KERNEL_TSS	0x40		/* master TSS (uniprocessor) */
#else
#define	KERNEL_TSS	0x20		/* master TSS (uniprocessor) */
#endif


#define	USER_LDT	0x28		/* place for per-thread LDT */

#ifdef __x86_64__
/* LDT needs two entries */
#define	USER_TSS	0x58		/* place for per-thread TSS
					   that holds IO bitmap */
#else
#define	USER_TSS	0x30		/* place for per-thread TSS
					   that holds IO bitmap */
#endif


#ifndef	MACH_PV_DESCRIPTORS
#define	LINEAR_DS	0x38		/* linear mapping */
#endif	/* MACH_PV_DESCRIPTORS */

/*			0x40		   was USER_FPREGS, now used by TSS in 64bit mode */

#define	USER_GDT	0x48		/* user-defined 32bit GDT entries */
#define	USER_GDT_SLOTS	2

/*			0x58		   used by user TSS in 64bit mode */

#define PERCPU_DS	0x68		/* per-cpu data mapping */

#define	GDTSZ		sel_idx(0x70)

#ifndef __ASSEMBLER__

extern struct real_descriptor gdt[GDTSZ];

/* Fill a segment descriptor in the GDT.  */
#define _fill_gdt_descriptor(_gdt, segment, base, limit, access, sizebits) \
	fill_descriptor(&_gdt[sel_idx(segment)], base, limit, access, sizebits)

#define fill_gdt_descriptor(segment, base, limit, access, sizebits) \
	_fill_gdt_descriptor(gdt, segment, base, limit, access, sizebits)

/* 64bit variant */
#ifdef __x86_64__
#define _fill_gdt_descriptor64(_gdt, segment, base, limit, access, sizebits) \
	fill_descriptor64((struct real_descriptor64 *) &_gdt[sel_idx(segment)], base, limit, access, sizebits)

#define fill_gdt_descriptor64(segment, base, limit, access, sizebits) \
	_fill_gdt_descriptor64(gdt, segment, base, limit, access, sizebits)
#endif

/* System descriptor variants */
#ifdef __x86_64__
#define _fill_gdt_sys_descriptor(_gdt, segment, base, limit, access, sizebits) \
	_fill_gdt_descriptor64(_gdt, segment, base, limit, access, sizebits)
#define fill_gdt_sys_descriptor(segment, base, limit, access, sizebits) \
	fill_gdt_descriptor64(segment, base, limit, access, sizebits)
#else
#define _fill_gdt_sys_descriptor(_gdt, segment, base, limit, access, sizebits) \
	_fill_gdt_descriptor(_gdt, segment, base, limit, access, sizebits)
#define fill_gdt_sys_descriptor(segment, base, limit, access, sizebits) \
	fill_gdt_descriptor(segment, base, limit, access, sizebits)
#endif

extern void gdt_init(void);
extern void ap_gdt_init(int cpu);

#endif /* __ASSEMBLER__ */
#endif /* _I386_GDT_ */
