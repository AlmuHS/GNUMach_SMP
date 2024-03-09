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
#include <i386/i386/gdt.h>
#include <i386/i386/ldt.h>
#include <i386/i386/msr.h>
#include <i386/i386/i386asm.h>
#include <i386/i386/cpu_number.h>
#include <i386/i386/xen.h>


/*
 * Helpers for thread state as saved in the pcb area, during trap or irq handling
 */
#define pusha		\
	pushq %rax	;\
	pushq %rcx	;\
	pushq %rdx	;\
	pushq %rbx	;\
	subq $8,%rsp	;\
	pushq %rbp	;\
	pushq %rsi	;\
	pushq %rdi	;\
	pushq %r8	;\
	pushq %r9	;\
	pushq %r10	;\
	pushq %r11	;\
	pushq %r12	;\
	pushq %r13	;\
	pushq %r14	;\
	pushq %r15

#define popa		\
	popq %r15	;\
	popq %r14	;\
	popq %r13	;\
	popq %r12	;\
	popq %r11	;\
	popq %r10	;\
	popq %r9	;\
	popq %r8	;\
	popq %rdi	;\
	popq %rsi	;\
	popq %rbp	;\
	addq $8,%rsp	;\
	popq %rbx	;\
	popq %rdx	;\
	popq %rcx	;\
	popq %rax

#define PUSH_REGS_ISR	\
	pushq	%rcx    ;\
	pushq	%rdx    ;\
	pushq	%rsi    ;\
	pushq	%rdi    ;\
	pushq	%r8     ;\
	pushq	%r9     ;\
	pushq	%r10     ;\
	pushq	%r11

#define PUSH_AREGS_ISR	\
	pushq	%rax    ;\
	PUSH_REGS_ISR


#define POP_REGS_ISR	\
	popq	%r11    ;\
	popq	%r10     ;\
	popq	%r9     ;\
	popq	%r8     ;\
	popq	%rdi    ;\
	popq	%rsi    ;\
	popq	%rdx    ;\
	popq	%rcx

#define POP_AREGS_ISR	\
	POP_REGS_ISR	;\
	popq	%rax

/*
 * Note that we have to load the kernel segment registers even if this
 * is a trap from the kernel, because the kernel uses user segment
 * registers for copyin/copyout.
 * (XXX Would it be smarter just to use fs or gs for that?)
 */
#ifdef USER32
#define PUSH_SEGMENTS(reg)	\
	movq	%ds,reg		;\
	pushq	reg		;\
	movq	%es,reg		;\
	pushq	reg		;\
	pushq	%fs		;\
	pushq	%gs
#else
#define PUSH_SEGMENTS(reg)
#endif

#ifdef USER32
#define POP_SEGMENTS(reg)	\
	popq	%gs		;\
	popq	%fs		;\
	popq	reg		;\
	movq	reg,%es		;\
	popq	reg		;\
	movq	reg,%ds
#else
#define POP_SEGMENTS(reg)
#endif

#ifdef USER32
#define PUSH_SEGMENTS_ISR(reg)	\
	movq	%ds,reg		;\
	pushq	reg		;\
	movq	%es,reg		;\
	pushq	reg		;\
	pushq	%fs		;\
	pushq	%gs
#else
#define PUSH_SEGMENTS_ISR(reg)
#endif

#ifdef USER32
#define POP_SEGMENTS_ISR(reg)	\
	popq	%gs		;\
	popq	%fs		;\
	popq	reg		;\
	movq	reg,%es		;\
	popq	reg		;\
	movq	reg,%ds
#else
#define POP_SEGMENTS_ISR(reg)
#endif

#if NCPUS > 1
#define SET_KERNEL_SEGMENTS(reg)                \
	ud2		/* TODO: use swapgs or similar */
#else // NCPUS > 1
#ifdef USER32
#define SET_KERNEL_SEGMENTS(reg)	       \
	mov	%ss,reg /* switch to kernel segments */ ;\
	mov	reg,%ds	/* (same as kernel stack segment) */ ;\
	mov	reg,%es			;\
	mov	reg,%fs			;\
	mov	reg,%gs
#else // USER32
#define SET_KERNEL_SEGMENTS(reg)
#endif // USER32
#endif // NCPUS > 1

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
	pushf					/* Save flags */	;\
	cli					/* block interrupts */	;\
	movl	VA_ETC,%ebx			/* get timer value */	;\
	movl	CX(EXT(current_tstamp),%rdx),%ecx	/* get old time stamp */;\
	movl	%ebx,CX(EXT(current_tstamp),%rdx)	/* set new time stamp */;\
	subl	%ecx,%ebx			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%rdx),%ecx	/* get current timer */	;\
	addl	%ebx,LOW_BITS(%ecx)		/* add to low bits */	;\
	jns	0f				/* if overflow, */	;\
	call	timer_normalize			/* normalize timer */	;\
