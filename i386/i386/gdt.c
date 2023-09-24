/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity 
 * pertaining to distribution of the software without specific, written
 * prior permission.
 * 
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
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
/*
 * Global descriptor table.
 */
#include <mach/machine/vm_types.h>
#include <mach/xen.h>

#include <kern/assert.h>
#include <intel/pmap.h>
#include <kern/cpu_number.h>
#include <machine/percpu.h>

#include "vm_param.h"
#include "seg.h"
#include "gdt.h"
#include "mp_desc.h"

#ifdef	MACH_PV_DESCRIPTORS
/* It is actually defined in xen_boothdr.S */
extern
#endif	/* MACH_PV_DESCRIPTORS */
struct real_descriptor gdt[GDTSZ];

static void
gdt_fill(int cpu, struct real_descriptor *mygdt)
{
	/* Initialize the kernel code and data segment descriptors.  */
#ifdef __x86_64__
	assert(LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS == 0);
	_fill_gdt_descriptor(mygdt, KERNEL_CS, 0, 0, ACC_PL_K|ACC_CODE_R, SZ_64);
	_fill_gdt_descriptor(mygdt, KERNEL_DS, 0, 0, ACC_PL_K|ACC_DATA_W, SZ_64);
#ifndef	MACH_PV_DESCRIPTORS
	_fill_gdt_descriptor(mygdt, LINEAR_DS, 0, 0, ACC_PL_K|ACC_DATA_W, SZ_64);
#endif	/* MACH_PV_DESCRIPTORS */
#else
	_fill_gdt_descriptor(mygdt, KERNEL_CS,
			    LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
			    LINEAR_MAX_KERNEL_ADDRESS - (LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) - 1,
			    ACC_PL_K|ACC_CODE_R, SZ_32);
	_fill_gdt_descriptor(mygdt, KERNEL_DS,
			    LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
			    LINEAR_MAX_KERNEL_ADDRESS - (LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) - 1,
			    ACC_PL_K|ACC_DATA_W, SZ_32);
#ifndef	MACH_PV_DESCRIPTORS
	_fill_gdt_descriptor(mygdt, LINEAR_DS,
			    0,
			    0xffffffff,
			    ACC_PL_K|ACC_DATA_W, SZ_32);
#endif	/* MACH_PV_DESCRIPTORS */
	vm_offset_t thiscpu = kvtolin(&percpu_array[cpu]);
	_fill_gdt_descriptor(mygdt, PERCPU_DS,
			    thiscpu,
			    thiscpu + sizeof(struct percpu) - 1,
#ifdef __x86_64__
			    ACC_PL_K|ACC_DATA_W, SZ_64
#else
			    ACC_PL_K|ACC_DATA_W, SZ_32
#endif
	);
#endif

#ifdef	MACH_PV_DESCRIPTORS
	unsigned long frame = kv_to_mfn(mygdt);
	pmap_set_page_readonly(mygdt);
	if (hyp_set_gdt(kv_to_la(&frame), GDTSZ))
		panic("couldn't set gdt\n");
#endif
#ifdef	MACH_PV_PAGETABLES
	if (hyp_vm_assist(VMASST_CMD_enable, VMASST_TYPE_4gb_segments))
		panic("couldn't set 4gb segments vm assist");
#if 0
	if (hyp_vm_assist(VMASST_CMD_enable, VMASST_TYPE_4gb_segments_notify))
		panic("couldn't set 4gb segments vm assist notify");
#endif
#endif	/* MACH_PV_PAGETABLES */

#ifndef	MACH_PV_DESCRIPTORS
	/* Load the new GDT.  */
	{
		struct pseudo_descriptor pdesc;

		pdesc.limit = (GDTSZ * sizeof(struct real_descriptor))-1;
		pdesc.linear_base = kvtolin(mygdt);
		lgdt(&pdesc);
	}
#endif	/* MACH_PV_DESCRIPTORS */
}

static void
reload_segs(void)
{
	/* Reload all the segment registers from the new GDT.
	   We must load ds and es with 0 before loading them with KERNEL_DS
	   because some processors will "optimize out" the loads
	   if the previous selector values happen to be the same.  */
#ifndef __x86_64__
	asm volatile("ljmp	%0,$1f\n"
		     "1:\n"
		     "movw	%w2,%%ds\n"
		     "movw	%w2,%%es\n"
		     "movw	%w2,%%fs\n"
		     "movw	%w2,%%gs\n"
		     
		     "movw	%w1,%%ds\n"
		     "movw	%w1,%%es\n"
		     "movw	%w3,%%gs\n"
		     "movw	%w1,%%ss\n"
		     : : "i" (KERNEL_CS), "r" (KERNEL_DS), "r" (0), "r" (PERCPU_DS));
#endif
}

void
gdt_init(void)
{
	gdt_fill(0, gdt);

	reload_segs();

#ifdef	MACH_PV_PAGETABLES
#if VM_MIN_KERNEL_ADDRESS != LINEAR_MIN_KERNEL_ADDRESS
	/* things now get shifted */
#ifdef	MACH_PSEUDO_PHYS
	pfn_list = (void*) pfn_list + VM_MIN_KERNEL_ADDRESS - LINEAR_MIN_KERNEL_ADDRESS;
#endif	/* MACH_PSEUDO_PHYS */
	la_shift += LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS;
#endif
#endif	/* MACH_PV_PAGETABLES */
}

#if NCPUS > 1
void
ap_gdt_init(int cpu)
{
	gdt_fill(cpu, mp_gdt[cpu]);

	reload_segs();
}
#endif
