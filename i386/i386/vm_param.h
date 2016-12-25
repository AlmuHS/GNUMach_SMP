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

#include <kern/macros.h>

/* XXX use xu/vm_param.h */
#include <mach/vm_param.h>
#ifdef MACH_PV_PAGETABLES
#include <xen/public/xen.h>
#endif

/* The kernel address space is usually 1GB, usually starting at virtual address 0.  */
/* This can be changed freely to separate kernel addresses from user addresses
 * for better trace support in kdb; the _START symbol has to be offset by the
 * same amount. */
#define VM_MIN_KERNEL_ADDRESS	0xC0000000UL

#ifdef	MACH_XEN
/* PV kernels can be loaded directly to the target virtual address */
#define INIT_VM_MIN_KERNEL_ADDRESS	VM_MIN_KERNEL_ADDRESS
#else	/* MACH_XEN */
/* This must remain 0 */
#define INIT_VM_MIN_KERNEL_ADDRESS	0x00000000UL
#endif	/* MACH_XEN */

#ifdef	MACH_PV_PAGETABLES
#if	PAE
#define HYP_VIRT_START	HYPERVISOR_VIRT_START_PAE
#else	/* PAE */
#define HYP_VIRT_START	HYPERVISOR_VIRT_START_NONPAE
#endif	/* PAE */
#define VM_MAX_KERNEL_ADDRESS	(HYP_VIRT_START - LINEAR_MIN_KERNEL_ADDRESS + VM_MIN_KERNEL_ADDRESS)
#else	/* MACH_PV_PAGETABLES */
#define VM_MAX_KERNEL_ADDRESS	(LINEAR_MAX_KERNEL_ADDRESS - LINEAR_MIN_KERNEL_ADDRESS + VM_MIN_KERNEL_ADDRESS)
#endif	/* MACH_PV_PAGETABLES */

/*
 * Reserve mapping room for the kernel map, which includes
 * the device I/O map and the IPC map.
 */
#define VM_KERNEL_MAP_SIZE (152 * 1024 * 1024)

/* The kernel virtual address space is actually located
   at high linear addresses.
   This is the kernel address range in linear addresses.  */
#define LINEAR_MIN_KERNEL_ADDRESS	(VM_MAX_ADDRESS)
#define LINEAR_MAX_KERNEL_ADDRESS	(0xffffffffUL)

#ifdef	MACH_PV_PAGETABLES
/* need room for mmu updates (2*8bytes) */
#define KERNEL_STACK_SIZE	(4*I386_PGBYTES)
#define INTSTACK_SIZE		(4*I386_PGBYTES)
#else	/* MACH_PV_PAGETABLES */
#define KERNEL_STACK_SIZE	(1*I386_PGBYTES)
#define INTSTACK_SIZE		(1*I386_PGBYTES)
#endif	/* MACH_PV_PAGETABLES */
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

/*
 * Physical memory properties.
 */
#define VM_PAGE_DMA_LIMIT       DECL_CONST(0x1000000, UL)

#ifdef MACH_XEN
/* TODO Completely check Xen physical/virtual layout */
#define VM_PAGE_MAX_SEGS 3
#define VM_PAGE_DIRECTMAP_LIMIT (VM_MAX_KERNEL_ADDRESS \
				 - VM_MIN_KERNEL_ADDRESS \
				 - VM_KERNEL_MAP_SIZE)
#define VM_PAGE_HIGHMEM_LIMIT   DECL_CONST(0x10000000000000, ULL)
#else /* MACH_XEN */
#ifdef __LP64__
#define VM_PAGE_MAX_SEGS 4
#define VM_PAGE_DMA32_LIMIT     DECL_CONST(0x100000000, UL)
#define VM_PAGE_DIRECTMAP_LIMIT DECL_CONST(0x400000000000, UL)
#define VM_PAGE_HIGHMEM_LIMIT   DECL_CONST(0x10000000000000, UL)
#else /* __LP64__ */
#define VM_PAGE_DIRECTMAP_LIMIT (VM_MAX_KERNEL_ADDRESS \
				 - VM_MIN_KERNEL_ADDRESS \
				 - VM_KERNEL_MAP_SIZE + 1)
#ifdef PAE
#define VM_PAGE_MAX_SEGS 3
#define VM_PAGE_HIGHMEM_LIMIT   DECL_CONST(0x10000000000000, ULL)
#else /* PAE */
#define VM_PAGE_MAX_SEGS 3
#define VM_PAGE_HIGHMEM_LIMIT   DECL_CONST(0xfffff000, UL)
#endif /* PAE */
#endif /* __LP64__ */
#endif /* MACH_XEN */

/*
 * Physical segment indexes.
 */
#define VM_PAGE_SEG_DMA         0

#ifdef __LP64__
#define VM_PAGE_SEG_DMA32       1
#define VM_PAGE_SEG_DIRECTMAP   2
#define VM_PAGE_SEG_HIGHMEM     3
#else /* __LP64__ */
#define VM_PAGE_SEG_DMA32       1   /* Alias for the DIRECTMAP segment */
#define VM_PAGE_SEG_DIRECTMAP   1
#define VM_PAGE_SEG_HIGHMEM     2
#endif /* __LP64__ */

#endif /* _I386_KERNEL_I386_VM_PARAM_ */
