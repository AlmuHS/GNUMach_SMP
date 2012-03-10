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

/*
 * spl routines for the i386at.
 */

#include <mach/machine/asm.h>
#include <i386/i386/ipl.h>
#include <i386/i386/i386asm.h>
#include <i386/i386/xen.h>

#if NCPUS > 1
#define mb lock; addl $0,(%esp)
#else
#define mb
#endif

/*
 * Program XEN evt masks from %eax.
 */
#define XEN_SETMASK()				\
	pushq	%rbx;				\
	movl	%eax,%ebx;			\
	xchgl	%eax,hyp_shared_info+EVTMASK;	\
	notl	%ebx;				\
	andl	%eax,%ebx;			/* Get unmasked events */ \
	testl	hyp_shared_info+PENDING, %ebx;	\
	popq	%rbx;				\
	jz	9f;				/* Check whether there was some pending */ \
lock	orl	$1,hyp_shared_info+CPU_PENDING_SEL; /* Yes, activate it */ \
	movb	$1,hyp_shared_info+CPU_PENDING; \
9:

ENTRY(spl0)
	mb;
	movl	EXT(curr_ipl),%eax	/* save current ipl */
	pushq	%rax
	cli				/* disable interrupts */
#ifdef LINUX_DEV
	movl	EXT(bh_active),%eax
					/* get pending mask */
	andl	EXT(bh_mask),%eax	/* any pending unmasked interrupts? */
	jz	1f			/* no, skip */
	call	EXT(spl1)		/* block further interrupts */
	incl	EXT(intr_count)		/* set interrupt flag */
	call	EXT(linux_soft_intr)	/* go handle interrupt */
	decl	EXT(intr_count)		/* decrement interrupt flag */
	cli				/* disable interrupts */
1:
#endif
	cmpl	$0,softclkpending	/* softclock pending? */
	je	1f			/* no, skip */
	movl	$0,softclkpending	/* clear flag */
	call	EXT(spl1)		/* block further interrupts */
#ifdef LINUX_DEV
	incl	EXT(intr_count)		/* set interrupt flag */
#endif
	call	EXT(softclock)		/* go handle interrupt */
#ifdef LINUX_DEV
	decl	EXT(intr_count)		/* decrement interrupt flag */
#endif
	cli				/* disable interrupts */
1:
	cmpl	$(SPL0),EXT(curr_ipl)	/* are we at spl0? */
	je	1f			/* yes, all done */
	movl	$(SPL0),EXT(curr_ipl)	/* set ipl */
#ifdef MACH_XEN
	movl	EXT(int_mask)+SPL0*4,%eax
					/* get xen mask */
	XEN_SETMASK()			/* program xen evts */
#endif
1:
	sti				/* enable interrupts */
	popq	%rax			/* return previous mask */
	ret


/*
 * Historically, SETIPL(level) was called
 * for spl levels 1-6, now we have combined
 * all the intermediate levels into the highest level
 * such that interrupts are either on or off,
 * since modern hardware can handle it.
 * This simplifies the interrupt handling
 * especially for the linux drivers.
 */
Entry(splsoftclock)
ENTRY(spl1)
ENTRY(spl2)
ENTRY(spl3)
Entry(splnet)
Entry(splhdw)
ENTRY(spl4)
Entry(splbio)
Entry(spldcm)
ENTRY(spl5)
Entry(spltty)
Entry(splimp)
Entry(splvm)
ENTRY(spl6)
Entry(splclock)
Entry(splsched)
Entry(splhigh)
Entry(splhi)
ENTRY(spl7)
	mb;
	/* just clear IF */
	cli
	movl    $SPL7,%eax
	xchgl   EXT(curr_ipl),%eax
	ret

ENTRY(splx)
	movq	S_ARG0,%rdx		/* get ipl */

#if (MACH_KDB || MACH_TTD) && !defined(MACH_XEN)
	/* First make sure that if we're exitting from ipl7, IF is still cleared */
	cmpl	$SPL7,EXT(curr_ipl)	/* from ipl7? */
	jne	0f
	pushfl
	popl	%eax
	testl	$0x200,%eax		/* IF? */
	jz	0f
	int3				/* Oops, interrupts got enabled?! */

