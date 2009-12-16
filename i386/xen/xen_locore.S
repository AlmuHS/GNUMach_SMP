/*
 *  Copyright (C) 2006 Samuel Thibault <samuel.thibault@ens-lyon.org>
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <mach/machine/asm.h>

#include <i386/i386asm.h>
#include <i386/cpu_number.h>
#include <i386/xen.h>

	.data	2
int_active:
	.long	0
	

	.text
	.globl	hyp_callback, hyp_failsafe_callback
	P2ALIGN(TEXT_ALIGN)
hyp_callback:
	pushl	%eax
	jmp	EXT(all_intrs)

ENTRY(interrupt)
	incl	int_active		/* currently handling interrupts */
	call	EXT(hyp_c_callback)	/* call generic interrupt routine */
	decl	int_active		/* stopped handling interrupts */
	sti
	ret

/* FIXME: if we're _very_ unlucky, we may be re-interrupted, filling stack
 *
 * Far from trivial, see mini-os. That said, maybe we could just, before poping
 * everything (which is _not_ destructive), save sp into a known place and use
 * it+jmp back?
 *
 * Mmm, there seems to be an iret hypcall that does exactly what we want:
 * perform iret, and if IF is set, clear the interrupt mask.
 */

/* Pfff, we have to check pending interrupts ourselves. Some other DomUs just make an hypercall for retriggering the irq. Not sure it's really easier/faster */
ENTRY(hyp_sti)
	pushl	%ebp
	movl	%esp, %ebp
_hyp_sti:
	movb	$0,hyp_shared_info+CPU_CLI /* Enable interrupts */
	cmpl	$0,int_active		/* Check whether we were already checking pending interrupts */
	jz	0f
	popl	%ebp
	ret				/* Already active, just return */
0:
	/* Not active, check pending interrupts by hand */
	/* no memory barrier needed on x86 */
	cmpb	$0,hyp_shared_info+CPU_PENDING
	jne	0f
	popl	%ebp
	ret
0:
	movb	$0xff,hyp_shared_info+CPU_CLI
1:
	pushl	%eax
	pushl	%ecx
	pushl	%edx
	incl	int_active		/* currently handling interrupts */

	pushl	$0
	pushl	$0
	call	EXT(hyp_c_callback)
	popl	%edx
	popl	%edx

	popl	%edx
	popl	%ecx
	popl	%eax
	decl	int_active		/* stopped handling interrupts */
	cmpb	$0,hyp_shared_info+CPU_PENDING
	jne	1b
	jmp	_hyp_sti

/* Hypervisor failed to reload segments. Dump them.  */
hyp_failsafe_callback:
#if 1
	/* load sane segments */
	mov	%ss, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	push	%esp
	call	EXT(hyp_failsafe_c_callback)
#else
	popl	%ds
	popl	%es
	popl	%fs
	popl	%gs
	iret
#endif
