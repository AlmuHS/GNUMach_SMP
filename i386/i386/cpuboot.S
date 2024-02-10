/*
 * Copyright (c) 2022 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if NCPUS > 1
#include <mach/machine/asm.h>
#include <i386/i386asm.h>
#include <i386/proc_reg.h>
#include <i386/apic.h>
#include <i386/cpu_number.h>
#include <i386/seg.h>
#include <i386/gdt.h>

#define M(addr)		(addr - apboot)
#define CR0_CLEAR_FLAGS_CACHE_ENABLE	(CR0_CD | CR0_NW)
#define CR0_SET_FLAGS	(CR0_CLEAR_FLAGS_CACHE_ENABLE | CR0_PE)
#define CR0_CLEAR_FLAGS	(CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP)
#define BOOT_CS		0x8
#define BOOT_DS		0x10

.data

.align 16
apboot_idt_ptr:
	.long 0
.align 16
	.word 0
apboot_gdt_descr:
	.word 14*8-1
	.long apboot_gdt - KERNELBASE
.align 16
apboot_gdt:
	/* NULL segment = 0x0 */
	.quad 0

	/* KERNEL_CS = 0x8 */
	.word 0xffff /* Segment limit first 0-15 bits*/
	.word (-KERNELBASE) & 0xffff /*Base first 0-15 bits*/
	.byte ((-KERNELBASE) >> 16) & 0xff /*Base 16-23 bits */
	.byte ACC_PL_K | ACC_CODE_R | ACC_P /*Access byte */
	.byte ((SZ_32 | SZ_G) << 4) | 0xf /* High 4 bits */
	.byte ((-KERNELBASE) >> 24) & 0xff /*Base 24-31 bits */

	/* KERNEL_DS = 0x10 */
	.word 0xffff /*Segment limit */
	.word (-KERNELBASE) & 0xffff /*Base first 0-15 bits*/
	.byte ((-KERNELBASE) >> 16) & 0xff
	.byte ACC_PL_K | ACC_DATA_W | ACC_P /*Access byte*/
	.byte ((SZ_32 | SZ_G) << 4) | 0xf /* High 4 bits */
	.byte ((-KERNELBASE) >> 24) & 0xff /*Base 24-31 bits */

	/* LDT = 0x18 */
	.quad 0

	/* TSS = 0x20 */
	.quad 0

	/* USER_LDT = 0x28 */
	.quad 0

	/* USER_TSS = 0x30 */
	.quad 0

	/* LINEAR = 0x38 */
	.quad 0

	/* FPREGS = 0x40 */
	.quad 0

	/* USER_GDT = 0x48 and 0x50 */
	.quad 0
	.quad 0

	/* USER_TSS64 = 0x58 */
	.quad 0

	/* USER_TSS64 = 0x60 */
	.quad 0

	/* boot GS = 0x68 */
	.word 0xffff
apboot_percpu_low:
	.word 0
apboot_percpu_med:
	.byte 0
	.byte ACC_PL_K | ACC_DATA_W | ACC_P
	.byte ((SZ_32 | SZ_G) << 4) | 0xf
apboot_percpu_high:
	.byte 0

.globl apboot, apbootend, gdt_descr_tmp, apboot_jmp_offset
.align 16
.code16

apboot:
_apboot:
	/* This is now address CS:0 in real mode */

	/* Set data seg same as code seg */
	mov	%cs, %dx
	mov	%dx, %ds

	cli
	xorl	%eax, %eax
	movl    %eax, %cr3

	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	lgdt	M(gdt_descr_tmp)

	movl	%cr0, %eax
	andl	$~CR0_CLEAR_FLAGS, %eax
	orl	$CR0_SET_FLAGS, %eax
	movl	%eax, %cr0

	/* ljmpl with relocation from machine_init */
	.byte 0x66
	.byte 0xea
apboot_jmp_offset:
	.long M(0f)
	.word BOOT_CS

0:
	.code32
	/* Protected mode! */
	movw	$BOOT_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss

	lgdtl	apboot_gdt_descr - KERNELBASE
	ljmpl	$KERNEL_CS, $1f
1:
	xorl	%eax, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	$KERNEL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* Get CPU number */
	movl	$1, %eax
	cpuid
	shrl	$24, %ebx
	movl	%cs:CX(cpu_id_lut, %ebx), %ecx

	/* Access per_cpu area */
	movl	%ecx,%eax
	movl	$PC_SIZE,%ebx
	mul	%ebx
	addl	$percpu_array - KERNELBASE, %eax

	/* Record our cpu number */
	movl	%ecx, (PERCPU_CPU_ID + KERNELBASE)(%eax)

	/* Set up temporary percpu descriptor */
	movw	%ax, apboot_percpu_low
	shr	$16, %eax
	movb	%al, apboot_percpu_med
	shr	$8, %ax
	movb	%al, apboot_percpu_high

	movw	$PERCPU_DS, %ax
	movw	%ax, %gs

	/* Load null Interrupt descriptor table */
	mov	apboot_idt_ptr, %ebx
	lidt	(%ebx)

	/* Enable local apic in xAPIC mode */
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	$APIC_MSR, %ecx
	rdmsr
	orl	$APIC_MSR_ENABLE, %eax
	andl	$(~(APIC_MSR_BSP | APIC_MSR_X2APIC)), %eax
	movl	$APIC_MSR, %ecx
	wrmsr

	/* Load int_stack_top[cpu] -> esp */
	CPU_NUMBER_NO_STACK(%edx)
	movl	CX(EXT(int_stack_top), %edx), %esp

	/* Ensure stack alignment */
	andl	$0xfffffff0, %esp

	/* Reset EFLAGS to a known state */
	pushl $0
	popfl

	/* Finish the cpu configuration */
	call EXT(cpu_ap_main)

	/* NOT REACHED */
	hlt

.align 16
    .word 0
gdt_descr_tmp:
    .short 3*8-1
    .long M(gdt_tmp)

.align 16
gdt_tmp:
    /* 0 */
    .quad 0
    /* BOOT_CS */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte ACC_PL_K | ACC_CODE_R | ACC_P
    .byte ((SZ_32 | SZ_G) << 4) | 0xf
    .byte 0x00
    /* BOOT_DS */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte ACC_PL_K | ACC_DATA_W | ACC_P
    .byte ((SZ_32 | SZ_G) << 4) | 0xf
    .byte 0x00

_apbootend:
apbootend:
#endif