0:
#endif	/* (MACH_KDB || MACH_TTD) && !MACH_XEN */
	testl	%edx,%edx		/* spl0? */
	jz	EXT(spl0)		/* yes, handle specially */
	cmpl	EXT(curr_ipl),%edx	/* same ipl as current? */
	jne	spl			/* no */
	cmpl	$SPL7,%edx		/* spl7? */
	je	1f			/* to ipl7, don't enable interrupts */
	sti				/* ensure interrupts are enabled */
1:
	movl	%edx,%eax		/* return previous ipl */
	ret

/*
 * Like splx() but returns with interrupts disabled and does
 * not return the previous ipl.  This should only be called
 * when returning from an interrupt.
 */
	.align	TEXT_ALIGN
	.globl	splx_cli
splx_cli:
	movq	S_ARG0,%rdx		/* get ipl */
	cli				/* disable interrupts */
	testl	%edx,%edx		/* spl0? */
	jnz	2f			/* no, skip */
#ifdef LINUX_DEV
	movl	EXT(bh_active),%eax
					/* get pending mask */
	andl	EXT(bh_mask),%eax	/* any pending unmasked interrupts? */
	jz	1f			/* no, skip */
	call	EXT(spl1)		/* block further interrupts */
	incl	EXT(intr_count)		/* set interrupt flag */
	call	EXT(linux_soft_intr)	/* go handle interrupt */
	decl	EXT(intr_count)		/* decrement interrupt flag */
	cli				/* disable interrupts */
1:
#endif
	cmpl	$0,softclkpending	/* softclock pending? */
	je	1f			/* no, skip */
	movl	$0,softclkpending	/* clear flag */
	call	EXT(spl1)		/* block further interrupts */
#ifdef LINUX_DEV
	incl	EXT(intr_count)		/* set interrupt flag */
#endif
	call	EXT(softclock)		/* go handle interrupt */
#ifdef LINUX_DEV
	decl	EXT(intr_count)		/* decrement interrupt flag */
#endif
	cli				/* disable interrupts */
1:
	xorl	%edx,%edx		/* edx = ipl 0 */
2:
	cmpl	EXT(curr_ipl),%edx	/* same ipl as current? */
	je	1f			/* yes, all done */
	movl	%edx,EXT(curr_ipl)	/* set ipl */
#ifdef MACH_XEN
	movl	EXT(int_mask)(,%edx,4),%eax
					/* get int mask */
	XEN_SETMASK()			/* program xen evts with new mask */
#endif
1:
	ret

/*
 * NOTE: This routine must *not* use %ecx, otherwise
 * the interrupt code will break.
 */
	.align	TEXT_ALIGN
	.globl	spl
spl:
#if (MACH_KDB || MACH_TTD) && !defined(MACH_XEN)
	/* First make sure that if we're exitting from ipl7, IF is still cleared */
	cmpl	$SPL7,EXT(curr_ipl)	/* from ipl7? */
	jne	0f
	pushfl
	popl	%eax
	testl	$0x200,%eax		/* IF? */
	jz	0f
	int3				/* Oops, interrupts got enabled?! */

0:
#endif	/* (MACH_KDB || MACH_TTD) && !MACH_XEN */
	cmpl	$SPL7,%edx		/* spl7? */
	je	EXT(spl7)		/* yes, handle specially */
#ifdef MACH_XEN
	movl	EXT(int_mask)(,%edx,4),%eax
					/* get int mask */
#endif
	cli				/* disable interrupts */
	xchgl	EXT(curr_ipl),%edx	/* set ipl */
#ifdef MACH_XEN
	XEN_SETMASK()			/* program PICs with new mask */
#endif
	sti				/* enable interrupts */
	movl	%edx,%eax		/* return previous ipl */
	ret

ENTRY(sploff)
	pushfq
	popq	%rax
	cli
	ret

ENTRY(splon)
	pushq	S_ARG0
	popfq
	ret

	.data
	.align	DATA_ALIGN
softclkpending:
	.long	0
	.text

ENTRY(setsoftclock)
	incl	softclkpending
	ret
