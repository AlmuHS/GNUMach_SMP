#include <mach/machine/asm.h>
#include <i386/i386asm.h>

#define STACK_SIZE (4096 * 2)

    .section .boot, "awx"

    .align 0x10

    .text
    .globl counter

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
    .globl stack_bsp
    .comm stack_bsp, STACK_SIZE


#define M(addr) (addr - apboot + 0x7000)

    .text
    .globl apboot, apbootend, stack_ptr
    .align 0x10
    .code16

counter:
.long 0

apboot:
_apboot:
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x7000, %sp

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
	mov $0x7000, %esp

/*	lidtl idt_descr-KERNELBASE*/

/*    lgdt boot_gdt_descr-KERNELBASE*/
/*    ljmpl $0x8, $(1f-KERNELBASE)*/

/*1:*/
/*    mov $0x10, %eax*/
/*    movw %ax, %ds*/
/*    movw %ax, %es*/


	/* use segmentation to offset ourself. */
	lgdt	boot_gdt_descr - KERNELBASE
	ljmp	$8,$0f

0:
	movw	$0,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%fs
	movw	%ax,%gs
	movw	$16,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss


    movl stack_ptr, %esp
    addl $STACK_SIZE, %esp

    incw counter

    pushl stack_ptr

    incw counter

    call cpu_ap_main
    cli


stack_ptr:
    .long 0x0

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

    .globl dummyf
dummyf:
ret

