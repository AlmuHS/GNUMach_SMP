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
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	_setjmp(a)
 * by restoring registers from the stack,
 * The previous signal state is NOT restored.
 *
 */

#include <mach/machine/asm.h>

ENTRY(_setjmp)
	movq	%rbx,0(%rdi)
	movq	%rbp,8(%rdi)		/* save frame pointer of caller */
	movq	%r12,16(%rdi)
	movq	%r13,24(%rdi)
	movq	%r14,32(%rdi)
	movq	%r15,40(%rdi)
	popq	%rdx
	movq	%rsp,48(%rdi)		/* save stack pointer of caller */
	movq	%rdx,56(%rdi)		/* save pc of caller */
	xorq	%rax,%rax
        jmp     *%rdx

ENTRY(_longjmp)
	movq	%rsi,%rax		/* return(v) */
	movq	0(%rdi),%rbx
	movq	8(%rdi),%rbp
	movq	16(%rdi),%r12
	movq	24(%rdi),%r13
	movq	32(%rdi),%r14
	movq	40(%rdi),%r15
	movq	48(%rdi),%rsp
	orq	%rax,%rax
	jnz	0f
	incq	%rax
0:	jmp	*56(%rdi)		/* done, return.... */