0:	addl	$(TH_SYSTEM_TIMER-TH_USER_TIMER),%ecx			;\
						/* switch to sys timer */;\
	movl	%ecx,CX(EXT(current_timer),%rdx)	/* make it current */	;\
	popf					/* allow interrupts */

/*
 * Update time on system call entry.
 * 11 instructions (including cli on entry)
 * Assumes CPU number in %edx.
 * Uses %ebx, %ecx.
 * Same as TIME_TRAP_UENTRY, but preserves %eax.
 */
#define	TIME_TRAP_SENTRY \
	pushf					/* Save flags */	;\
	cli					/* block interrupts */	;\
	movl	VA_ETC,%ebx			/* get timer value */	;\
	movl	CX(EXT(current_tstamp),%rdx),%ecx	/* get old time stamp */;\
	movl	%ebx,CX(EXT(current_tstamp),%rdx)	/* set new time stamp */;\
	subl	%ecx,%ebx			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%rdx),%ecx	/* get current timer */	;\
	addl	%ebx,LOW_BITS(%ecx)		/* add to low bits */	;\
	jns	0f				/* if overflow, */	;\
	pushq	%rax				/* save %rax */		;\
	call	timer_normalize			/* normalize timer */	;\
	popq	%rax				/* restore %rax */	;\
0:	addl	$(TH_SYSTEM_TIMER-TH_USER_TIMER),%ecx			;\
						/* switch to sys timer */;\
	movl	%ecx,CX(EXT(current_timer),%rdx)	/* make it current */	;\
	popf					/* allow interrupts */

/*
 * update time on user trap exit.
 * 10 instructions.
 * Assumes CPU number in %edx.
 * Uses %ebx, %ecx.
 */
#define	TIME_TRAP_UEXIT \
	cli					/* block interrupts */	;\
	movl	VA_ETC,%ebx			/* get timer */		;\
	movl	CX(EXT(current_tstamp),%rdx),%ecx	/* get old time stamp */;\
	movl	%ebx,CX(EXT(current_tstamp),%rdx)	/* set new time stamp */;\
	subl	%ecx,%ebx			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%rdx),%ecx	/* get current timer */	;\
	addl	%ebx,LOW_BITS(%ecx)		/* add to low bits */	;\
	jns	0f				/* if overflow,	*/	;\
	call	timer_normalize			/* normalize timer */	;\
0:	addl	$(TH_USER_TIMER-TH_SYSTEM_TIMER),%ecx			;\
						/* switch to user timer	*/;\
	movl	%ecx,CX(EXT(current_timer),%rdx)	/* make it current */

/*
 * update time on interrupt entry.
 * 9 instructions.
 * Assumes CPU number in %edx.
 * Leaves old timer in %ebx.
 * Uses %ecx.
 */
#define	TIME_INT_ENTRY \
	movl	VA_ETC,%ecx			/* get timer */		;\
	movl	CX(EXT(current_tstamp),%rdx),%ebx	/* get old time stamp */;\
	movl	%ecx,CX(EXT(current_tstamp),%rdx)	/* set new time stamp */;\
	subl	%ebx,%ecx			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%rdx),%ebx	/* get current timer */	;\
	addl	%ecx,LOW_BITS(%ebx)		/* add to low bits */	;\
	leal	CX(0,%rdx),%ecx			/* timer is 16 bytes */	;\
	lea	CX(EXT(kernel_timer),%rdx),%ecx	/* get interrupt timer*/;\
	movl	%ecx,CX(EXT(current_timer),%rdx)	/* set timer */

/*
 * update time on interrupt exit.
 * 11 instructions
 * Assumes CPU number in %edx, old timer in %ebx.
 * Uses %eax, %ecx.
 */
#define	TIME_INT_EXIT \
	movl	VA_ETC,%eax			/* get timer */		;\
	movl	CX(EXT(current_tstamp),%rdx),%ecx	/* get old time stamp */;\
	movl	%eax,CX(EXT(current_tstamp),%rdx)	/* set new time stamp */;\
	subl	%ecx,%eax			/* elapsed = new-old */	;\
	movl	CX(EXT(current_timer),%rdx),%ecx	/* get current timer */	;\
	addl	%eax,LOW_BITS(%ecx)		/* add to low bits */	;\
	jns	0f				/* if overflow, */	;\
	call	timer_normalize			/* normalize timer */	;\
