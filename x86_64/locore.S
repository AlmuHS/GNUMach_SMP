/*
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the nema IBM not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.
 *
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
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
#include <mach/machine/eflags.h>
#include <i386/i386/proc_reg.h>
#include <i386/i386/trap.h>
#include <i386/i386/seg.h>
#include <i386/i386/ldt.h>
#include <i386/i386/i386asm.h>
#include <i386/i386/cpu_number.h>
#include <i386/i386/xen.h>

#define pusha pushq %rax ; pushq %rcx ; pushq %rdx ; pushq %rbx ; subq $8,%rsp ; pushq %rbp ; pushq %rsi ; pushq %rdi ; pushq %r8 ; pushq %r9 ; pushq %r10 ; pushq %r11 ; pushq %r12 ; pushq %r13 ; pushq %r14 ; pushq %r15
#define popa popq %r15 ; popq %r14 ; popq %r13 ; popq %r12 ; popq %r11 ; popq %r10 ; popq %r9 ; popq %r8 ; popq %rdi ; popq %rsi ; popq %rbp ; addq $8,%rsp ; popq %rbx ; popq %rdx ; popq %rcx ; popq %rax

/*
 * Fault recovery.
 */
#define	RECOVER_TABLE_START	\
	.text	2		;\
DATA(recover_table)		;\
	.text

#define	RECOVER(addr)		\
	.text	2		;\
	.quad	9f		;\
	.quad	addr		;\
	.text			;\
9:

#define	RECOVER_TABLE_END		\
	.text	2			;\
	.globl	EXT(recover_table_end)	;\
LEXT(recover_table_end)			;\
	.text

/*
 * Retry table for certain successful faults.
 */
#define	RETRY_TABLE_START	\
	.text	3		;\
DATA(retry_table)		;\
	.text

#define	RETRY(addr)		\
	.text	3		;\
	.quad	9f		;\
	.quad	addr		;\
	.text			;\
9:

#define	RETRY_TABLE_END			\
	.text	3			;\
	.globl	EXT(retry_table_end)	;\
LEXT(retry_table_end)			;\
	.text

/*
 * Allocate recovery and retry tables.
 */
	RECOVER_TABLE_START
	RETRY_TABLE_START

/*
 * Timing routines.
 */
#if	STAT_TIME

#define	TIME_TRAP_UENTRY
#define	TIME_TRAP_SENTRY
#define	TIME_TRAP_UEXIT
#define	TIME_INT_ENTRY
#define	TIME_INT_EXIT

#else	/* microsecond timing */

/*
 * Microsecond timing.
 * Assumes a free-running microsecond counter.
 * no TIMER_MAX check needed.
 */

/*
 * There is only one current time-stamp per CPU, since only
 * the time-stamp in the current timer is used.
 * To save time, we allocate the current time-stamps here.
 */
	.comm	EXT(current_tstamp), 4*NCPUS

/*
 * Update time on user trap entry.
 * 11 instructions (including cli on entry)
 * Assumes CPU number in %edx.
 * Uses %eax, %ebx, %ecx.
 */
#define	TIME_TRAP_UENTRY \
	cli					/* block interrupts */	;\
	movl	VA_ETC,%ebx			/* get timer value */	;\
	movl	CX(EXT(current_tstamp),%edx),%ecx	/* get old time stamp */;\
	movl	%ebx,CX(EXT(current_tstamp),%edx)	/* set new time stamp */;\
	subl	%ecx,%ebx			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%edx),%ecx	/* get current timer */	;\
	addl	%ebx,LOW_BITS(%ecx)		/* add to low bits */	;\
	jns	0f				/* if overflow, */	;\
	call	timer_normalize			/* normalize timer */	;\
0:	addl	$(TH_SYSTEM_TIMER-TH_USER_TIMER),%ecx			;\
						/* switch to sys timer */;\
	movl	%ecx,CX(EXT(current_timer),%edx)	/* make it current */	;\
	sti					/* allow interrupts */

/*
 * Update time on system call entry.
 * 11 instructions (including cli on entry)
 * Assumes CPU number in %edx.
 * Uses %ebx, %ecx.
 * Same as TIME_TRAP_UENTRY, but preserves %eax.
 */
#define	TIME_TRAP_SENTRY \
	cli					/* block interrupts */	;\
	movl	VA_ETC,%ebx			/* get timer value */	;\
	movl	CX(EXT(current_tstamp),%edx),%ecx	/* get old time stamp */;\
	movl	%ebx,CX(EXT(current_tstamp),%edx)	/* set new time stamp */;\
	subl	%ecx,%ebx			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%edx),%ecx	/* get current timer */	;\
	addl	%ebx,LOW_BITS(%ecx)		/* add to low bits */	;\
	jns	0f				/* if overflow, */	;\
	pushq	%rax				/* save %rax */		;\
	call	timer_normalize			/* normalize timer */	;\
	popq	%rax				/* restore %rax */	;\
0:	addl	$(TH_SYSTEM_TIMER-TH_USER_TIMER),%ecx			;\
						/* switch to sys timer */;\
	movl	%ecx,CX(EXT(current_timer),%edx)	/* make it current */	;\
	sti					/* allow interrupts */

/*
 * update time on user trap exit.
 * 10 instructions.
 * Assumes CPU number in %edx.
 * Uses %ebx, %ecx.
 */
#define	TIME_TRAP_UEXIT \
	cli					/* block interrupts */	;\
	movl	VA_ETC,%ebx			/* get timer */		;\
	movl	CX(EXT(current_tstamp),%edx),%ecx	/* get old time stamp */;\
	movl	%ebx,CX(EXT(current_tstamp),%edx)	/* set new time stamp */;\
	subl	%ecx,%ebx			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%edx),%ecx	/* get current timer */	;\
	addl	%ebx,LOW_BITS(%ecx)		/* add to low bits */	;\
	jns	0f				/* if overflow,	*/	;\
	call	timer_normalize			/* normalize timer */	;\
