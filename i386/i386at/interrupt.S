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
#ifdef APIC
# include <i386/apic.h>
#else
# include <i386/pic.h>
#endif
#include <i386/i386asm.h>

#define READ_ISR	(OCW_TEMPLATE|READ_NEXT_RD|READ_IS_ONRD)

/*
 * Generic interrupt handler.
 *
 * On entry, %eax contains the irq number.
 *
 * Note: kdb_kintr needs to know our stack usage
 */

#define S_REGS 28(%esp)
#define S_RET 24(%esp)
#define S_IRQ 20(%esp)
#define S_IPL 16(%esp)

ENTRY(interrupt)
#ifdef APIC
	cmpl	$255,%eax		/* was this a spurious intr? */
	jne	1f
	ret				/* if so, just return */
1:
#endif
	subl	$24,%esp		/* Two local variables + 4 parameters */
	movl	%eax,S_IRQ		/* save irq number */

	call	spl7			/* set ipl */
	movl	%eax,S_IPL		/* save previous ipl */

	movl	S_IRQ,%ecx		/* restore irq number */

#if NCPUS > 1
	cmpl	$CALL_PMAP_UPDATE,%ecx	/* was this a SMP pmap_update request? */
	je	_call_single

	cmpl	$CALL_AST_CHECK,%ecx	/* was this a SMP remote -> local ast request? */
	je	_call_local_ast
#endif

#ifndef APIC
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
#else
	movl	%ecx,(%esp)		/* load irq number as 1st arg */
	call	EXT(ioapic_irq_eoi)	/* ioapic irq specific EOI */
#endif

	movl	S_IPL,%eax
	movl	%eax,4(%esp)		/* previous ipl as 2nd arg */

	movl	S_RET,%eax
	movl	%eax,8(%esp)		/* return address as 3rd arg */

	movl	S_REGS,%eax
	movl	%eax,12(%esp)		/* address of interrupted registers as 4th arg */

	movl	S_IRQ,%eax		/* copy irq number */

	shll	$2,%eax			/* irq * 4 */
	movl	EXT(iunit)(%eax),%edx	/* get device unit number */
	movl	%edx,(%esp)		/* unit number as 1st arg */

	call	*EXT(ivect)(%eax)	/* call interrupt handler */

_completed:
	movl	S_IPL,%eax		/* restore previous ipl */
	movl	%eax,(%esp)
	call	splx_cli		/* restore previous ipl */

	addl	$24,%esp		/* pop local variables */
	ret

#if NCPUS > 1
_call_single:
	call	EXT(lapic_eoi)		/* lapic EOI before the handler to allow extra update */
	call	EXT(pmap_update_interrupt)
	jmp	_completed

_call_local_ast:
	call	EXT(lapic_eoi)		/* lapic EOI */
	call	EXT(ast_check)		/* AST check on this cpu */
	jmp	_completed

#endif
END(interrupt)
