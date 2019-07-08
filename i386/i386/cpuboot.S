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

/*#if PAE*/
/*    //set_cr3((unsigned)_kvtophys(kernel_pmap->pdpbase));*/
/*	movl (pdpbase_addr), %eax*/
/*	movl %eax, %cr3*/

/*#ifndef	MACH_HYP*/
/*    //if (!CPU_HAS_FEATURE(CPU_FEATURE_PAE))*/
/*    movl (cpu_features), %eax*/
/*    test $(1<<CPU_FEATURE_PAE), %eax*/
/*    jnz setpae*/
/*    jz nopae*/

/*setpae:*/
/*    //set_cr4(get_cr4() | CR4_PAE);*/
/*	movl %cr4, %eax*/
/*	orl $CR4_PAE, %eax*/
/*	movl %eax, %cr4*/
/*nopae:*/
/*    nop*/

/*#endif	/* MACH_HYP */
/*#else*/
/*    //set_cr3((unsigned)_kvtophys(kernel_page_dir));*/
/*    movl (kernel_page_dir_addr), %eax*/
/*    movl %eax, %cr3*/

/*#endif	 PAE */
/*#ifndef	MACH_HYP*/
/*    /* Turn paging on.*/
/*     * Also set the WP bit so that on 486 or better processors*/
/*     * page-level write protection works in kernel mode.*/
/*     */
/*    //set_cr0(get_cr0() | CR0_PG | CR0_WP);*/
/*	//movl %cr0, %eax*/
/*	//orl $(CR0_PG | CR0_WP), %eax*/
/*	movl $0x8001003b, %eax*/
/*	movl %eax, %cr0*/

/*    //set_cr0(get_cr0() & ~(CR0_CD | CR0_NW));*/
/*	movl %cr0, %eax*/
/*	andl $(~(CR0_CD | CR0_NW)), %eax*/
/*	movl %eax, %cr0*/


/*    //if (CPU_HAS_FEATURE(CPU_FEATURE_PGE))*/
/*    movl (cpu_features), %eax*/
/*    test $(1<<CPU_FEATURE_PGE), %eax*/
/*    jnz setpge*/
/*    jz nopge*/

/*    setpge:*/
/*        //set_cr4(get_cr4() | CR4_PGE);*/
/*		movl %cr4, %eax*/
/*		orl $CR4_PGE, %eax*/
/*		movl %eax, %cr4*/
/*    nopge:*/
/*        nop*/

/*#endif	/* MACH_HYP */

/*    //flush_instr_queue();*/
/*	jmp 0f*/
/*0:*/
/*	//flush_tlb();*/
/*	movl %cr3, %eax*/
/*	movl %eax, %cr3*/


	movl stack_ptr, %esp
	addl $STACK_SIZE, %esp

	/* Reset EFLAGS to a known state.  */
	pushl $0
	popf

	pushl stack_ptr

	call paging_setup
	call cpu_ap_main
	cli

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