0:	addl	$(TH_USER_TIMER-TH_SYSTEM_TIMER),%ecx			;\
						/* switch to user timer	*/;\
	movl	%ecx,CX(EXT(current_timer),%edx)	/* make it current */

/*
 * update time on interrupt entry.
 * 9 instructions.
 * Assumes CPU number in %edx.
 * Leaves old timer in %ebx.
 * Uses %ecx.
 */
#define	TIME_INT_ENTRY \
	movl	VA_ETC,%ecx			/* get timer */		;\
	movl	CX(EXT(current_tstamp),%edx),%ebx	/* get old time stamp */;\
	movl	%ecx,CX(EXT(current_tstamp),%edx)	/* set new time stamp */;\
	subl	%ebx,%ecx			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%edx),%ebx	/* get current timer */	;\
	addl	%ecx,LOW_BITS(%ebx)		/* add to low bits */	;\
	leal	CX(0,%edx),%ecx			/* timer is 16 bytes */	;\
	lea	CX(EXT(kernel_timer),%edx),%ecx	/* get interrupt timer*/;\
	movl	%ecx,CX(EXT(current_timer),%edx)	/* set timer */

/*
 * update time on interrupt exit.
 * 11 instructions
 * Assumes CPU number in %edx, old timer in %ebx.
 * Uses %eax, %ecx.
 */
#define	TIME_INT_EXIT \
	movl	VA_ETC,%eax			/* get timer */		;\
	movl	CX(EXT(current_tstamp),%edx),%ecx	/* get old time stamp */;\
	movl	%eax,CX(EXT(current_tstamp),%edx)	/* set new time stamp */;\
	subl	%ecx,%eax			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%edx),%ecx	/* get current timer */	;\
	addl	%eax,LOW_BITS(%ecx)		/* add to low bits */	;\
	jns	0f				/* if overflow, */	;\
	call	timer_normalize			/* normalize timer */	;\
0:	testb	$0x80,LOW_BITS+3(%ebx)		/* old timer overflow? */;\
	jz	0f				/* if overflow, */	;\
	movl	%ebx,%ecx			/* get old timer */	;\
	call	timer_normalize			/* normalize timer */	;\
0:	movl	%ebx,CX(EXT(current_timer),%edx)	/* set timer */


/*
 * Normalize timer in ecx.
 * Preserves edx; clobbers eax.
 */
	.align	2
timer_high_unit:
	.long	TIMER_HIGH_UNIT			/* div has no immediate opnd */

timer_normalize:
	pushq	%rdx				/* save register */
	xorl	%edx,%edx			/* clear divisor high */
	movl	LOW_BITS(%ecx),%eax		/* get divisor low */
	divl	timer_high_unit,%eax		/* quotient in eax */
						/* remainder in edx */
	addl	%eax,HIGH_BITS_CHECK(%ecx)	/* add high_inc to check  */
	movl	%edx,LOW_BITS(%ecx)		/* remainder to low_bits  */
	addl	%eax,HIGH_BITS(%ecx)		/* add high_inc to high bits */
	popq	%rdx				/* restore register  */
	ret

/*
 * Switch to a new timer.
 */
ENTRY(timer_switch)
	CPU_NUMBER(%edx)			/* get this CPU  */
	movl	VA_ETC,%ecx			/* get timer */
	movl	CX(EXT(current_tstamp),%edx),%eax	/* get old time stamp  */
	movl	%ecx,CX(EXT(current_tstamp),%edx)	/* set new time stamp */
	subl	%ecx,%eax			/* elapsed = new - old */
	movl	CX(EXT(current_timer),%edx),%ecx	/* get current timer */
	addl	%eax,LOW_BITS(%ecx)		/* add to low bits */
	jns	0f				/* if overflow, */
	call	timer_normalize			/* normalize timer */
0:
	movl	S_ARG0,%ecx			/* get new timer */
	movl	%ecx,CX(EXT(current_timer),%edx)	/* set timer */
	ret

/*
 * Initialize the first timer for a CPU.
 */
ENTRY(start_timer)
	CPU_NUMBER(%edx)			/* get this CPU */
	movl	VA_ETC,%ecx			/* get timer */
	movl	%ecx,CX(EXT(current_tstamp),%edx)	/* set initial time stamp */
	movl	S_ARG0,%ecx			/* get timer */
	movl	%ecx,CX(EXT(current_timer),%edx)	/* set initial timer */
	ret

#endif	/* accurate timing */

/**/

/*
 * Trap/interrupt entry points.
 *
 * All traps must create the following save area on the kernel stack:
 *
 *	gs
 *	fs
 *	es
 *	ds
 *	edi
 *	esi
 *	ebp
 *	cr2 if page fault - otherwise unused
 *	ebx
 *	edx
 *	ecx
 *	eax
 *	trap number
 *	error code
 *	eip
 *	cs
 *	eflags
 *	user rsp - if from user
 *	user ss  - if from user
 *	es       - if from V86 thread
 *	ds       - if from V86 thread
 *	fs       - if from V86 thread
 *	gs       - if from V86 thread
 *
 */

/*
 * General protection or segment-not-present fault.
 * Check for a GP/NP fault in the kernel_return
 * sequence; if there, report it as a GP/NP fault on the user's instruction.
 *
 * rsp->     0:	trap code (NP or GP)
 *	     8:	segment number in error
 *	    16	eip
 *	    24	cs
 *	    32	eflags
 *	    40	old registers (trap is from kernel)
 */
ENTRY(t_gen_prot)
	INT_FIX
	pushq	$(T_GENERAL_PROTECTION)	/* indicate fault type  */
	jmp	trap_check_kernel_exit	/* check for kernel exit sequence */

ENTRY(t_segnp)
	INT_FIX
	pushq	$(T_SEGMENT_NOT_PRESENT)
					/* indicate fault type */

trap_check_kernel_exit:
	testq	$(EFL_VM),32(%rsp)	/* is trap from V86 mode? */
	jnz	EXT(alltraps)		/* isn`t kernel trap if so */
	/* Note: handling KERNEL_RING value by hand */
	testq	$2,24(%rsp)		/* is trap from kernel mode? */
	jnz	EXT(alltraps)		/* if so:  */
					/* check for the kernel exit sequence */
	cmpq	$_kret_iret,16(%rsp)	/* on IRET? */
	je	fault_iret
