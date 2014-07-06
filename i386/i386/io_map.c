/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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

#include <kern/printf.h>
#include <mach/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

extern vm_offset_t kernel_virtual_start;

/*
 * Allocate and map memory for devices that may need to be mapped before
 * Mach VM is running.
 */
vm_offset_t
io_map(
	vm_offset_t	phys_addr,
	vm_size_t	size)
{
	vm_offset_t	start;

	if (kernel_map == VM_MAP_NULL) {
	    /*
	     * VM is not initialized.  Grab memory.
	     */
	    start = kernel_virtual_start;
	    kernel_virtual_start += round_page(size);
	    printf("stealing kernel virtual addresses %08lx-%08lx\n", start, kernel_virtual_start);
	}
	else {
	    (void) kmem_alloc_pageable(kernel_map, &start, round_page(size));
	}
	(void) pmap_map_bd(start, phys_addr, phys_addr + round_page(size),
			VM_PROT_READ|VM_PROT_WRITE);
	return (start);
}

/*
 * Allocate and map memory for devices that may need to be mapped before
 * Mach VM is running.
 *
 * This maps the all pages containing [PHYS_ADDR:PHYS_ADDR + SIZE].
 * For contiguous requests to those pages will reuse the previously
 * established mapping.
 *
 * Warning: this leaks memory maps for now, do not use it yet for something
 * else than Mach shutdown.
 */
vm_offset_t
io_map_cached(
	vm_offset_t	phys_addr,
	vm_size_t	size)
{
  static vm_offset_t base;
  static vm_size_t length;
  static vm_offset_t map;

  if (! map
      || (phys_addr < base)
      || (base + length < phys_addr + size))
    {
      base = trunc_page(phys_addr);
      length = round_page(phys_addr - base + size);
      map = io_map(base, length);
    }

  return map + (phys_addr - base);
}