0:	testb	$0x80,LOW_BITS+3(%ebx)		/* old timer overflow? */;\
	jz	0f				/* if overflow, */	;\
	movl	%ebx,%ecx			/* get old timer */	;\
	call	timer_normalize			/* normalize timer */	;\
0:	movl	%ebx,CX(EXT(current_timer),%rdx)	/* set timer */


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
	movl	CX(EXT(current_tstamp),%rdx),%eax	/* get old time stamp  */
	movl	%ecx,CX(EXT(current_tstamp),%rdx)	/* set new time stamp */
	subl	%ecx,%eax			/* elapsed = new - old */
	movl	CX(EXT(current_timer),%rdx),%ecx	/* get current timer */
	addl	%eax,LOW_BITS(%ecx)		/* add to low bits */
	jns	0f				/* if overflow, */
	call	timer_normalize			/* normalize timer */
0:
	movl	S_ARG0,%ecx			/* get new timer */
	movl	%ecx,CX(EXT(current_timer),%rdx)	/* set timer */
	ret

/*
 * Initialize the first timer for a CPU.
 */
ENTRY(start_timer)
	CPU_NUMBER(%edx)			/* get this CPU */
	movl	VA_ETC,%ecx			/* get timer */
	movl	%ecx,CX(EXT(current_tstamp),%rdx)	/* set initial time stamp */
	movl	S_ARG0,%ecx			/* get timer */
	movl	%ecx,CX(EXT(current_timer),%rdx)	/* set initial timer */
	ret

#endif	/* accurate timing */

/**/

/*
 * Trap/interrupt entry points.
 *
 * All traps must create the i386_saved_state struct on the stack on
 * entry. Note that:
 *      - CR2 is only used if the trap is a page fault
 *      - user_rsp/user_ss are only used if entering from user space
 *      - v86_regs are used only from V86 threads
 *         (TODO check if V86 is still used with USER32)
 *
 * Depending the CPL before entry, the stack might be switched or not;
 * if entering from user-space the CPU loads TSS->RSP0 in RSP,
 * otherwise RSP is unchanged. After this, the cpu pushes
 * SS/RSP/RFLAFS/CS/RIP and optionally ErrorCode and executes the handler.
 */

/* Try to save/show some information when a double fault happens
 * We can't recover to a working state, so if we have a debugger wait for it,
 * otherwise reset */
ENTRY(t_dbl_fault)
	INT_FIX
	cli	/* disable interrupts that might corrupt the state*/
	pusha
	movq	%cr2,%rax
	movq	%rax,R_CR2-R_R15(%rsp)	/* CR2 might contain the faulting address */
	subq	$48,%rsp	// FIXME remove when segments are cleaned up
	movq	%rsp,%rdi		/* pass the saved state */
	call	handle_double_fault
	jmp	cpu_shutdown	/* reset */
END(t_dbl_fault)

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
#ifdef USER32
	testq	$(EFL_VM),32(%rsp)	/* is trap from V86 mode? */
	jnz	EXT(alltraps)		/* isn`t kernel trap if so */
#endif
	/* Note: handling KERNEL_RING value by hand */
	testq	$2,24(%rsp)		/* is trap from kernel mode? */
	jnz	EXT(alltraps)		/* if so:  */
					/* check for the kernel exit sequence */
	cmpq	$_kret_iret,16(%rsp)	/* on IRET? */
	je	fault_iret
#ifdef USER32
	cmpq	$_kret_popl_ds,16(%rsp)	/* popping DS? */
	je	fault_popl_ds
	cmpq	$_kret_popl_es,16(%rsp)	/* popping ES? */
	je	fault_popl_es
	cmpq	$_kret_popl_fs,16(%rsp)	/* popping FS? */
	je	fault_popl_fs
	cmpq	$_kret_popl_gs,16(%rsp)	/* popping GS? */
	je	fault_popl_gs
#endif
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

#ifdef USER32
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
	movq	%es,%rcx
	pushq	%rcx			/* restore es, */
push_fs:
	pushq	%fs			/* restore fs, */
push_gs:
	pushq	%gs			/* restore gs. */
push_gsbase:
	pushq	$0
	pushq	$0
#endif
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
#ifdef USER32
	testq	$(EFL_VM),16(%rsp)	/* is trap from V86 mode? */
	jnz	0f			/* isn`t kernel trap if so */
