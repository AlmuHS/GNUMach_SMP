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
 *	File:	machine/thread.h
 *
 *	This file contains the structure definitions for the thread
 *	state as applied to I386 processors.
 */

#ifndef	_I386_THREAD_H_
#define _I386_THREAD_H_

#include <mach/boolean.h>
#include <mach/machine/vm_types.h>
#include <mach/machine/fp_reg.h>
#include <mach/machine/thread_status.h>

#include <kern/lock.h>

#include "gdt.h"

/*
 *	i386_saved_state:
 *
 *	This structure corresponds to the state of user registers
 *	as saved upon kernel entry.  It lives in the pcb.
 *	It is also pushed onto the stack for exceptions in the kernel.
 */

struct i386_saved_state {
#if !defined(__x86_64__) || defined(USER32)
	unsigned long	gs;
	unsigned long	fs;
	unsigned long	es;
	unsigned long	ds;
#endif
#ifdef __x86_64__
	unsigned long	r15;
	unsigned long	r14;
	unsigned long	r13;
	unsigned long	r12;
	unsigned long	r11;
	unsigned long	r10;
	unsigned long	r9;
	unsigned long	r8;
#endif
	unsigned long	edi;
	unsigned long	esi;
	unsigned long	ebp;
	unsigned long	cr2;		/* kernel esp stored by pusha -
					   we save cr2 here later */
	unsigned long	ebx;
	unsigned long	edx;
	unsigned long	ecx;
	unsigned long	eax;
	unsigned long	trapno;
	unsigned long	err;
	unsigned long	eip;
	unsigned long	cs;
	unsigned long	efl;
	unsigned long	uesp;
	unsigned long	ss;
#if !defined(__x86_64__) || defined(USER32)
	struct v86_segs {
	    unsigned long v86_es;	/* virtual 8086 segment registers */
	    unsigned long v86_ds;
	    unsigned long v86_fs;
	    unsigned long v86_gs;
	} v86_segs;
#endif
};

/*
 *	i386_exception_link:
 *
 *	This structure lives at the high end of the kernel stack.
 *	It points to the current thread`s user registers.
 */
struct i386_exception_link {
	struct i386_saved_state *saved_state;
};

/*
 *	i386_kernel_state:
 *
 *	This structure corresponds to the state of kernel registers
 *	as saved in a context-switch.  It lives at the base of the stack.
 */

struct i386_kernel_state {
	long			k_ebx;	/* kernel context */
	long			k_esp;
	long			k_ebp;
#ifdef __i386__
	long			k_edi;
	long			k_esi;
#endif
	long			k_eip;
#ifdef __x86_64__
	long			k_r12;
	long			k_r13;
	long			k_r14;
	long			k_r15;
#endif
};

/*
 *	Save area for user floating-point state.
 *	Allocated only when necessary.
 */

struct i386_fpsave_state {
	boolean_t		fp_valid;

	union {
		struct {
			struct i386_fp_save	fp_save_state;
			struct i386_fp_regs	fp_regs;
		};
		struct i386_xfp_save	xfp_save_state;
	};
};

#if !defined(__x86_64__) || defined(USER32)
/*
 *	v86_assist_state:
 *
 *	This structure provides data to simulate 8086 mode
 *	interrupts.  It lives in the pcb.
 */

struct v86_assist_state {
	vm_offset_t		int_table;
	unsigned short		int_count;
	unsigned short		flags;	/* 8086 flag bits */
};
#define	V86_IF_PENDING		0x8000	/* unused bit */
#endif

#if defined(__x86_64__) && !defined(USER32)
struct i386_segment_base_state {
	unsigned long fsbase;
	unsigned long gsbase;
};
#endif

/*
 *	i386_interrupt_state:
 *
 *	This structure describes the set of registers that must
 *	be pushed on the current ring-0 stack by an interrupt before
 *	we can switch to the interrupt stack.
 */

struct i386_interrupt_state {
#if !defined(__x86_64__) || defined(USER32)
	long	gs;
	long	fs;
	long	es;
	long	ds;
#endif
#ifdef __x86_64__
	long	r11;
	long	r10;
	long	r9;
	long	r8;
	long	rdi;
	long	rsi;
#endif
	long	edx;
	long	ecx;
	long	eax;
	long	eip;
	long	cs;
	long	efl;
};

/*
 *	i386_machine_state:
 *
 *	This structure corresponds to special machine state.
 *	It lives in the pcb.  It is not saved by default.
 */

struct i386_machine_state {
	struct user_ldt	*	ldt;
	struct i386_fpsave_state *ifps;
#if !defined(__x86_64__) || defined(USER32)
	struct v86_assist_state	v86s;
#endif
	struct real_descriptor user_gdt[USER_GDT_SLOTS];
	struct i386_debug_state ids;
#if defined(__x86_64__) && !defined(USER32)
	struct i386_segment_base_state sbs;
#endif
};

typedef struct pcb {
	/* START of the exception stack.
	 * NOTE: this area is used as exception stack when switching
	 * CPL, and it MUST be big enough to save the thread state and
	 * switch to a proper stack area, even considering recursive
	 * exceptions, otherwise it could corrupt nearby memory */
	struct i386_interrupt_state iis[2];	/* interrupt and NMI */
#ifdef __x86_64__
	unsigned long pad;	   /* ensure exception stack is aligned to 16 */
#endif
	struct i386_saved_state iss;
	/* END of exception stack*/
	struct i386_machine_state ims;
	decl_simple_lock_data(, lock)
	unsigned short init_control;		/* Initial FPU control to set */
#ifdef LINUX_DEV
	void *data;
#endif /* LINUX_DEV */
} *pcb_t;

/*
 *	On the kernel stack is:
 *	stack:	...
 *		struct i386_exception_link
 *		struct i386_kernel_state
 *	stack+KERNEL_STACK_SIZE
 */

#define STACK_IKS(stack)	\
	((struct i386_kernel_state *)((stack) + KERNEL_STACK_SIZE) - 1)
#define STACK_IEL(stack)	\
	((struct i386_exception_link *)STACK_IKS(stack) - 1)

#ifdef __x86_64__
#define KERNEL_STACK_ALIGN 16
#else
#define KERNEL_STACK_ALIGN 4
#endif

#if defined(__x86_64__) && !defined(USER32)
/* Follow System V AMD64 ABI guidelines. */
#define USER_STACK_ALIGN 16
#else
#define USER_STACK_ALIGN 4
#endif

#define USER_REGS(thread)	(&(thread)->pcb->iss)


#define syscall_emulation_sync(task)	/* do nothing */


/* #include_next "thread.h" */


#endif	/* _I386_THREAD_H_ */
