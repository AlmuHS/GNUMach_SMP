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
#include <i386/proc_reg.h>
#include <i386/trap.h>
#include <i386/seg.h>
#include <i386/ldt.h>
#include <i386/i386asm.h>
#include <i386/cpu_number.h>
#include <i386/xen.h>

/*
 * Fault recovery.
 */
#define	RECOVER_TABLE_START	\
	.text	2		;\
DATA(recover_table)		;\
	.text

#define	RECOVER(addr)		\
	.text	2		;\
	.long	9f		;\
	.long	addr		;\
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
	.long	9f		;\
	.long	addr		;\
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
	pushl	%eax				/* save %eax */		;\
	call	timer_normalize			/* normalize timer */	;\
	popl	%eax				/* restore %eax */	;\
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
	pushl	%edx				/* save register */
	xorl	%edx,%edx			/* clear divisor high */
	movl	LOW_BITS(%ecx),%eax		/* get divisor low */
	divl	timer_high_unit,%eax		/* quotient in eax */
						/* remainder in edx */
	addl	%eax,HIGH_BITS_CHECK(%ecx)	/* add high_inc to check  */
	movl	%edx,LOW_BITS(%ecx)		/* remainder to low_bits  */
	addl	%eax,HIGH_BITS(%ecx)		/* add high_inc to high bits */
	popl	%edx				/* restore register  */
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
 *	user esp - if from user
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
 * esp->     0:	trap code (NP or GP)
 *	     4:	segment number in error
 *	     8	eip
 *	    12	cs
 *	    16	eflags
 *	    20	old registers (trap is from kernel)
 */
ENTRY(t_gen_prot)
	pushl	$(T_GENERAL_PROTECTION)	/* indicate fault type  */
	jmp	trap_check_kernel_exit	/* check for kernel exit sequence */

ENTRY(t_segnp)
	pushl	$(T_SEGMENT_NOT_PRESENT)
					/* indicate fault type */

trap_check_kernel_exit:
	testl	$(EFL_VM),16(%esp)	/* is trap from V86 mode? */
	jnz	EXT(alltraps)		/* isn`t kernel trap if so */
	/* Note: handling KERNEL_RING value by hand */
	testl	$2,12(%esp)		/* is trap from kernel mode? */
	jnz	EXT(alltraps)		/* if so:  */
					/* check for the kernel exit sequence */
	cmpl	$_kret_iret,8(%esp)	/* on IRET? */
	je	fault_iret
	cmpl	$_kret_popl_ds,8(%esp)	/* popping DS? */
	je	fault_popl_ds
	cmpl	$_kret_popl_es,8(%esp)	/* popping ES? */
	je	fault_popl_es
	cmpl	$_kret_popl_fs,8(%esp)	/* popping FS? */
	je	fault_popl_fs
	cmpl	$_kret_popl_gs,8(%esp)	/* popping GS? */
	je	fault_popl_gs
take_fault:				/* if none of the above: */
	jmp	EXT(alltraps)		/* treat as normal trap. */

/*
 * GP/NP fault on IRET: CS or SS is in error.
 * All registers contain the user's values.
 *
 * on SP is
 *  0	trap number
 *  4	errcode
 *  8	eip
 * 12	cs		--> trapno
 * 16	efl		--> errcode
 * 20	user eip
 * 24	user cs
 * 28	user eflags
 * 32	user esp
 * 36	user ss
 */
fault_iret:
	movl	%eax,8(%esp)		/* save eax (we don`t need saved eip) */
	popl	%eax			/* get trap number */
	movl	%eax,12-4(%esp)		/* put in user trap number */
	popl	%eax			/* get error code */
	movl	%eax,16-8(%esp)		/* put in user errcode */
	popl	%eax			/* restore eax */
	jmp	EXT(alltraps)		/* take fault */

/*
 * Fault restoring a segment register.  The user's registers are still
 * saved on the stack.  The offending segment register has not been
 * popped.
 */
fault_popl_ds:
	popl	%eax			/* get trap number  */
	popl	%edx			/* get error code */
	addl	$12,%esp		/* pop stack to user regs */
	jmp	push_es			/* (DS on top of stack) */
fault_popl_es:
	popl	%eax			/* get trap number */
	popl	%edx			/* get error code */
	addl	$12,%esp		/* pop stack to user regs */
	jmp	push_fs			/* (ES on top of stack) */
fault_popl_fs:
	popl	%eax			/* get trap number */
	popl	%edx			/* get error code */
	addl	$12,%esp		/* pop stack to user regs */
	jmp	push_gs			/* (FS on top of stack) */
fault_popl_gs:
	popl	%eax			/* get trap number */
	popl	%edx			/* get error code */
	addl	$12,%esp		/* pop stack to user regs */
	jmp	push_segregs		/* (GS on top of stack) */

push_es:
	pushl	%es			/* restore es, */
push_fs:
	pushl	%fs			/* restore fs, */
push_gs:
	pushl	%gs			/* restore gs. */
push_segregs:
	movl	%eax,R_TRAPNO(%esp)	/* set trap number */
	movl	%edx,R_ERR(%esp)	/* set error code */
	jmp	trap_set_segs		/* take trap */

