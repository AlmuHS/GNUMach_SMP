/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity 
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
/*
 * Clock interrupt.
 */
#include <mach/machine/eflags.h>

#include <kern/mach_clock.h>
#include <i386/thread.h>

#if	defined(AT386)
#include <i386/ipl.h>
#endif

extern void	clock_interrupt();
extern char	return_to_iret[];

void
hardclock(iunit,        old_ipl, ret_addr, regs)
        int     iunit;          /* 'unit' number */
	int	old_ipl;	/* old interrupt level */
	char *	ret_addr;	/* return address in interrupt handler */
	struct i386_interrupt_state *regs;
				/* saved registers */
{
	if (ret_addr == return_to_iret)
	    /*
	     * Interrupt from user mode or from thread stack.
	     */
	    clock_interrupt(tick,			/* usec per tick */
			    (regs->efl & EFL_VM) ||	/* user mode */
			    ((regs->cs & 0x03) != 0),	/* user mode */
#if defined(LINUX_DEV)
			    FALSE			/* ignore SPL0 */
#else	/* LINUX_DEV */
			    old_ipl == SPL0		/* base priority */
#endif	/* LINUX_DEV */
			    );
	else
	    /*
	     * Interrupt from interrupt stack.
	     */
	    clock_interrupt(tick,			/* usec per tick */
			    FALSE,			/* kernel mode */
			    FALSE);			/* not SPL0 */

#ifdef LINUX_DEV
	linux_timer_intr();
#endif
}