#endif
	/* Note: handling KERNEL_RING value by hand */
	testq	$2,8(%rsp)		/* is trap from kernel mode? */
	jnz	0f			/* if so: */
#ifdef USER32
	cmpq	$syscall_entry,(%rsp)	/* system call entry? */
	jne	0f			/* if so: */
					/* flags are sitting where syscall */
					/* wants them */
	addq	$32,%rsp		/* remove eip/cs */
	jmp	syscall_entry_2		/* continue system call entry */
#else
	// TODO: implement the 64-bit case
	ud2
#endif
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
	PUSH_SEGMENTS(%rax)		/* and the segment registers */
	SET_KERNEL_SEGMENTS(%rax)	/* switch to kernel data segment */
trap_set_segs:
	cld				/* clear direction flag */
#ifdef USER32
	testl	$(EFL_VM),R_EFLAGS(%rsp) /* in V86 mode? */
	jnz	trap_from_user		/* user mode trap if so */
#endif
	/* Note: handling KERNEL_RING value by hand */
	testb	$2,R_CS(%rsp)		/* user mode trap? */
	jz	trap_from_kernel	/* kernel trap if not */
trap_from_user:

	CPU_NUMBER(%edx)
	TIME_TRAP_UENTRY

	movq	CX(EXT(kernel_stack),%rdx),%rbx
	xchgq	%rbx,%rsp		/* switch to kernel stack */
					/* user regs pointer already set */
_take_trap:
	movq	%rbx,%rdi		/* pass register save area to trap */
	call	EXT(user_trap)		/* call user trap routine */
#ifdef USER32
	orq     %rax,%rax               /* emulated syscall? */
	jz      1f			/* no, just return */
	movq    R_EAX(%rbx),%rax        /* yes, get syscall number */
	jmp     syscall_entry_3         /* and emulate it */
#endif
1:
	movq	(%rsp),%rsp		/* switch back to PCB stack */

/*
 * Return from trap or system call, checking for ASTs.
 * On PCB stack.
 */

_return_from_trap:
	CPU_NUMBER(%edx)
	cmpl	$0,CX(EXT(need_ast),%rdx)
	jz	_return_to_user		/* if we need an AST: */

	movq	CX(EXT(kernel_stack),%rdx),%rsp
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
#ifdef USER32
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
#endif
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

	CPU_NUMBER(%ecx)
	and	$(~(INTSTACK_SIZE-1)),%rdx
	cmpq	CX(EXT(int_stack_base),%rcx),%rdx
	je	1f			/* OK if so */

	movl	%ecx,%edx
	cmpq	CX(EXT(kernel_stack),%rdx),%rsp
					/* already on kernel stack? */
	ja	0f
	cmpq	MY(ACTIVE_STACK),%rsp
	ja	1f			/* switch if not */
0:
	movq	CX(EXT(kernel_stack),%rdx),%rsp
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
	or	$(KERNEL_STACK_SIZE-1),%rcx
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
	or	$(KERNEL_STACK_SIZE-1),%rcx
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

/* IOAPIC has 24 interrupts, put spurious in the same array */

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
/* Legacy APIC interrupts or PIC interrupts */
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
#ifdef APIC
/* APIC PCI interrupts PIRQ A-H */
INTERRUPT(16)
INTERRUPT(17)
INTERRUPT(18)
INTERRUPT(19)
INTERRUPT(20)
INTERRUPT(21)
INTERRUPT(22)
INTERRUPT(23)
#endif
#if NCPUS > 1
INTERRUPT(CALL_AST_CHECK)
INTERRUPT(CALL_PMAP_UPDATE)
#endif
#ifdef APIC
/* Spurious interrupt, set irq number to vect number */
INTERRUPT(255)
#endif

/* XXX handle NMI - at least print a warning like Linux does.  */

/*
 * All interrupts enter here. The cpu might have loaded a new RSP,
 * depending on the previous CPL, as in alltraps.
 * Old %eax on stack, interrupt number in %eax; we need to fill the remaining
 * fields of struct i386_interrupt_state, which might be in the pcb or in the
 * interrupt stack.
 */
ENTRY(all_intrs)
	PUSH_REGS_ISR			/* save registers */
	cld				/* clear direction flag */

	PUSH_SEGMENTS_ISR(%rdx)		/* save segment registers */

	CPU_NUMBER_NO_GS(%ecx)
	movq	%rsp,%rdx		/* on an interrupt stack? */
	and	$(~(INTSTACK_SIZE-1)),%rdx
	cmpq	%ss:CX(EXT(int_stack_base),%rcx),%rdx
	je	int_from_intstack	/* if not: */

	SET_KERNEL_SEGMENTS(%rdx)	/* switch to kernel segments */

	CPU_NUMBER(%edx)

	movq	CX(EXT(int_stack_top),%rdx),%rcx

	xchgq	%rcx,%rsp		/* switch to interrupt stack */