#if 0
	cmpq	$_kret_popl_ds,16(%rsp)	/* popping DS? */
	je	fault_popl_ds
	cmpq	$_kret_popl_es,16(%rsp)	/* popping ES? */
	je	fault_popl_es
#endif
	cmpq	$_kret_popl_fs,16(%rsp)	/* popping FS? */
	je	fault_popl_fs
	cmpq	$_kret_popl_gs,16(%rsp)	/* popping GS? */
	je	fault_popl_gs
take_fault:				/* if none of the above: */
	jmp	EXT(alltraps)		/* treat as normal trap. */

/*
 * GP/NP fault on IRET: CS or SS is in error.
 * All registers contain the user's values.
 *
 * on SP is
 *  0	trap number
 *  8	errcode
 * 16	eip
 * 24	cs		--> trapno
 * 32	efl		--> errcode
 * 40	user eip
 * 48	user cs
 * 56	user eflags
 * 64	user rsp
 * 72	user ss
 */
fault_iret:
	movq	%rax,16(%rsp)		/* save eax (we don`t need saved eip) */
	popq	%rax			/* get trap number */
	movq	%rax,24-8(%rsp)		/* put in user trap number */
	popq	%rax			/* get error code */
	movq	%rax,32-16(%rsp)	/* put in user errcode */
	popq	%rax			/* restore eax */
	jmp	EXT(alltraps)		/* take fault */

/*
 * Fault restoring a segment register.  The user's registers are still
 * saved on the stack.  The offending segment register has not been
 * popped.
 */
fault_popl_ds:
	popq	%rax			/* get trap number  */
	popq	%rdx			/* get error code */
	addq	$24,%rsp		/* pop stack to user regs */
	jmp	push_es			/* (DS on top of stack) */
fault_popl_es:
	popq	%rax			/* get trap number */
	popq	%rdx			/* get error code */
	addq	$24,%rsp		/* pop stack to user regs */
	jmp	push_fs			/* (ES on top of stack) */
fault_popl_fs:
	popq	%rax			/* get trap number */
	popq	%rdx			/* get error code */
	addq	$24,%rsp		/* pop stack to user regs */
	jmp	push_gs			/* (FS on top of stack) */
fault_popl_gs:
	popq	%rax			/* get trap number */
	popq	%rdx			/* get error code */
	addq	$24,%rsp		/* pop stack to user regs */
	jmp	push_segregs		/* (GS on top of stack) */

push_es:
	//pushq	%es			/* restore es, */
push_fs:
	pushq	%fs			/* restore fs, */
push_gs:
	pushq	%gs			/* restore gs. */
push_segregs:
	movq	%rax,R_TRAPNO(%rsp)	/* set trap number */
	movq	%rdx,R_ERR(%rsp)	/* set error code */
	jmp	trap_set_segs		/* take trap */

/*
 * Debug trap.  Check for single-stepping across system call into
 * kernel.  If this is the case, taking the debug trap has turned
 * off single-stepping - save the flags register with the trace
 * bit set.
 */
ENTRY(t_debug)
	INT_FIX
	testq	$(EFL_VM),16(%rsp)	/* is trap from V86 mode? */
	jnz	0f			/* isn`t kernel trap if so */
	/* Note: handling KERNEL_RING value by hand */
	testq	$2,8(%rsp)		/* is trap from kernel mode? */
	jnz	0f			/* if so: */
	cmpq	$syscall_entry,(%rsp)	/* system call entry? */
	jne	0f			/* if so: */
					/* flags are sitting where syscall */
					/* wants them */
	addq	$32,%rsp		/* remove eip/cs */
	jmp	syscall_entry_2		/* continue system call entry */

0:	pushq	$0			/* otherwise: */
	pushq	$(T_DEBUG)		/* handle as normal */
	jmp	EXT(alltraps)		/* debug fault */

/*
 * Page fault traps save cr2.
 */
ENTRY(t_page_fault)
	INT_FIX
	pushq	$(T_PAGE_FAULT)		/* mark a page fault trap */
	pusha				/* save the general registers */
#ifdef	MACH_XEN
	movq	%ss:hyp_shared_info+CR2,%rax
#else	/* MACH_XEN */
	movq	%cr2,%rax		/* get the faulting address */
#endif	/* MACH_XEN */
	movq	%rax,R_CR2-R_R15(%rsp)	/* save in rsp save slot */
	jmp	trap_push_segs		/* continue fault */

/*
 * All 'exceptions' enter here with:
 *	rsp->   trap number
 *		error code
 *		old eip
 *		old cs
 *		old eflags
 *		old rsp		if trapped from user
 *		old ss		if trapped from user
 */
ENTRY(alltraps)
	pusha				/* save the general registers */
trap_push_segs:
	movq	%ds,%rax			/* and the segment registers */
	pushq	%rax
	movq	%es,%rax			/* and the segment registers */
	pushq	%rax
	pushq	%fs
	pushq	%gs

	/* Note that we have to load the segment registers
	   even if this is a trap from the kernel,
	   because the kernel uses user segment registers for copyin/copyout.
	   (XXX Would it be smarter just to use fs or gs for that?)  */
	mov	%ss,%ax			/* switch to kernel data segment */
	mov	%ax,%ds			/* (same as kernel stack segment) */
	mov	%ax,%es
	mov	%ax,%fs
	mov	%ax,%gs

trap_set_segs:
	cld				/* clear direction flag */
	testl	$(EFL_VM),R_EFLAGS(%rsp) /* in V86 mode? */
	jnz	trap_from_user		/* user mode trap if so */
	/* Note: handling KERNEL_RING value by hand */
	testb	$2,R_CS(%rsp)		/* user mode trap? */
	jz	trap_from_kernel	/* kernel trap if not */