/*
 * Debug trap.  Check for single-stepping across system call into
 * kernel.  If this is the case, taking the debug trap has turned
 * off single-stepping - save the flags register with the trace
 * bit set.
 */
ENTRY(t_debug)
	testl	$(EFL_VM),8(%esp)	/* is trap from V86 mode? */
	jnz	0f			/* isn`t kernel trap if so */
	/* Note: handling KERNEL_RING value by hand */
	testl	$2,4(%esp)		/* is trap from kernel mode? */
	jnz	0f			/* if so: */
	cmpl	$syscall_entry,(%esp)	/* system call entry? */
	jne	0f			/* if so: */
					/* flags are sitting where syscall */
					/* wants them */
	addl	$8,%esp			/* remove eip/cs */
	jmp	syscall_entry_2		/* continue system call entry */

0:	pushl	$0			/* otherwise: */
	pushl	$(T_DEBUG)		/* handle as normal */
	jmp	EXT(alltraps)		/* debug fault */

/*
 * Page fault traps save cr2.
 */
ENTRY(t_page_fault)
	pushl	$(T_PAGE_FAULT)		/* mark a page fault trap */
	pusha				/* save the general registers */
#ifdef	MACH_XEN
	movl	%ss:hyp_shared_info+CR2,%eax
#else	/* MACH_XEN */
	movl	%cr2,%eax		/* get the faulting address */
#endif	/* MACH_XEN */
	movl	%eax,12(%esp)		/* save in esp save slot */
	jmp	trap_push_segs		/* continue fault */

/*
 * All 'exceptions' enter here with:
 *	esp->   trap number
 *		error code
 *		old eip
 *		old cs
 *		old eflags
 *		old esp		if trapped from user
 *		old ss		if trapped from user
 */
ENTRY(alltraps)
	pusha				/* save the general registers */
trap_push_segs:
	pushl	%ds			/* and the segment registers */
	pushl	%es
	pushl	%fs
	pushl	%gs

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
	testl	$(EFL_VM),R_EFLAGS(%esp) /* in V86 mode? */
	jnz	trap_from_user		/* user mode trap if so */
	/* Note: handling KERNEL_RING value by hand */
	testb	$2,R_CS(%esp)		/* user mode trap? */
	jz	trap_from_kernel	/* kernel trap if not */
trap_from_user:

	CPU_NUMBER(%edx)
	TIME_TRAP_UENTRY

	movl	CX(EXT(kernel_stack),%edx),%ebx
	xchgl	%ebx,%esp		/* switch to kernel stack */
					/* user regs pointer already set */
_take_trap:
	pushl	%ebx			/* pass register save area to trap */
	call	EXT(user_trap)		/* call user trap routine */
	movl	4(%esp),%esp		/* switch back to PCB stack */

	orl     %eax,%eax               /* emulated syscall? */
	jz      _return_from_trap       /* no, just return */
	movl    R_EAX(%ebx),%eax        /* yes, get syscall number */
	jmp     syscall_entry_3         /* and emulate it */

/*
 * Return from trap or system call, checking for ASTs.
 * On PCB stack.
 */

_return_from_trap:
	CPU_NUMBER(%edx)
	cmpl	$0,CX(EXT(need_ast),%edx)
	jz	_return_to_user		/* if we need an AST: */

	movl	CX(EXT(kernel_stack),%edx),%esp
					/* switch to kernel stack */
	call	EXT(i386_astintr)	/* take the AST */
	popl	%esp			/* switch back to PCB stack */
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
	popl	%gs			/* restore segment registers */
_kret_popl_fs:
	popl	%fs
_kret_popl_es:
	popl	%es
_kret_popl_ds:
	popl	%ds
	popa				/* restore general registers */
	addl	$8,%esp			/* discard trap number and error code */
_kret_iret:
	iret				/* return from interrupt */


/*
 * Trap from kernel mode.  No need to switch stacks.
 */
trap_from_kernel:
#if	MACH_KDB || MACH_TTD
	movl	%esp,%ebx		/* save current stack */

	cmpl	EXT(int_stack_high),%esp	/* on an interrupt stack? */
	jb	1f			/* OK if so */

	CPU_NUMBER(%edx)		/* get CPU number */
	cmpl	CX(EXT(kernel_stack),%edx),%esp
					/* already on kernel stack? */
	ja	0f
	cmpl	CX(EXT(active_stacks),%edx),%esp
	ja	1f			/* switch if not */
0:
	movl	CX(EXT(kernel_stack),%edx),%esp
1:
	pushl	%ebx			/* save old stack */
	pushl	%ebx			/* pass as parameter */
	call	EXT(kernel_trap)	/* to kernel trap routine */
	addl	$4,%esp			/* pop parameter */
	popl	%esp			/* return to old stack */
#else	/* MACH_KDB || MACH_TTD */

	pushl	%esp			/* pass parameter */
	call	EXT(kernel_trap)	/* to kernel trap routine */
	addl	$4,%esp			/* pop parameter */
#endif	/* MACH_KDB || MACH_TTD */
	jmp	_return_from_kernel


