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
 * On entry, %rax contains the irq number.
 */
ENTRY(interrupt)
	pushq	%rax			/* save irq number */
	call	spl7			/* set ipl */
	pushq	%rax			/* save previous ipl */
	movl	8(%esp),%edx		/* set irq number as 3rd arg */
	movl	%edx,%ebx		/* copy irq number */
	shll	$2,%ebx			/* irq * 4 */
	movl	EXT(iunit)(%ebx),%edi	/* get device unit number as 1st arg */
	movl	%eax, %esi		/* previous ipl as 2nd arg */
	movq	16(%esp), %rcx		/* return address as 4th arg */
	movq	24(%esp), %r8		/* address of interrupted registers as 5th arg */
	shll	$1,%ebx			/* irq * 8 */
	call	*EXT(ivect)(%ebx)	/* call interrupt handler */
	popq	%rdi			/* restore previous ipl */
	call	splx_cli		/* restore previous ipl */

	cli				/* XXX no more nested interrupts */
	popq	%rcx			/* restore irq number */

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