trap_from_user:

	CPU_NUMBER(%edx)
	TIME_TRAP_UENTRY

	movq	CX(EXT(kernel_stack),%edx),%rbx
	xchgq	%rbx,%rsp		/* switch to kernel stack */
					/* user regs pointer already set */
_take_trap:
	movq	%rbx,%rdi		/* pass register save area to trap */
	call	EXT(user_trap)		/* call user trap routine */

	orq     %rax,%rax               /* emulated syscall? */
	jz      1f			/* no, just return */
	movq    R_EAX(%rbx),%rax        /* yes, get syscall number */
	jmp     syscall_entry_3         /* and emulate it */

1:
	movq	(%rsp),%rsp		/* switch back to PCB stack */

/*
 * Return from trap or system call, checking for ASTs.
 * On PCB stack.
 */

_return_from_trap:
	CPU_NUMBER(%edx)
	cmpl	$0,CX(EXT(need_ast),%edx)
	jz	_return_to_user		/* if we need an AST: */

	movq	CX(EXT(kernel_stack),%edx),%rsp
					/* switch to kernel stack */
	call	EXT(i386_astintr)	/* take the AST */
	popq	%rsp			/* switch back to PCB stack */
	jmp	_return_from_trap	/* and check again (rare) */
					/* ASTs after this point will */
					/* have to wait */

_return_to_user:
	TIME_TRAP_UEXIT

/*
 * Return from kernel mode to interrupted thread.
 */

_return_from_kernel:
_kret_popl_gs:
	popq	%gs			/* restore segment registers */
_kret_popl_fs:
	popq	%fs
_kret_popl_es:
	popq	%rax
	movq	%rax,%es
_kret_popl_ds:
	popq	%rax
	movq	%rax,%ds
	popa				/* restore general registers */
	addq	$16,%rsp		/* discard trap number and error code */
_kret_iret:
	iretq				/* return from interrupt */


/*
 * Trap from kernel mode.  No need to switch stacks.
 */
trap_from_kernel:
#if	MACH_KDB || MACH_TTD
	movq	%rsp,%rbx		/* save current stack */

	movq	%rsp,%rdx		/* on an interrupt stack? */
	and	$(~(KERNEL_STACK_SIZE-1)),%rdx
	cmpq	EXT(int_stack_base),%rdx
	je	1f			/* OK if so */

	CPU_NUMBER(%edx)		/* get CPU number */
	cmpq	CX(EXT(kernel_stack),%edx),%rsp
					/* already on kernel stack? */
	ja	0f
	cmpq	CX(EXT(active_stacks),%edx),%rsp
	ja	1f			/* switch if not */
0:
	movq	CX(EXT(kernel_stack),%edx),%rsp
1:
	pushq	%rbx			/* save old stack */
	movq	%rbx,%rdi		/* pass as parameter */
	call	EXT(kernel_trap)	/* to kernel trap routine */
	popq	%rsp			/* return to old stack */
#else	/* MACH_KDB || MACH_TTD */

	movq	%rsp,%rdi		/* pass parameter */
	call	EXT(kernel_trap)	/* to kernel trap routine */
#endif	/* MACH_KDB || MACH_TTD */

	jmp	_return_from_kernel


/*
 *	Called as a function, makes the current thread
 *	return from the kernel as if from an exception.
 */

ENTRY(thread_exception_return)
ENTRY(thread_bootstrap_return)
	movq	%rsp,%rcx			/* get kernel stack */
	or	$(KERNEL_STACK_SIZE-1),%ecx
	movq	-7-IKS_SIZE(%rcx),%rsp		/* switch back to PCB stack */
	jmp	_return_from_trap

/*
 *	Called as a function, makes the current thread
 *	return from the kernel as if from a syscall.
 *	Takes the syscall's return code as an argument.
 */

ENTRY(thread_syscall_return)
	movq	S_ARG0,%rax			/* get return value */
	movq	%rsp,%rcx			/* get kernel stack */
	or	$(KERNEL_STACK_SIZE-1),%ecx
	movq	-7-IKS_SIZE(%rcx),%rsp		/* switch back to PCB stack */
	movq	%rax,R_EAX(%rsp)		/* save return value */
	jmp	_return_from_trap

ENTRY(call_continuation)
	movq	S_ARG0,%rax			/* get continuation */
	movq	%rsp,%rcx			/* get kernel stack */
	or	$(KERNEL_STACK_SIZE-1),%rcx
	addq	$(-7-IKS_SIZE),%rcx
	movq	%rcx,%rsp			/* pop the stack */
	xorq	%rbp,%rbp			/* zero frame pointer */
	pushq	$0				/* Dummy return address */
	jmp	*%rax				/* goto continuation */


#define INTERRUPT(n)				\
	.data	2				;\
	.quad	0f				;\
	.text					;\
	P2ALIGN(TEXT_ALIGN)			;\
0:						;\
	INT_FIX					;\
	pushq	%rax				;\
	movq	$(n),%rax			;\
	jmp	EXT(all_intrs)

	.data	2
DATA(int_entry_table)
	.text
INTERRUPT(0)
INTERRUPT(1)
INTERRUPT(2)
INTERRUPT(3)
INTERRUPT(4)
INTERRUPT(5)
INTERRUPT(6)
INTERRUPT(7)
INTERRUPT(8)
INTERRUPT(9)
INTERRUPT(10)
INTERRUPT(11)
INTERRUPT(12)
INTERRUPT(13)
INTERRUPT(14)
INTERRUPT(15)

/* XXX handle NMI - at least print a warning like Linux does.  */

/*
 * All interrupts enter here.
 * old %eax on stack; interrupt number in %eax.
 */
ENTRY(all_intrs)
	pushq	%rcx			/* save registers */
	pushq	%rdx
	pushq	%rsi
	pushq	%rdi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	cld				/* clear direction flag */

	movq	%rsp,%rdx		/* on an interrupt stack? */
	and	$(~(KERNEL_STACK_SIZE-1)),%rdx
	cmpq	%ss:EXT(int_stack_base),%rdx
	je	int_from_intstack	/* if not: */

	movq	%ds,%rdx			/* save segment registers */
	pushq	%rdx
	movq	%es,%rdx
	pushq	%rdx
	pushq	%fs
	pushq	%gs
	mov	%ss,%dx			/* switch to kernel segments */
	mov	%dx,%ds
	mov	%dx,%es
	mov	%dx,%fs
	mov	%dx,%gs

	CPU_NUMBER(%edx)

	movq	CX(EXT(int_stack_top),%edx),%rcx

	xchgq	%rcx,%rsp		/* switch to interrupt stack */