/*
 *	Called as a function, makes the current thread
 *	return from the kernel as if from an exception.
 */

ENTRY(thread_exception_return)
ENTRY(thread_bootstrap_return)
	movl	%esp,%ecx			/* get kernel stack */
	or	$(KERNEL_STACK_SIZE-1),%ecx
	movl	-3-IKS_SIZE(%ecx),%esp		/* switch back to PCB stack */
	jmp	_return_from_trap

/*
 *	Called as a function, makes the current thread
 *	return from the kernel as if from a syscall.
 *	Takes the syscall's return code as an argument.
 */

ENTRY(thread_syscall_return)
	movl	S_ARG0,%eax			/* get return value */
	movl	%esp,%ecx			/* get kernel stack */
	or	$(KERNEL_STACK_SIZE-1),%ecx
	movl	-3-IKS_SIZE(%ecx),%esp		/* switch back to PCB stack */
	movl	%eax,R_EAX(%esp)		/* save return value */
	jmp	_return_from_trap

ENTRY(call_continuation)
	movl	S_ARG0,%eax			/* get continuation */
	movl	%esp,%ecx			/* get kernel stack */
	or	$(KERNEL_STACK_SIZE-1),%ecx
	addl	$(-3-IKS_SIZE),%ecx
	movl	%ecx,%esp			/* pop the stack */
	xorl	%ebp,%ebp			/* zero frame pointer */
	jmp	*%eax				/* goto continuation */



#define INTERRUPT(n)				\
	.data	2				;\
	.long	0f				;\
	.text					;\
	P2ALIGN(TEXT_ALIGN)			;\
0:						;\
	pushl	%eax				;\
	movl	$(n),%eax			;\
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
	pushl	%ecx			/* save registers */
	pushl	%edx
	cld				/* clear direction flag */

	cmpl	%ss:EXT(int_stack_high),%esp /* on an interrupt stack? */
	jb	int_from_intstack	/* if not: */

	pushl	%ds			/* save segment registers */
	pushl	%es
	pushl	%fs
	pushl	%gs
	mov	%ss,%dx			/* switch to kernel segments */
	mov	%dx,%ds
	mov	%dx,%es
	mov	%dx,%fs
	mov	%dx,%gs

	CPU_NUMBER(%edx)

	movl	CX(EXT(int_stack_top),%edx),%ecx
	xchgl	%ecx,%esp		/* switch to interrupt stack */

#if	STAT_TIME
	pushl	%ecx			/* save pointer to old stack */
#else
	pushl	%ebx			/* save %ebx - out of the way */
					/* so stack looks the same */
	pushl	%ecx			/* save pointer to old stack */
	TIME_INT_ENTRY			/* do timing */
#endif

	call	EXT(interrupt)		/* call generic interrupt routine */

	.globl	EXT(return_to_iret)
LEXT(return_to_iret)			/* ( label for kdb_kintr and hardclock) */

	CPU_NUMBER(%edx)
#if	STAT_TIME
#else
	TIME_INT_EXIT			/* do timing */
	movl	4(%esp),%ebx		/* restore the extra reg we saved */
#endif

	popl	%esp			/* switch back to old stack */

	testl	$(EFL_VM),I_EFL(%esp)	/* if in V86 */
	jnz	0f			/* or */
	/* Note: handling KERNEL_RING value by hand */
	testb	$2,I_CS(%esp)		/* user mode, */
	jz	1f			/* check for ASTs */
0:
	cmpl	$0,CX(EXT(need_ast),%edx)
	jnz	ast_from_interrupt	/* take it if so */
1:
	pop	%gs			/* restore segment regs */
	pop	%fs
	pop	%es
	pop	%ds
	pop	%edx
	pop	%ecx
	pop	%eax
	iret				/* return to caller */

int_from_intstack:
	cmpl	$EXT(_intstack),%esp	/* seemingly looping? */
	jb	stack_overflowed	/* if not: */
	call	EXT(interrupt)		/* call interrupt routine */
_return_to_iret_i:			/* ( label for kdb_kintr) */
	pop	%edx			/* must have been on kernel segs */
	pop	%ecx
	pop	%eax			/* no ASTs */
	iret

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
 *	esp
 *	ss
 */
ast_from_interrupt:
	pop	%gs			/* restore all registers ... */
	pop	%fs
	pop	%es
	pop	%ds
	popl	%edx
	popl	%ecx
	popl	%eax
	pushl	$0			/* zero code */
	pushl	$0			/* zero trap number */
	pusha				/* save general registers */
	push	%ds			/* save segment registers */
	push	%es
	push	%fs
	push	%gs
	mov	%ss,%dx			/* switch to kernel segments */
	mov	%dx,%ds
	mov	%dx,%es
	mov	%dx,%fs
	mov	%dx,%gs

	CPU_NUMBER(%edx)
	TIME_TRAP_UENTRY

	movl	CX(EXT(kernel_stack),%edx),%esp
					/* switch to kernel stack */
	call	EXT(i386_astintr)	/* take the AST */
	popl	%esp			/* back to PCB stack */
	jmp	_return_from_trap	/* return */

