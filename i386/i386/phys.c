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

#include <string.h>

#include <mach/boolean.h>
#include <mach/xen.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <vm/vm_map.h>
#include "vm_param.h"
#include <mach/vm_prot.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <i386/pmap.h>
#include <i386/model_dep.h>
#include <mach/machine/vm_param.h>

#define INTEL_PTE_W(p) (INTEL_PTE_VALID | INTEL_PTE_WRITE | INTEL_PTE_REF | INTEL_PTE_MOD | pa_to_pte(p))
#define INTEL_PTE_R(p) (INTEL_PTE_VALID | INTEL_PTE_REF | pa_to_pte(p))

/*
 *	pmap_zero_page zeros the specified (machine independent) page.
 */
void
pmap_zero_page(phys_addr_t p)
{
	assert(p != vm_page_fictitious_addr);
	vm_offset_t v;
	pmap_mapwindow_t *map;
	boolean_t mapped = p >= VM_PAGE_DIRECTMAP_LIMIT;

	if (mapped)
	{
		map = pmap_get_mapwindow(INTEL_PTE_W(p));
		v = map->vaddr;
	}
	else
		v = phystokv(p);

	memset((void*) v, 0, PAGE_SIZE);

	if (mapped)
		pmap_put_mapwindow(map);
}

/*
 *	pmap_copy_page copies the specified (machine independent) pages.
 */
void
pmap_copy_page(
	phys_addr_t src,
	phys_addr_t dst)
{
	vm_offset_t src_addr_v, dst_addr_v;
	pmap_mapwindow_t *src_map = NULL;
	pmap_mapwindow_t *dst_map;
	boolean_t src_mapped = src >= VM_PAGE_DIRECTMAP_LIMIT;
	boolean_t dst_mapped = dst >= VM_PAGE_DIRECTMAP_LIMIT;
	assert(src != vm_page_fictitious_addr);
	assert(dst != vm_page_fictitious_addr);

	if (src_mapped)
	{
		src_map = pmap_get_mapwindow(INTEL_PTE_R(src));
		src_addr_v = src_map->vaddr;
	}
	else
		src_addr_v = phystokv(src);

	if (dst_mapped)
	{
		dst_map = pmap_get_mapwindow(INTEL_PTE_W(dst));
		dst_addr_v = dst_map->vaddr;
	}
	else
		dst_addr_v = phystokv(dst);

	memcpy((void *) dst_addr_v, (void *) src_addr_v, PAGE_SIZE);

	if (src_mapped)
		pmap_put_mapwindow(src_map);
	if (dst_mapped)
		pmap_put_mapwindow(dst_map);
}

/*
 *	copy_to_phys(src_addr_v, dst_addr_p, count)
 *
 *	Copy virtual memory to physical memory
 */
void
copy_to_phys(
	vm_offset_t 	src_addr_v, 
	phys_addr_t 	dst_addr_p,
	int 		count)
{
	vm_offset_t dst_addr_v;
	pmap_mapwindow_t *dst_map;
	boolean_t mapped = dst_addr_p >= VM_PAGE_DIRECTMAP_LIMIT;
	assert(dst_addr_p != vm_page_fictitious_addr);
	assert(pa_to_pte(dst_addr_p + count-1) == pa_to_pte(dst_addr_p));

	if (mapped)
	{
		dst_map = pmap_get_mapwindow(INTEL_PTE_W(dst_addr_p));
		dst_addr_v = dst_map->vaddr + (dst_addr_p & (INTEL_PGBYTES-1));
	}
	else
		dst_addr_v = phystokv(dst_addr_p);

	memcpy((void *)dst_addr_v, (void *)src_addr_v, count);

	if (mapped)
		pmap_put_mapwindow(dst_map);
}

/*
 *	copy_from_phys(src_addr_p, dst_addr_v, count)
 *
 *	Copy physical memory to virtual memory.  The virtual memory
 *	is assumed to be present (e.g. the buffer pool).
 */
void
copy_from_phys(
	phys_addr_t 	src_addr_p, 
	vm_offset_t 	dst_addr_v,
	int 		count)
{
	vm_offset_t src_addr_v;
	pmap_mapwindow_t *src_map;
	boolean_t mapped = src_addr_p >= VM_PAGE_DIRECTMAP_LIMIT;
	assert(src_addr_p != vm_page_fictitious_addr);
	assert(pa_to_pte(src_addr_p + count-1) == pa_to_pte(src_addr_p));

	if (mapped)
	{
		src_map = pmap_get_mapwindow(INTEL_PTE_R(src_addr_p));
		src_addr_v = src_map->vaddr + (src_addr_p & (INTEL_PGBYTES-1));
	}
	else
		src_addr_v = phystokv(src_addr_p);

	memcpy((void *)dst_addr_v, (void *)src_addr_v, count);

	if (mapped)
		pmap_put_mapwindow(src_map);
}

/*
 *	kvtophys(addr)
 *
 *	Convert a kernel virtual address to a physical address
 */
phys_addr_t
kvtophys(vm_offset_t addr)
{
	pt_entry_t *pte;

	if ((pte = pmap_pte(kernel_pmap, addr)) == PT_ENTRY_NULL)
		return 0;
	return pte_to_pa(*pte) | (addr & INTEL_OFFMASK);
}