#if	STAT_TIME
	pushq	%rcx			/* save pointer to old stack */
#else
	pushq	%rbx			/* save %ebx - out of the way */
					/* so stack looks the same */
	pushq	%rcx			/* save pointer to old stack */
	TIME_INT_ENTRY			/* do timing */
#endif

	call	EXT(interrupt)		/* call generic interrupt routine */

	.globl	EXT(return_to_iret)
LEXT(return_to_iret)			/* ( label for kdb_kintr and hardclock) */

	CPU_NUMBER(%edx)
#if	STAT_TIME
#else
	TIME_INT_EXIT			/* do timing */
	movq	8(%rsp),%rbx		/* restore the extra reg we saved */
#endif

	popq	%rsp			/* switch back to old stack */

	testl	$(EFL_VM),I_EFL(%rsp)	/* if in V86 */
	jnz	0f			/* or */
	/* Note: handling KERNEL_RING value by hand */
	testb	$2,I_CS(%rsp)		/* user mode, */
	jz	1f			/* check for ASTs */
0:
	cmpq	$0,CX(EXT(need_ast),%edx)
	jnz	ast_from_interrupt	/* take it if so */
1:
	pop	%gs			/* restore segment regs */
	pop	%fs
	pop	%rdx
	mov	%rdx,%es
	pop	%rdx
	mov	%rdx,%ds
	pop	%r11
	pop	%r10
	pop	%r9
	pop	%r8
	pop	%rdi
	pop	%rsi
	pop	%rdx
	pop	%rcx
	pop	%rax

	iretq				/* return to caller */

int_from_intstack:
	cmpq	EXT(int_stack_base),%rsp	/* seemingly looping? */
	jb	stack_overflowed	/* if not: */
	call	EXT(interrupt)		/* call interrupt routine */
_return_to_iret_i:			/* ( label for kdb_kintr) */
	pop	%r11
	pop	%r10
	pop	%r9
	pop	%r8
	pop	%rdi
	pop	%rsi
	pop	%rdx			/* must have been on kernel segs */
	pop	%rcx
	pop	%rax			/* no ASTs */

	iretq

stack_overflowed:
	ud2

/*
 *	Take an AST from an interrupt.
 *	On PCB stack.
 * sp->	gs	-> edx
 *	fs	-> ecx
 *	es	-> eax
 *	ds	-> trapno
 *	edx	-> code
 *	ecx
 *	eax
 *	eip
 *	cs
 *	efl
 *	rsp
 *	ss
 */
ast_from_interrupt:
	pop	%gs			/* restore all registers ... */
	pop	%fs
	pop	%rdx
	mov	%rdx,%es
	pop	%rdx
	mov	%rdx,%ds
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdi
	popq	%rsi
	popq	%rdx
	popq	%rcx
	popq	%rax
	pushq	$0			/* zero code */
	pushq	$0			/* zero trap number */
	pusha				/* save general registers */
	mov	%ds,%rdx		/* save segment registers */
	push	%rdx
	mov	%es,%rdx
	push	%rdx
	push	%fs
	push	%gs
	mov	%ss,%dx			/* switch to kernel segments */
	mov	%dx,%ds
	mov	%dx,%es
	mov	%dx,%fs
	mov	%dx,%gs

	CPU_NUMBER(%edx)
	TIME_TRAP_UENTRY

	movq	CX(EXT(kernel_stack),%edx),%rsp
					/* switch to kernel stack */
	call	EXT(i386_astintr)	/* take the AST */
	popq	%rsp			/* back to PCB stack */
	jmp	_return_from_trap	/* return */

#if	MACH_KDB
/*
 * kdb_kintr:	enter kdb from keyboard interrupt.
 * Chase down the stack frames until we find one whose return
 * address is the interrupt handler.   At that point, we have:
 *
 * frame->	saved %rbp
 *		return address in interrupt handler
 *		return address == return_to_iret_i
 *		saved %r11
 *		saved %r10
 *		saved %r9
 *		saved %r8
 *		saved %rdx
 *		saved %rcx
 *		saved %rax
 *		saved %rip
 *		saved %cs
 *		saved %rfl
 *
 * OR:
 * frame->	saved %rbp
 *		return address in interrupt handler
 *		return address == return_to_iret
 *		pointer to save area on old stack
 *	      [ saved %ebx, if accurate timing ]
 *
 * old stack:	saved %gs
 *		saved %fs
 *		saved %es
 *		saved %ds
 *		saved %r11
 *		saved %r10
 *		saved %r9
 *		saved %r8
 *		saved %rdi
 *		saved %rsi
 *		saved %rdx
 *		saved %rcx
 *		saved %eax
 *		saved %rip
 *		saved %cs
 *		saved %rfl
 *
 * Call kdb, passing it that register save area.
 */

#define	RET_OFFSET	16


ENTRY(kdb_kintr)
	movq	%rbp,%rax		/* save caller`s frame pointer */
	movq	$EXT(return_to_iret),%rcx	/* interrupt return address 1 */
	movq	$_return_to_iret_i,%rdx	/* interrupt return address 2 */

0:	cmpq	RET_OFFSET(%rax),%rcx	/* does this frame return to */
					/* interrupt handler (1)? */
	je	1f
	cmpq	RET_OFFSET(%rax),%rdx	/* interrupt handler (2)? */
	je	2f			/* if not: */
	movq	(%rax),%rax		/* try next frame */
	jmp	0b

1:	movq	$kdb_from_iret,RET_OFFSET(%rax)
	ret				/* returns to kernel/user stack */

2:	movq	$kdb_from_iret_i,RET_OFFSET(%rax)
					/* returns to interrupt stack */
	ret