#if	MACH_KDB
/*
 * kdb_kintr:	enter kdb from keyboard interrupt.
 * Chase down the stack frames until we find one whose return
 * address is the interrupt handler.   At that point, we have:
 *
 * frame->	saved %ebp
 *		return address in interrupt handler
 * #ifndef MACH_XEN
 *		iunit
 *		saved SPL
 * #endif
 *		return address == return_to_iret_i
 *		saved %edx
 *		saved %ecx
 *		saved %eax
 *		saved %eip
 *		saved %cs
 *		saved %efl
 *
 * OR:
 * frame->	saved %ebp
 *		return address in interrupt handler
 * #ifndef MACH_XEN
 *		iunit
 *		saved SPL
 * #endif
 *		return address == return_to_iret
 *		pointer to save area on old stack
 *	      [ saved %ebx, if accurate timing ]
 *
 * old stack:	saved %gs
 *		saved %fs
 *		saved %es
 *		saved %ds
 *		saved %edx
 *		saved %ecx
 *		saved %eax
 *		saved %eip
 *		saved %cs
 *		saved %efl
 *
 * Call kdb, passing it that register save area.
 */

#ifdef	MACH_XEN
#define	RET_OFFSET	8
#else	/* MACH_XEN */
#define	RET_OFFSET	16
#endif	/* MACH_XEN */

ENTRY(kdb_kintr)
	movl	%ebp,%eax		/* save caller`s frame pointer */
	movl	$EXT(return_to_iret),%ecx	/* interrupt return address 1 */
	movl	$_return_to_iret_i,%edx	/* interrupt return address 2 */

0:	cmpl	RET_OFFSET(%eax),%ecx	/* does this frame return to */
					/* interrupt handler (1)? */
	je	1f
	cmpl	RET_OFFSET(%eax),%edx	/* interrupt handler (2)? */
	je	2f			/* if not: */
	movl	(%eax),%eax		/* try next frame */
	jmp	0b

1:	movl	$kdb_from_iret,RET_OFFSET(%eax)
	ret				/* returns to kernel/user stack */

2:	movl	$kdb_from_iret_i,RET_OFFSET(%eax)
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
	pushl	%ebx			/* caller`s %ebx is in reg */
#else
	movl	4(%esp),%eax		/* get caller`s %ebx */
	pushl	%eax			/* push on stack */
#endif
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%esp			/* pass regs */
	call	EXT(kdb_kentry)		/* to kdb */
	addl	$4,%esp			/* pop parameters */
	popl	%edi			/* restore registers */
	popl	%esi
	popl	%ebp
#if	STAT_TIME
	popl	%ebx
#else
	popl	%eax
	movl	%eax,4(%esp)
#endif
	jmp	EXT(return_to_iret)	/* normal interrupt return */

kdb_from_iret_i:			/* on interrupt stack */
	pop	%edx			/* restore saved registers */
	pop	%ecx
	pop	%eax
	pushl	$0			/* zero error code */
	pushl	$0			/* zero trap number */
	pusha				/* save general registers */
	push	%ds			/* save segment registers */
	push	%es
	push	%fs
	push	%gs
	pushl	%esp			/* pass regs, */
	pushl	$0			/* code, */
	pushl	$-1			/* type to kdb */
	call	EXT(kdb_trap)
	addl	$12,%esp		/* remove parameters */
	pop	%gs			/* restore segment registers */
	pop	%fs
	pop	%es
	pop	%ds
	popa				/* restore general registers */
	addl	$8,%esp
	iret

#endif	/* MACH_KDB */

#if	MACH_TTD
/*
 * Same code as that above for the keyboard entry into kdb.
 */
ENTRY(kttd_intr)
	movl	%ebp,%eax		/* save caller`s frame pointer */
	movl	$EXT(return_to_iret),%ecx	/* interrupt return address 1 */
	movl	$_return_to_iret_i,%edx	/* interrupt return address 2 */

0:	cmpl	16(%eax),%ecx		/* does this frame return to */
					/* interrupt handler (1)? */
	je	1f
	cmpl	16(%eax),%edx		/* interrupt handler (2)? */
	je	2f			/* if not: */
	movl	(%eax),%eax		/* try next frame */
	jmp	0b

1:	movl	$ttd_from_iret,16(%eax)	/* returns to kernel/user stack */
	ret

2:	movl	$ttd_from_iret_i,16(%eax)
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
	pushl	%ebx			/* caller`s %ebx is in reg */
#else
	movl	4(%esp),%eax		/* get caller`s %ebx */
	pushl	%eax			/* push on stack */
#endif
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%esp			/* pass regs */
	call	_kttd_netentry		/* to kdb */
	addl	$4,%esp			/* pop parameters */
	popl	%edi			/* restore registers */
	popl	%esi
	popl	%ebp
#if	STAT_TIME
	popl	%ebx
#else
	popl	%eax
	movl	%eax,4(%esp)
#endif
	jmp	EXT(return_to_iret)	/* normal interrupt return */

