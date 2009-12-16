/* 
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
#ifndef _I386_KERNEL_I386_VM_PARAM_
#define _I386_KERNEL_I386_VM_PARAM_

/* XXX use xu/vm_param.h */
#include <mach/vm_param.h>
#include <xen/public/xen.h>

/* The kernel address space is 1GB, starting at virtual address 0.  */
#ifdef	MACH_XEN
#define VM_MIN_KERNEL_ADDRESS	0x20000000UL
#else	/* MACH_XEN */
#define VM_MIN_KERNEL_ADDRESS	0x00000000UL
#endif	/* MACH_XEN */

#ifdef	MACH_XEN
#if	PAE
#define HYP_VIRT_START	HYPERVISOR_VIRT_START_PAE
#else	/* PAE */
#define HYP_VIRT_START	HYPERVISOR_VIRT_START_NONPAE
#endif	/* PAE */
#define VM_MAX_KERNEL_ADDRESS	(HYP_VIRT_START - LINEAR_MIN_KERNEL_ADDRESS + VM_MIN_KERNEL_ADDRESS)
#else	/* MACH_XEN */
#define VM_MAX_KERNEL_ADDRESS	(LINEAR_MAX_KERNEL_ADDRESS - LINEAR_MIN_KERNEL_ADDRESS + VM_MIN_KERNEL_ADDRESS)
#endif	/* MACH_XEN */

/* The kernel virtual address space is actually located
   at high linear addresses.
   This is the kernel address range in linear addresses.  */
#define LINEAR_MIN_KERNEL_ADDRESS	(VM_MAX_ADDRESS)
#define LINEAR_MAX_KERNEL_ADDRESS	(0xffffffffUL)

#ifdef	MACH_XEN
/* need room for mmu updates (2*8bytes) */
#define KERNEL_STACK_SIZE	(4*I386_PGBYTES)
#define INTSTACK_SIZE		(4*I386_PGBYTES)
#else	/* MACH_XEN */
#define KERNEL_STACK_SIZE	(1*I386_PGBYTES)
#define INTSTACK_SIZE		(1*I386_PGBYTES)
#endif	/* MACH_XEN */
						/* interrupt stack size */

/*
 *	Conversion between 80386 pages and VM pages
 */

#define trunc_i386_to_vm(p)	(atop(trunc_page(i386_ptob(p))))
#define round_i386_to_vm(p)	(atop(round_page(i386_ptob(p))))
#define vm_to_i386(p)		(i386_btop(ptoa(p)))

/*
 *	Physical memory is direct-mapped to virtual memory
 *	starting at virtual address VM_MIN_KERNEL_ADDRESS.
 */
#define phystokv(a)	((vm_offset_t)(a) + VM_MIN_KERNEL_ADDRESS)
/*
 * This can not be used with virtual mappings, but can be used during bootstrap
 */
#define _kvtophys(a)	((vm_offset_t)(a) - VM_MIN_KERNEL_ADDRESS)

/*
 *	Kernel virtual memory is actually at 0xc0000000 in linear addresses.
 */
#define kvtolin(a)	((vm_offset_t)(a) - VM_MIN_KERNEL_ADDRESS + LINEAR_MIN_KERNEL_ADDRESS)
#define lintokv(a)	((vm_offset_t)(a) - LINEAR_MIN_KERNEL_ADDRESS + VM_MIN_KERNEL_ADDRESS)

#endif /* _I386_KERNEL_I386_VM_PARAM_ */