/*
 * On return from keyboard interrupt, we will execute
 * kdb_from_iret_i
 *	if returning to an interrupt on the interrupt stack
 * kdb_from_iret
 *	if returning to an interrupt on the user or kernel stack
 */
kdb_from_iret:
					/* save regs in known locations */
#if	STAT_TIME
	pushq	%rbx			/* caller`s %ebx is in reg */
#else
	movq	8(%rsp),%rax		/* get caller`s %ebx */
	pushq	%rax			/* push on stack */
#endif
	pushq	%rbp
	movq	%rsp,%rdi		/* pass regs */
	call	EXT(kdb_kentry)		/* to kdb */
	popq	%rbp
#if	STAT_TIME
	popq	%rbx
#else
	popq	%rax
	movq	%rax,8(%rsp)
#endif
	jmp	EXT(return_to_iret)	/* normal interrupt return */

kdb_from_iret_i:			/* on interrupt stack */
	pop	%rdx			/* restore saved registers */
	pop	%rcx
	pop	%rax
	pushq	$0			/* zero error code */
	pushq	$0			/* zero trap number */
	pusha				/* save general registers */
	mov	%ds,%rdx		/* save segment registers */
	push	%rdx
	mov	%es,%rdx
	push	%rdx
	push	%fs
	push	%gs
	movq	%rsp,%rdx		/* pass regs, */
	movq	$0,%rsi			/* code, */
	movq	$-1,%rdi		/* type to kdb */
	call	EXT(kdb_trap)
	pop	%gs			/* restore segment registers */
	pop	%fs
	pop	%rdx
	mov	%rdx,%es
	pop	%rdx
	mov	%rdx,%ds
	popa				/* restore general registers */
	addq	$16,%rsp

// TODO: test it before dropping ud2
movq (%rsp),%rax
ud2
	iretq

#endif	/* MACH_KDB */

#if	MACH_TTD
/*
 * Same code as that above for the keyboard entry into kdb.
 */
ENTRY(kttd_intr)
// TODO: test it before dropping ud2
ud2
	movq	%rbp,%rax		/* save caller`s frame pointer */
	movq	$EXT(return_to_iret),%rcx	/* interrupt return address 1 */
	movq	$_return_to_iret_i,%rdx	/* interrupt return address 2 */

0:	cmpq	32(%rax),%rcx		/* does this frame return to */
					/* interrupt handler (1)? */
	je	1f
	cmpq	32(%rax),%rdx		/* interrupt handler (2)? */
	je	2f			/* if not: */
	movq	(%rax),%rax		/* try next frame */
	jmp	0b

1:	movq	$ttd_from_iret,32(%rax)	/* returns to kernel/user stack */
	ret

2:	movq	$ttd_from_iret_i,32(%rax)
					/* returns to interrupt stack */
	ret

/*
 * On return from keyboard interrupt, we will execute
 * ttd_from_iret_i
 *	if returning to an interrupt on the interrupt stack
 * ttd_from_iret
 *	if returning to an interrupt on the user or kernel stack
 */
ttd_from_iret:
					/* save regs in known locations */
#if	STAT_TIME
	pushq	%rbx			/* caller`s %ebx is in reg */
#else
	movq	8(%rsp),%rax		/* get caller`s %ebx */
	pushq	%rax			/* push on stack */
#endif
	pushq	%rbp
	pushq	%rsi
	pushq	%rdi
	movq	%rsp,%rdi		/* pass regs */
	call	_kttd_netentry		/* to kdb */
	popq	%rdi			/* restore registers */
	popq	%rsi
	popq	%rbp
#if	STAT_TIME
	popq	%rbx
#else
	popq	%rax
	movq	%rax,8(%rsp)
#endif
	jmp	EXT(return_to_iret)	/* normal interrupt return */

ttd_from_iret_i:			/* on interrupt stack */
	pop	%rdx			/* restore saved registers */
	pop	%rcx
	pop	%rax
	pushq	$0			/* zero error code */
	pushq	$0			/* zero trap number */
	pusha				/* save general registers */
	mov	%ds,%rdx		/* save segment registers */
	push	%rdx
	mov	%es,%rdx
	push	%rdx
	push	%fs
	push	%gs
	movq	%rsp,%rdx		/* pass regs, */
	movq	$0,%rsi			/* code, */
	movq	$-1,%rdi		/* type to kdb */
	call	_kttd_trap
	pop	%gs			/* restore segment registers */
	pop	%fs
	pop	%rdx
	mov	%rdx,%es
	pop	%rdx
	mov	%rdx,%ds
	popa				/* restore general registers */
	addq	$16,%rsp

// TODO: test it before dropping ud2
movq (%rsp),%rax
ud2
	iretq

#endif	/* MACH_TTD */

/*
 * System call enters through a call gate.  Flags are not saved -
 * we must shuffle stack to look like trap save area.
 *
 * rsp->	old eip
 *		old cs
 *		old rsp
 *		old ss
 *
 * eax contains system call number.
 */
ENTRY(syscall)
syscall_entry:
	pushf				/* save flags as soon as possible */
syscall_entry_2:
	cld				/* clear direction flag */

	pushq	%rax			/* save system call number */
	pushq	$0			/* clear trap number slot */

// TODO: test it before dropping ud2
	ud2

	pusha				/* save the general registers */
	movq	%ds,%rdx		/* and the segment registers */
	pushq	%rdx
	movq	%es,%rdx
	pushq	%rdx
	pushq	%fs
	pushq	%gs

	mov	%ss,%dx			/* switch to kernel data segment */
	mov	%dx,%ds
	mov	%dx,%es
	mov	%dx,%fs
	mov	%dx,%gs

