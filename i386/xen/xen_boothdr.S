/*
 *  Copyright (C) 2006, 2009, 2010, 2011 Samuel Thibault <samuel.thibault@ens-lyon.org>
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

#include <xen/public/elfnote.h>

.section __xen_guest
	.ascii	"GUEST_OS=GNU Mach"
	.ascii  ",GUEST_VERSION=1.3"
	.ascii	",XEN_VER=xen-3.0"
	.ascii  ",VIRT_BASE=0x20000000"
	.ascii  ",ELF_PADDR_OFFSET=0x20000000"
	.ascii	",HYPERCALL_PAGE=0x2"
#if PAE
	.ascii  ",PAE=yes"
#else
	.ascii  ",PAE=no"
#endif
	.ascii	",LOADER=generic"
#ifdef	MACH_PSEUDO_PHYS
	.ascii	",FEATURES=pae_pgdir_above_4gb"
#else	/* MACH_PSEUDO_PHYS */
	.ascii	",FEATURES=!auto_translated_physmap"
#endif
	.byte	0

/* Macro taken from linux/include/linux/elfnote.h */
#define ELFNOTE(name, type, desctype, descdata)	\
.pushsection .note.name			;	\
  .align 4				;	\
  .long 2f - 1f		/* namesz */	;	\
  .long 4f - 3f		/* descsz */	;	\
  .long type				;	\
1:.asciz "name"				;	\
2:.align 4				;	\
3:desctype descdata			;	\
4:.align 4				;	\
.popsection				;

	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_OS,		.asciz, "GNU Mach")
	ELFNOTE(Xen, XEN_ELFNOTE_GUEST_VERSION,		.asciz, "1.3")
	ELFNOTE(Xen, XEN_ELFNOTE_XEN_VERSION,		.asciz, "xen-3.0")
	ELFNOTE(Xen, XEN_ELFNOTE_VIRT_BASE,		.long,  _START)
	ELFNOTE(Xen, XEN_ELFNOTE_PADDR_OFFSET,		.long,  _START)
	ELFNOTE(Xen, XEN_ELFNOTE_ENTRY,			.long,  start)
	ELFNOTE(Xen, XEN_ELFNOTE_HYPERCALL_PAGE,	.long,  hypcalls)
#if PAE
	ELFNOTE(Xen, XEN_ELFNOTE_PAE_MODE,		.asciz, "yes")
#else
	ELFNOTE(Xen, XEN_ELFNOTE_PAE_MODE,		.asciz, "no")
#endif
	ELFNOTE(Xen, XEN_ELFNOTE_LOADER,		.asciz, "generic")
	ELFNOTE(Xen, XEN_ELFNOTE_FEATURES,              .asciz, ""
#ifdef MACH_PSEUDO_PHYS
						"pae_pgdir_above_4gb"
#else /* MACH_PSEUDO_PHYS */
						"!auto_translated_physmap|"
#endif
	)

#include <mach/machine/asm.h>

#include <i386/i386/i386asm.h>

	.text
	.globl	gdt, ldt
	.globl	start, _start, gdt
start:
_start:

	/* Switch to our own interrupt stack.  */
	movl	$(_intstack+INTSTACK_SIZE),%eax
	movl	%eax,%esp

	/* Reset EFLAGS to a known state.  */
	pushl	$0
	popf

	/* Push the start_info pointer to be the second argument.  */
	subl    $KERNELBASE,%esi
	pushl	%esi

	/* Fix ifunc entries */
	movl    $__rel_iplt_start,%esi
	movl    $__rel_iplt_end,%edi
iplt_cont:
	cmpl    %edi,%esi
	jae     iplt_done
	movl    (%esi),%ebx	/* r_offset */
	movb    4(%esi),%al	/* info */
	cmpb    $42,%al		/* IRELATIVE */
	jnz     iplt_next
	call    *(%ebx)		/* call ifunc */
	movl    %eax,(%ebx)	/* fixed address */
iplt_next:
	addl    $8,%esi
	jmp     iplt_cont
iplt_done:

	/* Jump into C code.  */
	call	EXT(c_boot_entry)

/* Those need to be aligned on page boundaries.  */
.global hyp_shared_info, hypcalls

	.org	(start + 0x1000)
hyp_shared_info:
	.org	hyp_shared_info + 0x1000

/* Labels just for debuggers */
#define hypcall(name, n) \
	.org	hypcalls + n*32 ; \
.globl __hyp_##name ; \
__hyp_##name:

hypcalls:
	hypcall(set_trap_table, 0)
	hypcall(mmu_update, 1)
	hypcall(set_gdt, 2)
	hypcall(stack_switch, 3)
	hypcall(set_callbacks, 4)
	hypcall(fpu_taskswitch, 5)
	hypcall(sched_op_compat, 6)
	hypcall(platform_op, 7)
	hypcall(set_debugreg, 8)
	hypcall(get_debugreg, 9)
	hypcall(update_descriptor, 10)
	hypcall(memory_op, 12)
	hypcall(multicall, 13)
	hypcall(update_va_mapping, 14)
	hypcall(set_timer_op, 15)
	hypcall(event_channel_op_compat, 16)
	hypcall(xen_version, 17)
	hypcall(console_io, 18)
	hypcall(physdev_op_compat, 19)
	hypcall(grant_table_op, 20)
	hypcall(vm_assist, 21)
	hypcall(update_va_mapping_otherdomain, 22)
	hypcall(iret, 23)
	hypcall(vcpu_op, 24)
	hypcall(set_segment_base, 25)
	hypcall(mmuext_op, 26)
	hypcall(acm_op, 27)
	hypcall(nmi_op, 28)
	hypcall(sched_op, 29)
	hypcall(callback_op, 30)
	hypcall(xenoprof_op, 31)
	hypcall(event_channel_op, 32)
	hypcall(physdev_op, 33)
	hypcall(hvm_op, 34)
	hypcall(sysctl, 35)
	hypcall(domctl, 36)
	hypcall(kexec_op, 37)

	hypcall(arch_0, 48)
	hypcall(arch_1, 49)
	hypcall(arch_2, 50)
	hypcall(arch_3, 51)
	hypcall(arch_4, 52)
	hypcall(arch_5, 53)
	hypcall(arch_6, 54)
	hypcall(arch_7, 55)

	.org	hypcalls + 0x1000

gdt:
	.org	gdt + 0x1000

ldt:
	.org	ldt + 0x1000

stack:
	.long	_intstack+INTSTACK_SIZE,0xe021
	.comm	_intstack,INTSTACK_SIZE

