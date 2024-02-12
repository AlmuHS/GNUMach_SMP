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

#ifndef	_I386_TRAP_H_
#define	_I386_TRAP_H_

#include <mach/machine/trap.h>

#ifndef __ASSEMBLER__
#include <i386/thread.h>
#include <mach/mach_types.h>

char *trap_name(unsigned int trapnum);

unsigned int interrupted_pc(thread_t);

void
i386_exception(
	int	exc,
	int	code,
	long	subcode) __attribute__ ((noreturn));

extern void
thread_kdb_return(void);

/*
 * Trap from kernel mode.  Only page-fault errors are recoverable,
 * and then only in special circumstances.  All other errors are
 * fatal.
 */
void kernel_trap(struct i386_saved_state *regs);

/*
 *	Trap from user mode.
 *	Return TRUE if from emulated system call.
 */
int user_trap(struct i386_saved_state *regs);

/*
 * Handle AST traps for i386.
 * Check for delayed floating-point exception from
 * AT-bus machines.
 */
void i386_astintr(void);

#endif /* !__ASSEMBLER__ */

#endif	/* _I386_TRAP_H_ */
