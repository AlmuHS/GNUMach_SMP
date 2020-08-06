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

#ifndef _MACH_I386_ASM_H_
#define _MACH_I386_ASM_H_

#ifdef __i386__
#define S_ARG0	 4(%esp)
#define S_ARG1	 8(%esp)
#define S_ARG2	12(%esp)
#define S_ARG3	16(%esp)

#define FRAME	pushl %ebp; movl %esp, %ebp
#define EMARF	leave

#define B_ARG0	 8(%ebp)
#define B_ARG1	12(%ebp)
#define B_ARG2	16(%ebp)
#define B_ARG3	20(%ebp)
#endif

#ifdef __x86_64__
#define S_ARG0	%rdi
#define S_ARG1	%rsi
#define S_ARG2	%rdx
#define S_ARG3	%rcx
#define S_ARG4	%r8
#define S_ARG5	%r9

#define FRAME	pushq %rbp; movq %rsp, %rbp
#define EMARF	leave

#define B_ARG0	S_ARG0
#define B_ARG1	S_ARG1
#define B_ARG2	S_ARG2
#define B_ARG3	S_ARG3

#ifdef MACH_XEN
#define INT_FIX \
	popq %rcx ;\
	popq %r11
#else
#define INT_FIX
#endif
#endif

#ifdef i486
#define TEXT_ALIGN	4
#else
#define TEXT_ALIGN	2
#endif
#define DATA_ALIGN	2
#define ALIGN		TEXT_ALIGN

#define P2ALIGN(p2)	.p2align p2	/* gas-specific */

#define	LCL(x)	x

#define LB(x,n) n
#ifdef	__STDC__
#ifndef __ELF__
#define EXT(x) _ ## x
#define LEXT(x) _ ## x ## :
#define SEXT(x) "_"#x
#else
#define EXT(x) x
#define LEXT(x) x ## :
#define SEXT(x) #x
#endif
#define LCLL(x) x ## :
#define gLB(n)  n ## :
#define LBb(x,n) n ## b
#define LBf(x,n) n ## f
#else /* __STDC__ */
#error XXX elf
#define EXT(x) _/**/x
#define LEXT(x) _/**/x/**/:
#define LCLL(x) x/**/:
#define gLB(n) n/**/:
#define LBb(x,n) n/**/b
#define LBf(x,n) n/**/f
#endif /* __STDC__ */
#define SVC .byte 0x9a; .long 0; .word 0x7

#define String	.ascii
#define Value	.word
#define Times(a,b) (a*b)
#define Divide(a,b) (a/b)

#define INB	inb	%dx, %al
#define OUTB	outb	%al, %dx
#define INL	inl	%dx, %eax
#define OUTL	outl	%eax, %dx

#define data16	.byte 0x66
#define addr16	.byte 0x67



#ifdef GPROF

#define MCOUNT		.data; gLB(9) .long 0; .text; lea LBb(x, 9),%edx; call mcount
#define	ENTRY(x)	.globl EXT(x); .type EXT(x), @function; .p2align TEXT_ALIGN; LEXT(x) ; \
			pushl %ebp; movl %esp, %ebp; MCOUNT; popl %ebp;
#define	ENTRY2(x,y)	.globl EXT(x); .type EXT(x), @function; .globl EXT(y); .type EXT(y), @function; \
			.p2align TEXT_ALIGN; LEXT(x) LEXT(y)
#define	ASENTRY(x) 	.globl x; .type x, @function; .p2align TEXT_ALIGN; gLB(x) ; \
  			pushl %ebp; movl %esp, %ebp; MCOUNT; popl %ebp;
#define	END(x)		.size x,.-x
#else	/* GPROF */

#define MCOUNT
#define	ENTRY(x)	.globl EXT(x); .type EXT(x), @function; .p2align TEXT_ALIGN; LEXT(x)
#define	ENTRY2(x,y)	.globl EXT(x); .type EXT(x), @function; .globl EXT(y); .type EXT(y), @function; \
			.p2align TEXT_ALIGN; LEXT(x) LEXT(y)
#define	ASENTRY(x)	.globl x; .type x, @function; .p2align TEXT_ALIGN; gLB(x)
#define	END(x)		.size x,.-x
#endif	/* GPROF */

#define	Entry(x)	.globl EXT(x); .type EXT(x), @function; .p2align TEXT_ALIGN; LEXT(x)
#define	DATA(x)		.globl EXT(x); .p2align DATA_ALIGN; LEXT(x)

#endif /* _MACH_I386_ASM_H_ */