#if	STAT_TIME
	pushq	%rcx			/* save pointer to old stack */
#else
	pushq	%rbx			/* save %ebx - out of the way */
					/* so stack looks the same */
	pushq	%rcx			/* save pointer to old stack */
	TIME_INT_ENTRY			/* do timing */
#endif

#ifdef MACH_LDEBUG
	incl	CX(EXT(in_interrupt),%rdx)
#endif

	call	EXT(interrupt)		/* call generic interrupt routine */
	.globl	EXT(return_to_iret)	/* ( label for kdb_kintr and hardclock */
LEXT(return_to_iret)			/* to find the return from calling interrupt) */

	CPU_NUMBER(%edx)
#ifdef MACH_LDEBUG
	decl	CX(EXT(in_interrupt),%rdx)
#endif

#if	STAT_TIME
#else
	TIME_INT_EXIT			/* do timing */
	movq	8(%rsp),%rbx		/* restore the extra reg we saved */
#endif

	popq	%rsp			/* switch back to old stack */

#ifdef USER32
	testl	$(EFL_VM),I_EFL(%rsp)	/* if in V86 */
	jnz	0f			/* or */
#endif
	/* Note: handling KERNEL_RING value by hand */
	testb	$2,I_CS(%rsp)		/* user mode, */
	jz	1f			/* check for ASTs */
0:
	cmpq	$0,CX(EXT(need_ast),%rdx)
	jnz	ast_from_interrupt	/* take it if so */
1:
	POP_SEGMENTS_ISR(%rdx)		/* restore segment regs */
	POP_AREGS_ISR			/* restore registers */

	iretq				/* return to caller */

int_from_intstack:
	CPU_NUMBER_NO_GS(%edx)
	cmpq	CX(EXT(int_stack_base),%rdx),%rsp /* seemingly looping? */
	jb	stack_overflowed	/* if not: */
	call	EXT(interrupt)		/* call interrupt routine */
_return_to_iret_i:			/* ( label for kdb_kintr) */
	POP_SEGMENTS_ISR(%rdx)
	POP_AREGS_ISR			/* restore registers */
					/* no ASTs */

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
	POP_SEGMENTS_ISR(%rdx)		/* restore all registers ... */
	POP_AREGS_ISR
	pushq	$0			/* zero code */
	pushq	$0			/* zero trap number */
	pusha				/* save general registers */
	PUSH_SEGMENTS_ISR(%rdx)		/* save segment registers */
	SET_KERNEL_SEGMENTS(%rdx)	/* switch to kernel segments */
	CPU_NUMBER(%edx)
	TIME_TRAP_UENTRY

	movq	CX(EXT(kernel_stack),%rdx),%rsp
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
 *		saved SPL
 *		saved IRQ
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

#define	RET_OFFSET	32


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
	testq	%rax,%rax
	jnz	0b
	ud2				/* oops, didn't find frame, fix me :/ */

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
	PUSH_SEGMENTS(%rdx)		/* save segment registers */
	movq	%rsp,%rdx		/* pass regs, */
	movq	$0,%rsi			/* code, */
	movq	$-1,%rdi		/* type to kdb */
	call	EXT(kdb_trap)
	POP_SEGMENTS(%rdx)		/* restore segment registers */
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
	PUSH_SEGMENTS_ISR(%rdx)		/* save segment registers */
	ud2	// TEST it
	movq	%rsp,%rdx		/* pass regs, */
	movq	$0,%rsi			/* code, */
	movq	$-1,%rdi		/* type to kdb */
	call	_kttd_trap
	POP_SEGMENTS_ISR(%rdx)		/* restore segment registers */
	popa				/* restore general registers */
	addq	$16,%rsp

// TODO: test it before dropping ud2
movq (%rsp),%rax
ud2
	iretq

#endif	/* MACH_TTD */

#ifdef USER32
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

	pusha				/* save the general registers */
	PUSH_SEGMENTS(%rdx)		/* and the segment registers */
	SET_KERNEL_SEGMENTS(%rdx)	/* switch to kernel data segment */

