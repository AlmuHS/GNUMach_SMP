#include <mach/machine/asm.h>
#include <i386/i386asm.h>
#include <i386/locore.h>
#include <i386/proc_reg.h>

#define STACK_SIZE (4096 * 2)

    .section .boot, "awx"

    .align 0x10

    .text

idt_descr:
    .word 0
    .long 0

.align 16
	.word 0
boot_gdt_descr:
	.word 3*8+7
	.long boot_gdt - KERNELBASE
.align 16
boot_gdt:
	/* NULL segment */
	.quad 0
	/* boot CS = 8 . Code Segment*/
	.word 0xffff /* Segment limit first 0-15 bits*/
	.word (-KERNELBASE) & 0xffff /*Base first 0-15 bits*/
	.byte ((-KERNELBASE) >> 16) & 0xff /*Base 16-23 bits */
	.byte 0x9a /*Access byte */
	.byte 0xcf /* High 4 bits */
	.byte ((-KERNELBASE) >> 24) & 0xff /*Base 24-31 bits */
	/* boot DS = 8 . Data segment*/
	.word 0xffff /*Segment limit */
	.word (-KERNELBASE) & 0xffff /*Base first 0-15 bits*/
	.byte ((-KERNELBASE) >> 16) & 0xff
	.byte 0x92 /*Access byte*/
	.byte 0xcf /* High 4 bits */
	.byte ((-KERNELBASE) >> 24) & 0xff /*Base 24-31 bits */

    .bss
    .p2align 4


#define M(addr) (addr - apboot + 0x7000)

    .text
    .globl apboot, apbootend
    .align 0x10
    .code16

apboot:
_apboot:
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    lgdt M(gdt_descr_tmp)

    mov %cr0, %eax
    or $0x1, %eax
    mov %eax, %cr0

    ljmp $0x8, $M(0f)

0:
	.code32

	mov $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss

	/* use segmentation to offset ourself. */
	lidt idt_descr - KERNELBASE
	lgdt	boot_gdt_descr - KERNELBASE
	ljmp $KERNEL_CS, $0f

0:
	movw	$KERNEL_DS,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss

	/* Load cpu stack */
	movl stack_ptr, %esp
	addl $STACK_SIZE, %esp

	/* Reset EFLAGS to a known state.  */
	pushl $0
	popf

	/* Enable paging in the cpu */
	call paging_setup

	/*lock interrupts*/
	cli

	/* Finish the cpu configuration */
	call cpu_ap_main

	/*if the processor is not added to the kernel, stop it*/
	sti
	nop
	hlt

	/*if the processor continues, skip to the end*/	
	jmp apbootend

    .align 0x10
gdt_descr_tmp:
    .short 0x8*3 - 1
    .long M(gdt_tmp)

    .align 0x10

gdt_tmp:
    /* 0 */
    .quad 0
    /* boot CS = 0x8 */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte 0x9a
    .byte 0xcf
    .byte 0x00
    /* boot DS = 0x10 */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xcf
    .byte 0x00


_apbootend:
apbootend:
nop
hlt
