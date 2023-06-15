/*
 *  Copyright (C) 2022 Free Software Foundation
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
#include <i386/i386/proc_reg.h>
#include <i386/i386/seg.h>
	/*
	 * This section will be put first into .boot.  See also	x86_64/ldscript.
	 */
	.section .boot.text,"ax"
	/* We should never be entered this way.	 */
	.globl	boot_start
boot_start:

	.code32
	jmp	boot_entry

	/* MultiBoot header - see multiboot.h.	*/
#define	MULTIBOOT_MAGIC		0x1BADB002
#define MULTIBOOT_FLAGS		0x00000003
	P2ALIGN(2)
boot_hdr:
	.long	MULTIBOOT_MAGIC
	.long	MULTIBOOT_FLAGS
	/*
	* The next item here is the checksum.
	* XX this works OK until we need at least the 30th bit.
	*/
	.long	- (MULTIBOOT_MAGIC+MULTIBOOT_FLAGS)

	.global _start
_start:
boot_entry:
	/*
	 * Prepare minimal page mapping to jump to 64 bit and to C code.
	 * The first 4GB is identity mapped, and the first 2GB are re-mapped
	 * to high addresses at KERNEL_MAP_BASE
	 */

	movl	$p3table,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p4table)
	/*
	 * Fill 4 entries in L3 table to cover the whole 32-bit 4GB address
	 * space. Part of it might be remapped later if the kernel is mapped
	 * below 4G.
	 */
	movl	$p2table,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p3table)
	movl	$p2table1,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p3table + 8)
	movl	$p2table2,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p3table + 16)
	movl	$p2table3,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p3table + 24)
	/* point each page table level two entry to a page */
	mov	$0,%ecx
.map_p2_table:
	mov	$0x200000,%eax	 // 2MiB page, should be always available
	mul	%ecx
	or	$(PTE_V|PTE_W|PTE_S),%eax  // enable 2MiB page instead of 4k
	mov	%eax,p2table(,%ecx,8)
	inc	%ecx
	cmp	$2048,%ecx  // 512 entries per table, map 4 L2 tables
	jne	.map_p2_table

	/*
	 * KERNEL_MAP_BASE must me aligned to 2GB.
	 * Depending on kernel starting address, we might need to add another
	 * entry in the L4 table (controlling 512 GB chunks). In any case, we
	 * add two entries in L3 table to make sure we map 2GB for the kernel.
	 * Note that this may override part of the mapping create above.
	 */
.kernel_map:
#if KERNEL_MAP_BASE >= (1U << 39)
	movl	$p3ktable,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p4table + (8 * ((KERNEL_MAP_BASE >> 39) & 0x1FF)))  // select 512G block
	movl	$p2ktable1,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p3ktable + (8 * ((KERNEL_MAP_BASE >> 30) & 0x1FF) ))  // select first 1G block
	movl	$p2ktable2,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p3ktable + (8 * (((KERNEL_MAP_BASE >> 30) & 0x1FF) + 1) ))  // select second 1G block
#else
	movl	$p2ktable1,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p3table + (8 * ((KERNEL_MAP_BASE >> 30) & 0x1FF) ))  // select first 1G block
	movl	$p2ktable2,%eax
	or	$(PTE_V|PTE_W),%eax
	movl	%eax,(p3table + (8 * (((KERNEL_MAP_BASE >> 30) & 0x1FF) + 1) ))  // select second 1G block
#endif

	mov	$0,%ecx
.map_p2k_table:
	mov	$0x200000,%eax	 // 2MiB page, should be always available
	mul	%ecx
	or	$(PTE_V|PTE_W|PTE_S),%eax  // enable 2MiB page instead of 4K
	mov	%eax,p2ktable1(,%ecx,8)
	inc	%ecx
	cmp	$1024,%ecx  // 512 entries per table, map 2 L2 tables
	jne	.map_p2k_table

switch64:
	/*
	 * Jump to 64 bit mode, we have to
	 * - enable PAE
	 * - enable long mode
	 * - enable paging and load the tables filled above in CR3
	 * - jump to a 64-bit code segment
	 */
	mov	%cr4,%eax
	or	$CR4_PAE,%eax
	mov	%eax,%cr4
	mov	$0xC0000080,%ecx  // select EFER register
	rdmsr
	or	$(1 << 8),%eax	// long mode enable bit
	wrmsr
	mov	$p4table,%eax
	mov	%eax,%cr3
	mov	%cr0,%eax
	or	$CR0_PG,%eax
	or	$CR0_WP,%eax
	mov	%eax,%cr0

	lgdt	gdt64pointer
	movw	$0,%ax
	movw	%ax,%fs
	movw	%ax,%gs
	movw	$16,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	ljmp	$8,$boot_entry64

	.code64

boot_entry64:
	/* Switch to our own interrupt stack.  */
	movq	$solid_intstack+INTSTACK_SIZE-16, %rax
	andq	$(~15),%rax
	movq	%rax,%rsp

	/* Reset EFLAGS to a known state.  */
	pushq	$0
	popf
	/* save multiboot info for later */
	movq	%rbx,%r8

	/* Fix ifunc entries */
	movq	$__rela_iplt_start,%rsi
	movq	$__rela_iplt_end,%rdi
iplt_cont:
	cmpq	%rdi,%rsi
	jae	iplt_done
	movq	(%rsi),%rbx	/* r_offset */
	movb	4(%rsi),%al	/* info */
	cmpb	$42,%al		/* IRELATIVE */
	jnz	iplt_next
	call	*(%ebx)		/* call ifunc */
	movq	%rax,(%rbx)	/* fixed address */
iplt_next:
	addq	$8,%rsi
	jmp	iplt_cont
iplt_done:

	/* restore multiboot info */
	movq	%r8,%rdi
	/* Jump into C code.  */
	call	EXT(c_boot_entry)
	/* not reached */
	nop

	.code32
	.section .boot.data
	.align 4096
#define  SEG_ACCESS_OFS 40
#define  SEG_GRANULARITY_OFS 52
gdt64:
	.quad	0
gdt64code:
        .quad	(ACC_P << SEG_ACCESS_OFS) | (ACC_CODE_R << SEG_ACCESS_OFS) | (SZ_64 << SEG_GRANULARITY_OFS)
gdt64data:
        .quad	(ACC_P << SEG_ACCESS_OFS) | (ACC_DATA_W << SEG_ACCESS_OFS)
gdt64end:
	.skip	(4096 - (gdt64end - gdt64))
gdt64pointer:
	.word	gdt64end - gdt64 - 1
	.quad	gdt64

	.section .boot.data
	.align 4096
p4table:	.space 4096
p3table:	.space 4096
p2table:	.space 4096
p2table1:	.space 4096
p2table2:	.space 4096
p2table3:	.space 4096
p3ktable:	.space 4096
p2ktable1:	 .space 4096
p2ktable2:	 .space 4096