/*
 * Shuffle eflags,eip,cs into proper places
 */

	movq	R_EIP(%rsp),%rbx	/* eflags are in EIP slot */
	movq	R_CS(%rsp),%rcx		/* eip is in CS slot */
	movq	R_EFLAGS(%rsp),%rdx	/* cs is in EFLAGS slot */
	movq	%rcx,R_EIP(%rsp)	/* fix eip */
	movq	%rdx,R_CS(%rsp)		/* fix cs */
	movq	%rbx,R_EFLAGS(%rsp)	/* fix eflags */

	CPU_NUMBER(%edx)
	TIME_TRAP_SENTRY

	movq	CX(EXT(kernel_stack),%edx),%rbx
					/* get current kernel stack */
	xchgq	%rbx,%rsp		/* switch stacks - %ebx points to */
					/* user registers. */
					/* user regs pointer already set */

/*
 * Check for MACH or emulated system call
 */
syscall_entry_3:
	movq	CX(EXT(active_threads),%edx),%rdx
					/* point to current thread */
	movq	TH_TASK(%rdx),%rdx	/* point to task */
	movq	TASK_EMUL(%rdx),%rdx	/* get emulation vector */
	orq	%rdx,%rdx		/* if none, */
	je	syscall_native		/*    do native system call */
	movq	%rax,%rcx		/* copy system call number */
	subq	DISP_MIN(%rdx),%rcx	/* get displacement into syscall */
					/* vector table */
	jl	syscall_native		/* too low - native system call */
	cmpq	DISP_COUNT(%rdx),%rcx	/* check range */
	jnl	syscall_native		/* too high - native system call */
	movq	DISP_VECTOR(%rdx,%rcx,4),%rdx
					/* get the emulation vector */
	orq	%rdx,%rdx		/* emulated system call if not zero */
	jnz	syscall_emul

/*
 * Native system call.
 */
syscall_native:
	negl	%eax			/* get system call number */
	jl	mach_call_range		/* out of range if it was positive */
	cmpl	EXT(mach_trap_count),%eax	/* check system call table bounds */
	jg	mach_call_range		/* error if out of range */
#if 0 /* debug hack to show the syscall number on the screen */
	movb	%al,%dl
	shrb	$4,%dl
	orb	$0x30,%dl
	movb	$0x0f,%dh
	movw	%dx,0xb800a
	movb	%al,%dl
	andb	$0xf,%dl
	orb	$0x30,%dl
	movb	$0xf,%dh
	movw	%dx,0xb800c
#endif
	shll	$5,%eax			/* manual indexing */
	xorq	%r10,%r10
	movl	EXT(mach_trap_table)(%eax),%r10d
					/* get number of arguments */
	andq	%r10,%r10
	jz	mach_call_call		/* skip argument copy if none */

	movq	R_UESP(%rbx),%rbx	/* get user stack pointer */
	addq	$4,%rbx			/* Skip user return address */

	movq	$USER_DS,%rdx		/* use user data segment for accesses */
	mov	%dx,%fs
	movq	%rsp,%r11		/* save kernel ESP for error recovery */

#define PARAM(reg,ereg) \
	RECOVER(mach_call_addr_push) \
	xorq	%reg,%reg		;\
	movl	%fs:(%rbx),%ereg	/* 1st parameter */ ;\
	addq	$4,%rbx			;\
	dec	%r10			;\
	jz	mach_call_call

	PARAM(rdi,edi)			/* 1st parameter */
	PARAM(rsi,esi)			/* 2nd parameter */
	PARAM(rdx,edx)			/* 3rd parameter */
	PARAM(rcx,ecx)			/* 4th parameter */
	PARAM(r8,r8d)			/* 5th parameter */
	PARAM(r9,r9d)			/* 6th parameter */

	lea	(%rbx,%r10,4),%rbx	/* point past last argument */
	xorq	%r12,%r12

0:	subq	$4,%rbx
	RECOVER(mach_call_addr_push)
	movl	%fs:(%rbx),%r12d
	pushq	%r12			/* push argument on stack */
	dec	%r10
	jnz	0b			/* loop for all arguments */

mach_call_call:

#ifdef DEBUG
	testb	$0xff,EXT(syscall_trace)
	jz	0f
	movq	%rax,%rdi
	call	EXT(syscall_trace_print)
	/* will return with syscallofs still (or again) in eax */
0:
#endif /* DEBUG */

	call	*EXT(mach_trap_table)+8(%eax)
					/* call procedure */
	movq	%rsp,%rcx		/* get kernel stack */
	or	$(KERNEL_STACK_SIZE-1),%rcx
	movq	-7-IKS_SIZE(%rcx),%rsp	/* switch back to PCB stack */
	movq	%rax,R_EAX(%rsp)	/* save return value */
	jmp	_return_from_trap	/* return to user */

/*
 * Address out of range.  Change to page fault.
 * %esi holds failing address.
 */
mach_call_addr_push:
	movq	%r11,%rsp		/* clean parameters from stack */
mach_call_addr:
	movq	%rsi,R_CR2(%rbx)	/* set fault address */
	movq	$(T_PAGE_FAULT),R_TRAPNO(%rbx)
					/* set page-fault trap */
	movq	$(T_PF_USER),R_ERR(%rbx)
					/* set error code - read user space */
	jmp	_take_trap		/* treat as a trap */

/*
 * System call out of range.  Treat as invalid-instruction trap.
 * (? general protection?)
 */
mach_call_range:
	movq	$(T_INVALID_OPCODE),R_TRAPNO(%rbx)
					/* set invalid-operation trap */
	movq	$0,R_ERR(%rbx)		/* clear error code */
	jmp	_take_trap		/* treat as a trap */

/*
 * User space emulation of system calls.
 * edx - user address to handle syscall
 *
 * User stack will become:
 * ursp->	eflags
 *		eip
 * eax still contains syscall number.
 */
syscall_emul:
	movq	$USER_DS,%rdi		/* use user data segment for accesses */
	mov	%di,%fs

/* XXX what about write-protected pages? */
	movq	R_UESP(%rbx),%rdi	/* get user stack pointer */
	subq	$16,%rdi		/* push space for new arguments */
	movq	R_EFLAGS(%rbx),%rax	/* move flags */
	RECOVER(syscall_addr)
	movl	%eax,%fs:0(%rdi)	/* to user stack */
	movl	R_EIP(%rbx),%eax	/* move eip */
	RECOVER(syscall_addr)
	movl	%eax,%fs:4(%rdi)	/* to user stack */
	movq	%rdi,R_UESP(%rbx)	/* set new user stack pointer */
	movq	%rdx,R_EIP(%rbx)	/* change return address to trap */
	movq	%rbx,%rsp		/* back to PCB stack */