/*
 * Shuffle eflags,eip,cs into proper places
 */

	movq	R_EIP(%rsp),%rbx	/* eflags are in EIP slot */
	movq	R_CS(%rsp),%rcx		/* eip is in CS slot */
	movq	R_EFLAGS(%rsp),%rdx	/* cs is in EFLAGS slot */
	movq	%rcx,R_EIP(%rsp)	/* fix eip */
	movq	%rdx,R_CS(%rsp)		/* fix cs */
	movq	%rbx,R_EFLAGS(%rsp)	/* fix eflags */

	CPU_NUMBER_NO_STACK(%edx)
	TIME_TRAP_SENTRY

	movq	CX(EXT(kernel_stack),%rdx),%rbx
					/* get current kernel stack */
	xchgq	%rbx,%rsp		/* switch stacks - %ebx points to */
					/* user registers. */
					/* user regs pointer already set */

/*
 * Check for MACH or emulated system call
 */
syscall_entry_3:
	movq	MY(ACTIVE_THREAD),%rdx
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
	shll	$5,%eax			/* manual indexing of mach_trap_t */
	xorq	%r10,%r10
	mov	EXT(mach_trap_table)(%rax),%r10
					/* get number of arguments */
	andq	%r10,%r10
	jz	mach_call_call		/* skip argument copy if none */

	movq	$USER_DS,%rdx		/* use user data segment for accesses */
	mov	%dx,%fs
	movq	%rsp,%r11		/* save kernel ESP for error recovery */

	movq	R_UESP(%rbx),%rbp	/* get user stack pointer */
	addq	$4,%rbp			/* Skip user return address */

	movq	$VM_MAX_ADDRESS, %rcx
	cmpq	%rcx,%rbp		/* Check segment limit by hand */
	jae	mach_call_addr_push

#define PARAM(reg,ereg) \
	xorq	%reg,%reg		;\
	RECOVER(mach_call_addr_push) \
	movl	%fs:(%rbp),%ereg	/* 1st parameter */ ;\
	addq	$4,%rbp			;\
	dec	%r10			;\
	jz	mach_call_call

	PARAM(rdi,edi)			/* 1st parameter */
	PARAM(rsi,esi)			/* 2nd parameter */
	PARAM(rdx,edx)			/* 3rd parameter */
	PARAM(rcx,ecx)			/* 4th parameter */
	PARAM(r8,r8d)			/* 5th parameter */
	PARAM(r9,r9d)			/* 6th parameter */

	lea	(%rbp,%r10,4),%rbp	/* point past last argument */
	xorq	%r12,%r12

0:	subq	$4,%rbp
	RECOVER(mach_call_addr_push)
	movl	%fs:(%rbp),%r12d
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
	call	*EXT(mach_trap_table)+8(%rax)  /* call procedure */
	movq	%rsp,%rcx		/* get kernel stack */
	or	$(KERNEL_STACK_SIZE-1),%rcx
	movq	-7-IKS_SIZE(%rcx),%rsp	/* switch back to PCB stack */
	movq	%rax,R_EAX(%rsp)	/* save return value */
	jmp	_return_from_trap	/* return to user */

/*
 * Address out of range.  Change to page fault.
 * %rbp holds failing address.
 */
mach_call_addr_push:
	movq	%r11,%rsp		/* clean parameters from stack */
mach_call_addr:
	movq	%rbp,R_CR2(%rbx)	/* set fault address */
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
	movq	$VM_MAX_ADDRESS, %rax
	cmpq	%rax,%rdi		/* Check segment limit by hand */
	jae	syscall_addr
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
END(syscall)

#else /* USER32 */

/* Entry point for 64-bit syscalls.
 * On entry we're still on the user stack, so better not use it. Instead we
 * save the thread state immediately in thread->pcb->iss, then try to invoke
 * the syscall.
 * Note: emulated syscalls seem to not be used anymore in GNU/Hurd, so they
 * are not handled here.
 * TODO:
     - for now we assume the return address is canonical, but apparently there
       can be cases where it's not (see how Linux handles this). Does it apply
       here?
     - check that the case where a task is suspended, and later returns via
       iretq from return_from_trap, works fine in all combinations
 */