ttd_from_iret_i:			/* on interrupt stack */
	pop	%edx			/* restore saved registers */
	pop	%ecx
	pop	%eax
	pushl	$0			/* zero error code */
	pushl	$0			/* zero trap number */
	pusha				/* save general registers */
	push	%ds			/* save segment registers */
	push	%es
	push	%fs
	push	%gs
	pushl	%esp			/* pass regs, */
	pushl	$0			/* code, */
	pushl	$-1			/* type to kdb */
	call	_kttd_trap
	addl	$12,%esp		/* remove parameters */
	pop	%gs			/* restore segment registers */
	pop	%fs
	pop	%es
	pop	%ds
	popa				/* restore general registers */
	addl	$8,%esp
	iret

#endif	/* MACH_TTD */

/*
 * System call enters through a call gate.  Flags are not saved -
 * we must shuffle stack to look like trap save area.
 *
 * esp->	old eip
 *		old cs
 *		old esp
 *		old ss
 *
 * eax contains system call number.
 */
ENTRY(syscall)
syscall_entry:
	pushf				/* save flags as soon as possible */
syscall_entry_2:
	cld				/* clear direction flag */

	pushl	%eax			/* save system call number */
	pushl	$0			/* clear trap number slot */

	pusha				/* save the general registers */
	pushl	%ds			/* and the segment registers */
	pushl	%es
	pushl	%fs
	pushl	%gs

	mov	%ss,%dx			/* switch to kernel data segment */
	mov	%dx,%ds
	mov	%dx,%es
	mov	%dx,%fs
	mov	%dx,%gs

/*
 * Shuffle eflags,eip,cs into proper places
 */

	movl	R_EIP(%esp),%ebx	/* eflags are in EIP slot */
	movl	R_CS(%esp),%ecx		/* eip is in CS slot */
	movl	R_EFLAGS(%esp),%edx	/* cs is in EFLAGS slot */
	movl	%ecx,R_EIP(%esp)	/* fix eip */
	movl	%edx,R_CS(%esp)		/* fix cs */
	movl	%ebx,R_EFLAGS(%esp)	/* fix eflags */

	CPU_NUMBER(%edx)
	TIME_TRAP_SENTRY

	movl	CX(EXT(kernel_stack),%edx),%ebx
					/* get current kernel stack */
	xchgl	%ebx,%esp		/* switch stacks - %ebx points to */
					/* user registers. */
					/* user regs pointer already set */

/*
 * Check for MACH or emulated system call
 */
syscall_entry_3:
	movl	CX(EXT(active_threads),%edx),%edx
					/* point to current thread */
	movl	TH_TASK(%edx),%edx	/* point to task */
	movl	TASK_EMUL(%edx),%edx	/* get emulation vector */
	orl	%edx,%edx		/* if none, */
	je	syscall_native		/*    do native system call */
	movl	%eax,%ecx		/* copy system call number */
	subl	DISP_MIN(%edx),%ecx	/* get displacement into syscall */
					/* vector table */
	jl	syscall_native		/* too low - native system call */
	cmpl	DISP_COUNT(%edx),%ecx	/* check range */
	jnl	syscall_native		/* too high - native system call */
	movl	DISP_VECTOR(%edx,%ecx,4),%edx
					/* get the emulation vector */
	orl	%edx,%edx		/* emulated system call if not zero */
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
	shll	$4,%eax			/* manual indexing */
	movl	EXT(mach_trap_table)(%eax),%ecx
					/* get number of arguments */
	jecxz	mach_call_call		/* skip argument copy if none */

	movl	R_UESP(%ebx),%esi	/* get user stack pointer */
	lea	4(%esi,%ecx,4),%esi	/* skip user return address, */
					/* and point past last argument */
	movl	$USER_DS,%edx		/* use user data segment for accesses */
	mov	%dx,%fs
	movl	%esp,%edx		/* save kernel ESP for error recovery */

0:	subl	$4,%esi
	RECOVER(mach_call_addr_push)
	pushl	%fs:(%esi)		/* push argument on stack */
	loop	0b			/* loop for all arguments */

mach_call_call:

#ifdef DEBUG
	testb	$0xff,EXT(syscall_trace)
	jz	0f
	pushl	%eax
	call	EXT(syscall_trace_print)
	/* will return with syscallofs still (or again) in eax */
	addl	$4,%esp
0:
#endif /* DEBUG */

	call	*EXT(mach_trap_table)+4(%eax)
					/* call procedure */
	movl	%esp,%ecx		/* get kernel stack */
	or	$(KERNEL_STACK_SIZE-1),%ecx
	movl	-3-IKS_SIZE(%ecx),%esp	/* switch back to PCB stack */
	movl	%eax,R_EAX(%esp)	/* save return value */
	jmp	_return_from_trap	/* return to user */

/*
 * Address out of range.  Change to page fault.
 * %esi holds failing address.
 */
mach_call_addr_push:
	movl	%edx,%esp		/* clean parameters from stack */
mach_call_addr:
	movl	%esi,R_CR2(%ebx)	/* set fault address */
	movl	$(T_PAGE_FAULT),R_TRAPNO(%ebx)
					/* set page-fault trap */
	movl	$(T_PF_USER),R_ERR(%ebx)
					/* set error code - read user space */
	jmp	_take_trap		/* treat as a trap */

