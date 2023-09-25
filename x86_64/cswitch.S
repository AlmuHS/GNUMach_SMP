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

#include <mach/machine/asm.h>

#include <i386/i386/proc_reg.h>
#include <i386/i386/i386asm.h>
#include <i386/i386/cpu_number.h>

/*
 * Context switch routines for x86_64.
 */

ENTRY(Load_context)
	movq	S_ARG0,%rcx			/* get thread */
	movq	TH_KERNEL_STACK(%rcx),%rcx	/* get kernel stack */
	lea	KERNEL_STACK_SIZE-IKS_SIZE-IEL_SIZE(%rcx),%rdx
						/* point to stack top */
	CPU_NUMBER(%eax)
	movq	%rcx,MY(ACTIVE_STACK)		/* store stack address */
	movq	%rdx,CX(EXT(kernel_stack),%rax)	/* store stack top */

/* XXX complete */

	movq	KSS_ESP(%rcx),%rsp		/* switch stacks */
	movq	KSS_EBP(%rcx),%rbp		/* restore registers */
	movq	KSS_EBX(%rcx),%rbx
	movq	KSS_R12(%rcx),%r12
	movq	KSS_R13(%rcx),%r13
	movq	KSS_R14(%rcx),%r14
	movq	KSS_R15(%rcx),%r15
	xorq	%rax,%rax			/* return zero (no old thread) */
	jmp	*KSS_EIP(%rcx)			/* resume thread */

/*
 *	This really only has to save registers
 *	when there is no explicit continuation.
 */

ENTRY(Switch_context)
	movq	MY(ACTIVE_STACK),%rcx		/* get old kernel stack */

	movq	%r12,KSS_R12(%rcx)		/* save registers */
	movq	%r13,KSS_R13(%rcx)
	movq	%r14,KSS_R14(%rcx)
	movq	%r15,KSS_R15(%rcx)
	movq	%rbx,KSS_EBX(%rcx)
	movq	%rbp,KSS_EBP(%rcx)
	popq	KSS_EIP(%rcx)			/* save return PC */
	movq	%rsp,KSS_ESP(%rcx)		/* save SP */

	movq	S_ARG0,%rax			/* get old thread */
	movq	%rcx,TH_KERNEL_STACK(%rax)	/* save old stack */
	movq	S_ARG1,%rbx			/* get continuation */
	movq	%rbx,TH_SWAP_FUNC(%rax)		/* save continuation */

	movq	S_ARG2,%rsi			/* get new thread */

	movq	TH_KERNEL_STACK(%rsi),%rcx	/* get its kernel stack */
	lea     KERNEL_STACK_SIZE-IKS_SIZE-IEL_SIZE(%rcx),%rbx
	        				/* point to stack top */

	CPU_NUMBER(%eax)
	movq	%rsi,MY(ACTIVE_THREAD)		/* new thread is active */
	movq	%rcx,MY(ACTIVE_STACK)		/* set current stack */
	movq	%rbx,CX(EXT(kernel_stack),%rax)	/* set stack top */

	movq	KSS_ESP(%rcx),%rsp		/* switch stacks */
	movq	KSS_EBP(%rcx),%rbp		/* restore registers */
	movq	KSS_EBX(%rcx),%rbx
	movq	KSS_R12(%rcx),%r12
	movq	KSS_R13(%rcx),%r13
	movq	KSS_R14(%rcx),%r14
	movq	KSS_R15(%rcx),%r15
	jmp	*KSS_EIP(%rcx)			/* return old thread */

ENTRY(Thread_continue)
	movq	%rax,%rdi			/* push the thread argument */
	xorq	%rbp,%rbp			/* zero frame pointer */
	call	*%rbx				/* call real continuation */

#if	NCPUS > 1
/*
 * void switch_to_shutdown_context(thread_t thread,
 *				   void (*routine)(processor_t),
 *				   processor_t processor)
 *
 * saves the kernel context of the thread,
 * switches to the interrupt stack,
 * continues the thread (with thread_continue),
 * then runs routine on the interrupt stack.
 *
 * Assumes that the thread is a kernel thread (thus
 * has no FPU state)
 */
ENTRY(switch_to_shutdown_context)
ud2
	movq	MY(ACTIVE_STACK),%rcx		/* get old kernel stack */
	movq	%r12,KSS_R12(%rcx)		/* save registers */
	movq	%r13,KSS_R13(%rcx)
	movq	%r14,KSS_R14(%rcx)
	movq	%r15,KSS_R15(%rcx)
	movq	%rbx,KSS_EBX(%rcx)
	movq	%rbp,KSS_EBP(%rcx)
	popq	KSS_EIP(%rcx)   		/* save return PC */
	movq	%rsp,KSS_ESP(%rcx)		/* save SP */

	movq	S_ARG0,%rax			/* get old thread */
	movq	%rcx,TH_KERNEL_STACK(%rax)	/* save old stack */
	movq	$0,TH_SWAP_FUNC(%rax)		/* clear continuation */
	movq	S_ARG1,%rbx			/* get routine to run next */
	movq	S_ARG2,%rsi			/* get its argument */

	CPU_NUMBER(%ecx)
	movq	CX(EXT(int_stack_base),%rcx),%rcx	/* point to its interrupt stack */
	lea	INTSTACK_SIZE(%rcx),%rsp	/* switch to it (top) */

	movq	%rax,%rdi			/* push thread */
	call	EXT(thread_dispatch)		/* reschedule thread */

	movq	%rsi,%rdi			/* push argument */
	call	*%rbx				/* call routine to run */
	hlt					/* (should never return) */

#endif	/* NCPUS > 1 */