ENTRY(syscall64)
	/* RFLAGS[32:63] are reserved, so combine syscall num (32 bit) and
	 * eflags in RAX to allow using r11 as temporary register
	 */
	shlq	$32,%r11
	shlq	$32,%rax	/* make sure bits 32:63 of %rax are zero */
	shrq	$32,%rax
	or	%r11,%rax

	/* Save thread state in pcb->iss, as on exception entry.
	 * Since this is triggered synchronously from userspace, we could
	 * save only the callee-preserved status according to the C ABI,
	 * plus RIP and EFLAGS for sysret
	 */
	movq	MY(ACTIVE_THREAD),%r11	/* point to current thread */
	movq	TH_PCB(%r11),%r11		/* point to pcb */
	addq	$ PCB_ISS,%r11			/* point to saved state */

	mov	%rsp,R_UESP(%r11)	/* callee-preserved register */
	mov	%rcx,R_EIP(%r11)	/* syscall places user RIP in RCX */
	mov	%rbx,R_EBX(%r11)	/* callee-preserved register */
	mov	%rax,%rbx		/* Now we can unpack eflags again */
	shr	$32,%rbx
	mov	%rbx,R_EFLAGS(%r11)	/* ... and save them in pcb as well */
	mov	%rbp,R_EBP(%r11)	/* callee-preserved register */
	mov	%r12,R_R12(%r11)	/* callee-preserved register */
	mov	%r13,R_R13(%r11)	/* callee-preserved register */
	mov	%r14,R_R14(%r11)	/* callee-preserved register */
	mov	%r15,R_R15(%r11)	/* callee-preserved register */

	/* Save syscall number and args for SYSCALL_EXAMINE/MSG_EXAMINE in glibc.
	 * Note: syscall number is only 32 bit, in EAX, so we sign-extend it in
	 * RAX to mask the EFLAGS bits.
	 */
	cdqe				/* sign-extend EAX in RAX */
	mov	%rax,R_EAX(%r11)	/* syscall number */
	mov	%rdi,R_EDI(%r11)	/* syscall arg0 */
	mov	%rsi,R_ESI(%r11)	/* syscall arg1 */
	mov	%rdx,R_EDX(%r11)	/* syscall arg2 */
	mov	%r10,R_R10(%r11)	/* syscall arg3 */
	mov	%r8,R_R8(%r11)		/* syscall arg4 */
	mov	%r9,R_R9(%r11)		/* syscall arg5 */

	mov	%r11,%rbx		/* prepare for error handling */
	mov	%r10,%rcx		/* fix arg3 location according to C ABI */

	/* switch to kernel stack, then we can enable interrupts */
	CPU_NUMBER_NO_STACK(%r11d)
	movq	CX(EXT(kernel_stack),%r11),%rsp
	sti

	/* Now we have saved state and args 1-6 are in place.
	 * Before invoking the syscall we do some bound checking and,
	 * if we have more that 6 arguments, we need to copy the
	 * remaining ones to the kernel stack, handling page faults when
	 * accessing the user stack.
	 */
	negl	%eax			/* get system call number */
	jl	_syscall64_range	/* out of range if it was positive */
	cmpl	EXT(mach_trap_count),%eax	/* check system call table bounds */
	jg	_syscall64_range	/* error if out of range */
	shll	$5,%eax			/* manual indexing of mach_trap_t */

	/* check if we need to place some arguments on the stack */
_syscall64_args_stack:
	mov	EXT(mach_trap_table)(%rax),%r10	/* get number of arguments */
	subq	$6,%r10			/* the first 6 args are already in place */
	jle	_syscall64_call		/* skip argument copy if num args <= 6 */

	movq	R_UESP(%rbx),%r11	/* get user stack pointer */
	addq	$8,%r11			/* Skip user return address */

	lea	(%r11,%r10,8),%r11	/* point past last argument */

	movq	$VM_MAX_ADDRESS, %r12
	cmpq	%r12,%r11		/* Check segment limit by hand */
	jae	_syscall64_addr_push

0:	subq	$8,%r11
	RECOVER(_syscall64_addr_push)
	mov	(%r11),%r12
	pushq	%r12			/* push argument on stack */
	dec	%r10
	jnz	0b			/* loop for all remaining arguments */

_syscall64_call:
	call	*EXT(mach_trap_table)+8(%rax)  /* call procedure */

_syscall64_check_for_ast:
	/* Check for ast. */
	CPU_NUMBER_NO_GS(%r11d)
	cmpl	$0,CX(EXT(need_ast),%r11)
	jz	_syscall64_restore_state

	/* Save the syscall return value, both on our stack, for the case
	 * i386_astintr returns normally, and in the PCB stack, in case it
	 * instead calls thread_block(thread_exception_return).
	 */
	pushq	%rax				/* save the return value on our stack */
	pushq	$0				/* dummy value to keep the stack aligned */

	/* Find the PCB stack. */
	movq	%rsp,%rcx
	or	$(KERNEL_STACK_SIZE-1),%rcx
	movq	-7-IKS_SIZE(%rcx),%rcx

	movq	%rax,R_EAX(%rcx)		/* save the return value in the PCB stack */
	call	EXT(i386_astintr)
	popq	%rax
	popq	%rax				/* restore the return value */
	jmp	_syscall64_check_for_ast	/* check again */

