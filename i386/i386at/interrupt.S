/* 
 * Copyright (c) 1995 Shantanu Goel
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

#include <mach/machine/asm.h>

#include <i386/ipl.h>
#include <i386/pic.h>
#include <i386/i386asm.h>

#define READ_ISR	(OCW_TEMPLATE|READ_NEXT_RD|READ_IS_ONRD)

/*
 * Generic interrupt handler.
 *
 * On entry, %eax contains the irq number.
 */
ENTRY(interrupt)
	pushl	%eax			/* save irq number */
	movl	%eax,%ecx		/* copy irq number */
	shll	$2,%ecx			/* irq * 4 */
	call	spl7			/* set ipl */
	movl	EXT(iunit)(%ecx),%edx	/* get device unit number */
	pushl	%eax			/* push previous ipl */
	pushl	%edx			/* push unit number */
	call	*EXT(ivect)(%ecx)	/* call interrupt handler */
	addl	$4,%esp			/* pop unit number */
	call	splx_cli		/* restore previous ipl */
	addl	$4,%esp			/* pop previous ipl */

	cli				/* XXX no more nested interrupts */
	popl	%ecx			/* restore irq number */

	movl	$1,%eax
	shll	%cl,%eax		/* get corresponding IRQ mask */
	orl	EXT(curr_pic_mask),%eax /* add current mask */

	cmpl	$8,%ecx			/* do we need to ack slave? */
	jl	1f			/* no, only master */

	/* EOI on slave */
	movb	%ah,%al
	outb	%al,$(PIC_SLAVE_OCW)	/* mask slave out */

	movb	$(SPECIFIC_EOI),%al	/* specific EOI for this irq */
	andb	$7,%cl			/* irq number for the slave */
	orb	%cl,%al			/* combine them */
	outb	%al,$(PIC_SLAVE_ICW)	/* ack interrupt to slave */

	movb	$(SPECIFIC_EOI + I_AM_SLAVE_2),%al	/* specific master EOI for cascaded slave */
	outb	%al,$(PIC_MASTER_ICW)	/* ack interrupt to master */

	movl	EXT(curr_pic_mask),%eax /* restore original mask */
	movb	%ah,%al
	outb	%al,$(PIC_SLAVE_OCW)	/* unmask slave */
	jmp	2f

1:
	/* EOI on master */
	outb	%al,$(PIC_MASTER_OCW)	/* mask master out */

	movb	$(SPECIFIC_EOI),%al	/* specific EOI for this irq */
	orb	%cl,%al			/* combine with irq number */
	outb	%al,$(PIC_MASTER_ICW)	/* ack interrupt to master */

	movl	EXT(curr_pic_mask),%eax /* restore original mask */
	outb	%al,$(PIC_MASTER_OCW)	/* unmask master */
2:
	ret
END(interrupt)
