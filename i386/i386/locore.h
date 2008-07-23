/*
 * Header file for printf type functions.
 * Copyright (C) 2006 Free Software Foundation.
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
 */

#ifndef _MACHINE_LOCORE_H_
#define _MACHINE_LOCORE_H_

#include <sys/types.h>

#include <kern/sched_prim.h>

extern int copyin (const void *userbuf, void *kernelbuf, size_t cn);

extern int copyinmsg (const void *userbuf, void *kernelbuf, size_t cn);

extern int copyout (const void *kernelbuf, void *userbuf, size_t cn);

extern int copyoutmsg (const void *kernelbuf, void *userbuf, size_t cn);

extern int call_continuation (continuation_t continuation);

extern unsigned int cpu_features[1];

#define CPU_FEATURE_FPU		 0
#define CPU_FEATURE_VME		 1
#define CPU_FEATURE_DE		 2
#define CPU_FEATURE_PSE		 3
#define CPU_FEATURE_TSC		 4
#define CPU_FEATURE_MSR		 5
#define CPU_FEATURE_PAE		 6
#define CPU_FEATURE_MCE		 7
#define CPU_FEATURE_CX8		 8
#define CPU_FEATURE_APIC	 9
#define CPU_FEATURE_SEP		11
#define CPU_FEATURE_MTRR	12
#define CPU_FEATURE_PGE		13
#define CPU_FEATURE_MCA		14
#define CPU_FEATURE_CMOV	15
#define CPU_FEATURE_PAT		16
#define CPU_FEATURE_PSE_36	17
#define CPU_FEATURE_PSN		18
#define CPU_FEATURE_CFLSH	19
#define CPU_FEATURE_DS		21
#define CPU_FEATURE_ACPI	22
#define CPU_FEATURE_MMX		23
#define CPU_FEATURE_FXSR	24
#define CPU_FEATURE_SSE		25
#define CPU_FEATURE_SSE2	26
#define CPU_FEATURE_SS		27
#define CPU_FEATURE_HTT		28
#define CPU_FEATURE_TM		29
#define CPU_FEATURE_PBE		31

#define CPU_HAS_FEATURE(feature) (cpu_features[(feature) / 32] & (1 << ((feature) % 32)))

#endif /* _MACHINE__LOCORE_H_ */