_syscall64_restore_state:
	/* Restore thread state and return to user using sysret. */
	cli	/* block interrupts when using the user stack in kernel space */
	movq	MY(ACTIVE_THREAD),%r11	/* point to current thread */
	movq	TH_PCB(%r11),%r11		/* point to pcb */
	addq	$ PCB_ISS,%r11			/* point to saved state */

	/* Restore syscall args. Note: we can't restore the syscall number in
	 * RAX because it needs to hold the return value.*/
	mov	R_EDI(%r11),%rdi	/* syscall arg0 */
	mov	R_ESI(%r11),%rsi	/* syscall arg1 */
	mov	R_EDX(%r11),%rdx	/* syscall arg2 */
	mov	R_R10(%r11),%r10	/* syscall arg3 */
	mov	R_R8(%r11),%r8		/* syscall arg4 */
	mov	R_R9(%r11),%r9		/* syscall arg5 */

	mov	R_UESP(%r11),%rsp	/* callee-preserved register,
					 * also switch back to user stack */
	mov	R_EIP(%r11),%rcx	/* sysret convention */
	mov	R_EBX(%r11),%rbx	/* callee-preserved register */
	mov	R_EBP(%r11),%rbp	/* callee-preserved register */
	mov	R_R12(%r11),%r12	/* callee-preserved register */
	mov	R_R13(%r11),%r13	/* callee-preserved register */
	mov	R_R14(%r11),%r14	/* callee-preserved register */
	mov	R_R15(%r11),%r15	/* callee-preserved register */
	mov	R_EFLAGS(%r11),%r11	/* sysret convention */

	sysretq		/* fast return to user-space, the thread didn't block */

/* Error handling fragments, from here we jump directly to the trap handler */
_syscall64_addr_push:
	movq	%r11,R_CR2(%rbx)	/* set fault address */
	movq	$(T_PAGE_FAULT),R_TRAPNO(%rbx)	/* set page-fault trap */
	movq	$(T_PF_USER),R_ERR(%rbx) /* set error code - read user space */
	jmp	_take_trap		/* treat as a trap */

_syscall64_range:
	movq	$(T_INVALID_OPCODE),R_TRAPNO(%rbx)
					/* set invalid-operation trap */
	movq	$0,R_ERR(%rbx)		/* clear error code */
	jmp	_take_trap		/* treat as a trap */

END(syscall64)
#endif /* USER32 */

	.data
DATA(cpu_features)
DATA(cpu_features_edx)
	.long	0
DATA(cpu_features_ecx)
	.long	0
	.text

/* Discover what kind of cpu we have; return the family number
   (3, 4, 5, 6, for 386, 486, 586, 686 respectively).  */
ENTRY(discover_x86_cpu_type)
	/* We are a modern enough processor to have the CPUID instruction;
	   use it to find out what we are. */
	movl	$1,%eax			/* Fetch CPU type info ... */
	cpuid				/*  ... into eax */
	movl	%ecx,cpu_features_ecx	/* Keep a copy */
	movl	%edx,cpu_features_edx	/* Keep a copy */
	shrl	$8,%eax			/* Slide family bits down */
	andl	$15,%eax		/* And select them */
	ret				/* And return */


/**/
/*
 * Utility routines.
 */

ENTRY(copyin)
	xchgq   %rsi,%rdi		/* Get user source and kernel destination */
	movq	$VM_MAX_ADDRESS, %rcx
	cmpq	%rcx,%rsi		/* Check segment limit by hand */
	jae	copyin_fail

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

bogus:
	ud2

ENTRY(copyout)
	xchgq   %rsi,%rdi		/* Get user source and kernel destination */
	movq	$VM_MAX_ADDRESS, %rcx
	cmpq	%rcx,%rdi		/* Check segment limit by hand */
	jae	copyin_fail

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
 * int inst_fetch(int eip, int cs);
 *
 * Fetch instruction byte.  Return -1 if invalid address.
 */
ENTRY(inst_fetch)
	movq	S_ARG1, %rax		/* get segment */
	movw	%ax,%fs			/* into FS */
	movq	S_ARG0, %rax		/* get offset */
	movq	$VM_MAX_ADDRESS, %rcx
	cmpq	%rcx,%rax		/* Check segment limit by hand */
	jae	_inst_fetch_fault
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
