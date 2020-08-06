/*
 *  Copyright (C) 2006-2009 Free Software Foundation
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

#include <i386/i386/i386asm.h>
#include <i386/i386/cpu_number.h>
#include <i386/i386/xen.h>

	.data	2
int_active:
	.long	0
	

	.text
	.globl	hyp_callback, hyp_failsafe_callback
	P2ALIGN(TEXT_ALIGN)
hyp_callback:
	popq	%rcx
	popq	%r11
	pushq	%rax
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
	pushq	%rbp
	movq	%rsp, %rbp
_hyp_sti:
	movb	$0,hyp_shared_info+CPU_CLI /* Enable interrupts */
	cmpl	$0,int_active		/* Check whether we were already checking pending interrupts */
	jz	0f
	popq	%rbp
	ret				/* Already active, just return */
0:
	/* Not active, check pending interrupts by hand */
	/* no memory barrier needed on x86 */
	cmpb	$0,hyp_shared_info+CPU_PENDING
	jne	0f
	popq	%rbp
	ret
0:
	movb	$0xff,hyp_shared_info+CPU_CLI
1:
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	pushq	%rdi
	pushq	%rsi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	incl	int_active		/* currently handling interrupts */

	xorq	%rdi,%rdi
	xorq	%rsi,%rsi
	call	EXT(hyp_c_callback)

	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rsi
	popq	%rdi
	popq	%rdx
	popq	%rcx
	popq	%rax
	decl	int_active		/* stopped handling interrupts */
	cmpb	$0,hyp_shared_info+CPU_PENDING
	jne	1b
	jmp	_hyp_sti

/* Hypervisor failed to reload segments. Dump them.  */
hyp_failsafe_callback:
ud2
#if 1
/* TODO: FIXME */
	/* load sane segments */
	mov	%ss, %ax
#if 0
	mov	%ax, %ds
	mov	%ax, %es
#endif
	mov	%ax, %fs
	mov	%ax, %gs
	movq	%rsp, %rdi
	call	EXT(hyp_failsafe_c_callback)
#else
	popq	%rdx
	movq	%rdx,%ds
	popq	%rdx
	movq	%rdx,%es
	popq	%fs
	popq	%gs

movq (%rsp),%rax
ud2
	iretq
#endif

#undef iretq
ENTRY(hyp_iretq)
	testb	$2,1*8(%rsp)
	jnz	slow
	/* There is no ring1 on x86_64, we have to force ring 3 */
	orb	$3,1*8(%rsp)
	orb	$3,4*8(%rsp)
	iretq

slow:
/* There is no ring 1/2 on x86_64, so going back to user needs to go through
 * hypervisor */
	pushq	$0
	jmp	__hyp_iret