/*
 * System call out of range.  Treat as invalid-instruction trap.
 * (? general protection?)
 */
mach_call_range:
	movl	$(T_INVALID_OPCODE),R_TRAPNO(%ebx)
					/* set invalid-operation trap */
	movl	$0,R_ERR(%ebx)		/* clear error code */
	jmp	_take_trap		/* treat as a trap */

/*
 * User space emulation of system calls.
 * edx - user address to handle syscall
 *
 * User stack will become:
 * uesp->	eflags
 *		eip
 * eax still contains syscall number.
 */
syscall_emul:
	movl	$USER_DS,%edi		/* use user data segment for accesses */
	mov	%di,%fs

/* XXX what about write-protected pages? */
	movl	R_UESP(%ebx),%edi	/* get user stack pointer */
	subl	$8,%edi			/* push space for new arguments */
	movl	R_EFLAGS(%ebx),%eax	/* move flags */
	RECOVER(syscall_addr)
	movl	%eax,%fs:0(%edi)	/* to user stack */
	movl	R_EIP(%ebx),%eax	/* move eip */
	RECOVER(syscall_addr)
	movl	%eax,%fs:4(%edi)	/* to user stack */
	movl	%edi,R_UESP(%ebx)	/* set new user stack pointer */
	movl	%edx,R_EIP(%ebx)	/* change return address to trap */
	movl	%ebx,%esp		/* back to PCB stack */
	jmp	_return_from_trap	/* return to user */

/*
 * Address error - address is in %edi.
 */
syscall_addr:
	movl	%edi,R_CR2(%ebx)	/* set fault address */
	movl	$(T_PAGE_FAULT),R_TRAPNO(%ebx)
					/* set page-fault trap */
	movl	$(T_PF_USER),R_ERR(%ebx)
					/* set error code - read user space */
	jmp	_take_trap		/* treat as a trap */


	.data
DATA(cpu_features)
	.long	0
	.text

/* Discover what kind of cpu we have; return the family number
   (3, 4, 5, 6, for 386, 486, 586, 686 respectively).  */
ENTRY(discover_x86_cpu_type)
	pushl	%ebp			/* Save frame pointer */
	movl	%esp,%ebp		/* Save stack pointer */
	and	$~0x3,%esp		/* Align stack pointer */

#ifdef	MACH_HYP
#warning Assuming not Cyrix CPU
#else	/* MACH_HYP */
	inb	$0xe8,%al		/* Enable ID flag for Cyrix CPU ... */
	andb	$0x80,%al		/* ... in CCR4 reg bit7 */
	outb	%al,$0xe8
#endif	/* MACH_HYP */

	pushfl				/* Fetch flags ... */
	popl	%eax			/*  ... into eax */
	movl	%eax,%ecx		/* Save original flags for return */
	xorl	$(EFL_AC+EFL_ID),%eax	/* Attempt to toggle ID and AC bits */
	pushl	%eax			/* Save flags... */
	popfl				/*  ... In EFLAGS */
	pushfl				/* Fetch flags back ... */
	popl	%eax			/*  ... into eax */
	pushl	%ecx			/* From ecx... */
	popfl				/* ... restore original flags */

	xorl	%ecx,%eax		/* See if any bits didn't change */
	testl	$EFL_AC,%eax		/* Test AC bit */
	jnz	0f			/* Skip next bit if AC toggled */
	movl	$3,%eax			/*   Return value is 386 */
	jmp	9f			/*   And RETURN */

0:	testl	$EFL_ID,%eax		/* Test ID bit */
	jnz	0f			/* Skip next bit if ID toggled */
	movl	$4,%eax			/*   Return value is 486 */
	jmp	9f			/*   And RETURN */

	/* We are a modern enough processor to have the CPUID instruction;
	   use it to find out what we are. */
0:	movl	$1,%eax			/* Fetch CPU type info ... */
	cpuid				/*  ... into eax */
	movl	%edx,cpu_features	/* Keep a copy */
	shrl	$8,%eax			/* Slide family bits down */
	andl	$15,%eax		/* And select them */

9:	movl	%ebp,%esp		/* Restore stack pointer */
	popl	%ebp			/* Restore frame pointer */
	ret				/* And return */


/**/
/*
 * Utility routines.
 */

/*
 * Copy from user address space.
 * arg0:	user address
 * arg1:	kernel address
 * arg2:	byte count
 */
ENTRY(copyin)
Entry(copyinmsg)
	pushl	%esi
	pushl	%edi			/* save registers */

	movl	8+S_ARG0,%esi		/* get user start address */
	movl	8+S_ARG1,%edi		/* get kernel destination address */
	movl	8+S_ARG2,%edx		/* get count */

	movl	$USER_DS,%eax		/* use user data segment for accesses */
	mov	%ax,%ds

	/*cld*/				/* count up: default mode in all GCC code */
	movl	%edx,%ecx		/* move by longwords first */
	shrl	$2,%ecx
	RECOVER(copyin_fail)
	rep
	movsl				/* move longwords */
	movl	%edx,%ecx		/* now move remaining bytes */
	andl	$3,%ecx
	RECOVER(copyin_fail)
	rep
	movsb
	xorl	%eax,%eax		/* return 0 for success */

