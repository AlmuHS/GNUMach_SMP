/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 *	Machine-dependent definitions for cpu identification.
 *
 */
#ifndef	_I386_CPU_NUMBER_H_
#define	_I386_CPU_NUMBER_H_

#if	NCPUS > 1

#define MY(stm)		%gs:PERCPU_##stm

#ifdef __i386__
#define	CX(addr, reg)	addr(,reg,4)
#endif
#ifdef __x86_64__
#define	CX(addr, reg)	addr(,reg,8)
#endif

#define	CPU_NUMBER_NO_STACK(reg)	\
	movl	%cs:lapic, reg		;\
	movl	%cs:APIC_ID(reg), reg	;\
	shrl	$24, reg		;\
	movl	%cs:CX(cpu_id_lut, reg), reg	;\

#ifdef __i386__
/* Never call CPU_NUMBER_NO_GS(%esi) */
#define CPU_NUMBER_NO_GS(reg)		\
	pushl	%esi		;\
	pushl	%eax		;\
	pushl	%ebx		;\
	pushl	%ecx		;\
	pushl	%edx		;\
	movl	$1, %eax	;\
	cpuid			;\
	shrl	$24, %ebx	;\
	movl	%cs:CX(cpu_id_lut, %ebx), %esi	;\
	popl	%edx		;\
	popl	%ecx		;\
	popl	%ebx		;\
	popl	%eax		;\
	movl	%esi, reg	;\
	popl	%esi
#endif
#ifdef __x86_64__
/* Never call CPU_NUMBER_NO_GS(%esi) */
#define CPU_NUMBER_NO_GS(reg)		\
	pushq	%rsi		;\
	pushq	%rax		;\
	pushq	%rbx		;\
	pushq	%rcx		;\
	pushq	%rdx		;\
	movl	$1, %eax	;\
	cpuid			;\
	shrl	$24, %ebx	;\
	movl	%cs:CX(cpu_id_lut, %ebx), %esi	;\
	popq	%rdx		;\
	popq	%rcx		;\
	popq	%rbx		;\
	popq	%rax		;\
	movl	%esi, reg	;\
	popq	%rsi
#endif

#define CPU_NUMBER(reg)	\
	movl    MY(CPU_ID), reg;

#ifndef __ASSEMBLER__
#include <kern/cpu_number.h>
#include <i386/apic.h>
#include <i386/percpu.h>

static inline int cpu_number_slow(void)
{
	return cpu_id_lut[apic_get_current_cpu()];
}

static inline int cpu_number(void)
{
	return percpu_get(int, cpu_id);
}
#endif

#else	/* NCPUS == 1 */

#define MY(stm)		(percpu_array + PERCPU_##stm)

#define	CPU_NUMBER_NO_STACK(reg)
#define	CPU_NUMBER_NO_GS(reg)
#define	CPU_NUMBER(reg)
#define	CX(addr,reg)	addr

#endif	/* NCPUS == 1 */

#endif	/* _I386_CPU_NUMBER_H_ */