// TODO: test it before dropping ud2
ud2
	jmp	_return_from_trap	/* return to user */

/*
 * Address error - address is in %edi.
 */
syscall_addr:
	movq	%rdi,R_CR2(%rbx)	/* set fault address */
	movq	$(T_PAGE_FAULT),R_TRAPNO(%rbx)
	        			/* set page-fault trap */
	movq	$(T_PF_USER),R_ERR(%rbx)
					/* set error code - read user space */
	jmp	_take_trap		/* treat as a trap */


	.data
DATA(cpu_features)
	.long	0
	.text

END(syscall)

/* Discover what kind of cpu we have; return the family number
   (3, 4, 5, 6, for 386, 486, 586, 686 respectively).  */
ENTRY(discover_x86_cpu_type)
	/* We are a modern enough processor to have the CPUID instruction;
	   use it to find out what we are. */
	movl	$1,%eax			/* Fetch CPU type info ... */
	cpuid				/*  ... into eax */
	movl	%edx,cpu_features	/* Keep a copy */
	shrl	$8,%eax			/* Slide family bits down */
	andl	$15,%eax		/* And select them */
	ret				/* And return */


/**/
/*
 * Utility routines.
 */

ENTRY(copyin)
	xchgq   %rsi,%rdi		/* Get user source and kernel destination */

copyin_remainder:
	/*cld*/				/* count up: default mode in all GCC code */
	movq	%rdx,%rcx		/* move by longwords first */
	shrq	$3,%rcx
	RECOVER(copyin_fail)
	rep
	movsq				/* move longwords */
	movq	%rdx,%rcx		/* now move remaining bytes */
	andq	$7,%rcx
	RECOVER(copyin_fail)
	rep
	movsb
	xorq	%rax,%rax		/* return 0 for success */

copyin_ret:
	ret				/* and return */

copyin_fail:
	movq	$1,%rax			/* return 1 for failure */
	jmp	copyin_ret		/* pop frame and return */

/*
 * Copy from user address space - version for copying messages.
 * arg0:	user address
 * arg1:	kernel address
 * arg2:	byte count
 */
ENTRY(copyinmsg)
	xchgq   %rsi,%rdi		/* Get user source and kernel destination */

/* 32 on 64 conversion */
	subq	$32,%rdx
	js	bogus

	/* Copy msgh_bits */
	RECOVER(copyin_fail)
	movsl

	/* Copy msgh_size */
	RECOVER(copyin_fail)
	lodsl
	addl	$8,%eax
	stosl

	xorq	%rax,%rax
	/* Copy msgh_remote_port */
	RECOVER(copyin_fail)
	lodsl
	stosq

	/* Copy msgh_local_port */
	RECOVER(copyin_fail)
	lodsl
	stosq

	/* Copy msgh_seqno and msgh_id */
	RECOVER(copyin_fail)
	movsq

	jmp	copyin_remainder

bogus:
	ud2

ENTRY(copyout)
	xchgq   %rsi,%rdi		/* Get user source and kernel destination */

copyout_remainder:
	movq	%rdx,%rax		/* use count */
	/*cld*/				/* count up: always this way in GCC code */
	movq	%rax,%rcx		/* move by longwords first */
	shrq	$3,%rcx
	RECOVER(copyout_fail)
	rep
	movsq
	movq	%rax,%rcx		/* now move remaining bytes */
	andq	$7,%rcx
	RECOVER(copyout_fail)
	rep
	movsb				/* move */
	xorq	%rax,%rax		/* return 0 for success */

copyout_ret:
	ret				/* and return */

copyout_fail:
	movq	$1,%rax			/* return 1 for failure */
	jmp	copyout_ret		/* pop frame and return */

/*
 * Copy to user address space.
 * arg0:	kernel address
 * arg1:	user address
 * arg2:	byte count
 */
ENTRY(copyoutmsg)
	xchgq   %rsi,%rdi		/* Get user source and kernel destination */

/* 32 on 64 conversion */
	subq	$32,%rdx
	js	bogus

	/* Copy msgh_bits */
	RECOVER(copyout_fail)
	movsl

	/* Copy msgh_size */
	lodsl
	subl	$8,%eax
	RECOVER(copyout_fail)
	stosl

	/* Copy msgh_remote_port */
	lodsq
	RECOVER(copyout_fail)
	stosl

	/* Copy msgh_local_port */
	lodsq
	RECOVER(copyout_fail)
	stosl

	/* Copy msgh_seqno and msgh_id */
	RECOVER(copyout_fail)
	movsq

	jmp	copyin_remainder

/*
 * int inst_fetch(int eip, int cs);
 *
 * Fetch instruction byte.  Return -1 if invalid address.
 */
ENTRY(inst_fetch)
	movq	S_ARG1, %rax		/* get segment */
	movw	%ax,%fs			/* into FS */
	movq	S_ARG0, %rax		/* get offset */
	RETRY(EXT(inst_fetch))		/* re-load FS on retry */
	RECOVER(_inst_fetch_fault)
	movzbq	%fs:(%rax),%rax		/* load instruction byte */
	ret

_inst_fetch_fault:
	movq	$-1,%rax		/* return -1 if error */
	ret


/*
 * Done with recovery and retry tables.
 */
	RECOVER_TABLE_END
	RETRY_TABLE_END



/*
 * cpu_shutdown()
 * Force reboot
 */
null_idt:
	.space	8 * 32

null_idtr:
        .word   8 * 32 - 1
        .quad   null_idt

Entry(cpu_shutdown)
        lidt    null_idtr       /* disable the interrupt handler */
        xor     %rcx,%rcx       /* generate a divide by zero */
        div     %rcx,%rax       /* reboot now */
        ret                     /* this will "never" be executed */