copyin_ret:
	mov	%ss,%di			/* restore DS to kernel segment */
	mov	%di,%ds

	popl	%edi			/* restore registers */
	popl	%esi
	ret				/* and return */

copyin_fail:
	movl	$1,%eax			/* return 1 for failure */
	jmp	copyin_ret		/* pop frame and return */

/*
 * Copy to user address space.
 * arg0:	kernel address
 * arg1:	user address
 * arg2:	byte count
 */
ENTRY(copyout)
Entry(copyoutmsg)
	pushl	%esi
	pushl	%edi			/* save registers */

	movl	8+S_ARG0,%esi		/* get kernel start address */
	movl	8+S_ARG1,%edi		/* get user start address */
	movl	8+S_ARG2,%edx		/* get count */

	movl	$USER_DS,%eax		/* use user data segment for accesses */
	mov	%ax,%es

/*
 * Check whether user address space is writable
 * before writing to it - hardware is broken.
 * XXX only have to do this on 386's.
 */
copyout_retry:
#ifdef	MACH_HYP
	movl	cr3,%ecx		/* point to page directory */
#else	/* MACH_HYP */
	movl	%cr3,%ecx		/* point to page directory */
#endif	/* MACH_HYP */
#if PAE
	movl	%edi,%eax		/* get page directory pointer bits */
	shrl	$(PDPSHIFT),%eax	/* from user address */
	movl	KERNELBASE(%ecx,%eax,PTE_SIZE),%ecx
					/* get page directory pointer */
#ifdef	MACH_PSEUDO_PHYS
	shrl	$(PTESHIFT),%ecx
	movl	pfn_list,%eax
	movl	(%eax,%ecx,4),%ecx	/* mfn_to_pfn */
	shll	$(PTESHIFT),%ecx
#else	/* MACH_PSEUDO_PHYS */
	andl	$(PTE_PFN),%ecx		/* isolate page frame address */
#endif	/* MACH_PSEUDO_PHYS */
#endif	/* PAE */
	movl	%edi,%eax		/* get page directory bits */
	shrl	$(PDESHIFT),%eax	/* from user address */
#if PAE
	andl	$(PDEMASK),%eax
#endif	/* PAE */
	movl	KERNELBASE(%ecx,%eax,PTE_SIZE),%ecx
					/* get page directory pointer */
	testl	$(PTE_V),%ecx		/* present? */
	jz	0f			/* if not, fault is OK */
#ifdef	MACH_PSEUDO_PHYS
	shrl	$(PTESHIFT),%ecx
	movl	pfn_list,%eax
	movl	(%eax,%ecx,4),%ecx	/* mfn_to_pfn */
	shll	$(PTESHIFT),%ecx
#else	/* MACH_PSEUDO_PHYS */
	andl	$(PTE_PFN),%ecx		/* isolate page frame address */
#endif	/* MACH_PSEUDO_PHYS */
	movl	%edi,%eax		/* get page table bits */
	shrl	$(PTESHIFT),%eax
	andl	$(PTEMASK),%eax		/* from user address */
	leal	KERNELBASE(%ecx,%eax,PTE_SIZE),%ecx
					/* point to page table entry */
	movl	(%ecx),%eax		/* get it */
	testl	$(PTE_V),%eax		/* present? */
	jz	0f			/* if not, fault is OK */
	testl	$(PTE_W),%eax		/* writable? */
	jnz	0f			/* OK if so */
/*
 * Not writable - must fake a fault.  Turn off access to the page.
 */
#ifdef	MACH_HYP
	pushl	%edx
	pushl	%ecx
	call	hyp_invalidate_pte
	popl	%ecx
	popl	%edx
#else	/* MACH_HYP */
	andl	$(PTE_INVALID),(%ecx)	/* turn off valid bit */
	movl	%cr3,%eax		/* invalidate TLB */
	movl	%eax,%cr3
#endif	/* MACH_HYP */
0:

/*
 * Copy only what fits on the current destination page.
 * Check for write-fault again on the next page.
 */
	leal	NBPG(%edi),%eax		/* point to */
	andl	$(-NBPG),%eax		/* start of next page */
	subl	%edi,%eax		/* get number of bytes to that point */
	cmpl	%edx,%eax		/* bigger than count? */
	jle	1f			/* if so, */
	movl	%edx,%eax		/* use count */
1:

	/*cld*/				/* count up: always this way in GCC code */
	movl	%eax,%ecx		/* move by longwords first */
	shrl	$2,%ecx
	RECOVER(copyout_fail)
	RETRY(copyout_retry)
	rep
	movsl
	movl	%eax,%ecx		/* now move remaining bytes */
	andl	$3,%ecx
	RECOVER(copyout_fail)
	RETRY(copyout_retry)
	rep
	movsb				/* move */
	subl	%eax,%edx		/* decrement count */
	jg	copyout_retry		/* restart on next page if not done */
	xorl	%eax,%eax		/* return 0 for success */

