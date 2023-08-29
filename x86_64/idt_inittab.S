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

#include <i386/i386/seg.h>
#include <i386/i386/i386asm.h>

#include <xen/public/arch-x86/xen-x86_64.h>

/* We'll be using macros to fill in a table in data hunk 2
   while writing trap entrypoint routines at the same time.
   Here's the header that comes before everything else.  */
	.data	2
ENTRY(idt_inittab)
	.text

/*
 * Interrupt descriptor table and code vectors for it.
 */
#ifdef	MACH_PV_DESCRIPTORS
#define	IDT_ENTRY(n,entry,type,ist) \
	.data	2		;\
	.byte	n		;\
	.byte	(((type)&ACC_PL)>>5)|((((type)&(ACC_TYPE|ACC_A))==ACC_INTR_GATE)<<2)	;\
	.word	FLAT_KERNEL_CS	;\
	.word	ist		;\
	.word	0	/*pad*/	;\
	.quad	entry		;\
	.text
#else	/* MACH_PV_DESCRIPTORS */
#define	IDT_ENTRY(n,entry,type,ist) \
	.data	2		;\
	.quad	entry		;\
	.word	n		;\
	.word	type		;\
	.word	ist		;\
	.word	0	/*pad*/	;\
	.text
#endif	/* MACH_PV_DESCRIPTORS */

/*
 * No error code.  Clear error code and push trap number.
 */
#define	EXCEPTION(n,name) \
	IDT_ENTRY(n,EXT(name),ACC_PL_K|ACC_TRAP_GATE, 0);\
ENTRY(name)				;\
	INT_FIX				;\
	pushq	$(0)			;\
	pushq	$(n)			;\
	jmp	EXT(alltraps)

/*
 * User-accessible exception.  Otherwise, same as above.
 */
#define	EXCEP_USR(n,name) \
	IDT_ENTRY(n,EXT(name),ACC_PL_U|ACC_TRAP_GATE, 0);\
ENTRY(name)				;\
	INT_FIX				;\
	pushq	$(0)			;\
	pushq	$(n)			;\
	jmp	EXT(alltraps)

/*
 * Error code has been pushed.  Just push trap number.
 */
#define	EXCEP_ERR(n,name) \
	IDT_ENTRY(n,EXT(name),ACC_PL_K|ACC_INTR_GATE, 0);\
ENTRY(name)				;\
	INT_FIX				;\
	pushq	$(n)			;\
	jmp	EXT(alltraps)

/*
 * Special interrupt code: dispatches to a unique entrypoint,
 * not defined automatically here.
 */
#define	EXCEP_SPC(n,name, ist)  \
	IDT_ENTRY(n,EXT(name),ACC_PL_K|ACC_TRAP_GATE, ist)


EXCEPTION(0x00,t_zero_div)
EXCEP_SPC(0x01,t_debug, 0)
/* skip NMI interrupt - let more specific code figure that out.  */
EXCEP_USR(0x03,t_int3)
EXCEP_USR(0x04,t_into)
EXCEP_USR(0x05,t_bounds)
EXCEPTION(0x06,t_invop)
EXCEPTION(0x07,t_nofpu)
EXCEP_SPC(0x08,t_dbl_fault, 1)
EXCEPTION(0x09,a_fpu_over)
EXCEPTION(0x0a,a_inv_tss)
EXCEP_SPC(0x0b,t_segnp, 0)
EXCEP_ERR(0x0c,t_stack_fault)
EXCEP_SPC(0x0d,t_gen_prot, 0)
EXCEP_SPC(0x0e,t_page_fault, 0)
#ifdef MACH_PV_DESCRIPTORS
EXCEP_ERR(0x0f,t_trap_0f)
#else
EXCEPTION(0x0f,t_trap_0f)
#endif
EXCEPTION(0x10,t_fpu_err)
EXCEPTION(0x11,t_trap_11)
EXCEPTION(0x12,t_trap_12)
EXCEPTION(0x13,t_trap_13)
EXCEPTION(0x14,t_trap_14)
EXCEPTION(0x15,t_trap_15)
EXCEPTION(0x16,t_trap_16)
EXCEPTION(0x17,t_trap_17)
EXCEPTION(0x18,t_trap_18)
EXCEPTION(0x19,t_trap_19)
EXCEPTION(0x1a,t_trap_1a)
EXCEPTION(0x1b,t_trap_1b)
EXCEPTION(0x1c,t_trap_1c)
EXCEPTION(0x1d,t_trap_1d)
EXCEPTION(0x1e,t_trap_1e)
EXCEPTION(0x1f,t_trap_1f)

/* Terminator */
	.data	2
	.long	0
#ifdef	MACH_PV_DESCRIPTORS
	.long	0
	.quad	0
#endif	/* MACH_PV_DESCRIPTORS */