copyout_ret:
	mov	%ss,%di			/* restore ES to kernel segment */
	mov	%di,%es

	popl	%edi			/* restore registers */
	popl	%esi
	ret				/* and return */

copyout_fail:
	movl	$1,%eax			/* return 1 for failure */
	jmp	copyout_ret		/* pop frame and return */

/*
 * int inst_fetch(int eip, int cs);
 *
 * Fetch instruction byte.  Return -1 if invalid address.
 */
ENTRY(inst_fetch)
	movl	S_ARG1, %eax		/* get segment */
	movw	%ax,%fs			/* into FS */
	movl	S_ARG0, %eax		/* get offset */
	RETRY(EXT(inst_fetch))		/* re-load FS on retry */
	RECOVER(_inst_fetch_fault)
	movzbl	%fs:(%eax),%eax		/* load instruction byte */
	ret

_inst_fetch_fault:
	movl	$-1,%eax		/* return -1 if error */
	ret


/*
 * Done with recovery and retry tables.
 */
	RECOVER_TABLE_END
	RETRY_TABLE_END



ENTRY(dr6)
#ifdef MACH_XEN
	pushl	%ebx
	movl	$6, %ebx
	call	__hyp_get_debugreg
	popl	%ebx
#else /* MACH_XEN */
	movl	%db6, %eax
#endif /* MACH_XEN */
	ret

/*	dr<i>(address, type, len, persistence)
 */
ENTRY(dr0)
	movl	S_ARG0, %eax
	movl	%eax,EXT(dr_addr)
#ifdef MACH_XEN
	pushl	%ebx
	movl	$0,%ebx
	movl	%eax,%ecx
	call	__hyp_set_debugreg
#else /* MACH_XEN */
	movl	%eax, %db0
#endif /* MACH_XEN */
	movl	$0, %ecx
	jmp	0f
ENTRY(dr1)
	movl	S_ARG0, %eax
	movl	%eax,EXT(dr_addr)+1*4
#ifdef MACH_XEN
	pushl	%ebx
	movl	$1,%ebx
	movl	%eax,%ecx
	call	__hyp_set_debugreg
#else /* MACH_XEN */
	movl	%eax, %db1
#endif /* MACH_XEN */
	movl	$2, %ecx
	jmp	0f
ENTRY(dr2)
	movl	S_ARG0, %eax
	movl	%eax,EXT(dr_addr)+2*4
#ifdef MACH_XEN
	pushl	%ebx
	movl	$2,%ebx
	movl	%eax,%ecx
	call	__hyp_set_debugreg
#else /* MACH_XEN */
	movl	%eax, %db2
#endif /* MACH_XEN */
	movl	$4, %ecx
	jmp	0f

ENTRY(dr3)
	movl	S_ARG0, %eax
	movl	%eax,EXT(dr_addr)+3*4
#ifdef MACH_XEN
	pushl	%ebx
	movl	$3,%ebx
	movl	%eax,%ecx
	call	__hyp_set_debugreg
#else /* MACH_XEN */
	movl	%eax, %db3
#endif /* MACH_XEN */
	movl	$6, %ecx

0:
	pushl	%ebp
	movl	%esp, %ebp

#ifdef MACH_XEN
	movl	$7,%ebx
	call	__hyp_get_debugreg
	movl	%eax, %edx
#else /* MACH_XEN */
	movl	%db7, %edx
#endif /* MACH_XEN */
	movl	%edx,EXT(dr_addr)+4*4
	andl	dr_msk(,%ecx,2),%edx	/* clear out new entry */
	movl	%edx,EXT(dr_addr)+5*4
	movzbl	B_ARG3, %eax
	andb	$3, %al
	shll	%cl, %eax
	orl	%eax, %edx

	movzbl	B_ARG1, %eax
	andb	$3, %al
	addb	%cl, %cl
	addb	$0x10, %cl
	shll	%cl, %eax
	orl	%eax, %edx

	movzbl	B_ARG2, %eax
	andb	$3, %al
	addb	$0x2, %cl
	shll	%cl, %eax
	orl	%eax, %edx

#ifdef MACH_XEN
	movl	$7,%ebx
	movl	%edx, %ecx
	call	__hyp_set_debugreg
	popl	%ebx
#else /* MACH_XEN */
	movl	%edx, %db7
#endif /* MACH_XEN */
	movl	%edx,EXT(dr_addr)+7*4
	movl	%edx, %eax
	leave
	ret

	.data
dr_msk:
	.long	~0x000f0003
	.long	~0x00f0000c
	.long	~0x0f000030
	.long	~0xf00000c0
ENTRY(dr_addr)
	.long	0,0,0,0
	.long	0,0,0,0
	.text

/*
 * cpu_shutdown()
 * Force reboot
 */
null_idt:
	.space	8 * 32

null_idtr:
        .word   8 * 32 - 1
        .long   null_idt

Entry(cpu_shutdown)
        lidt    null_idtr       /* disable the interrupt handler */
        xor     %ecx,%ecx       /* generate a divide by zero */
        div     %ecx,%eax       /* reboot now */
        ret                     /* this will "never" be executed */
