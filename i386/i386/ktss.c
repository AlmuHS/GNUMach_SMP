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
/*
 * Kernel task state segment.
 *
 * We don't use the i386 task switch mechanism.  We need a TSS
 * only to hold the kernel stack pointer for the current thread.
 *
 * XXX multiprocessor??
 */
#include "vm_param.h"
#include "seg.h"
#include "gdt.h"
#include "ktss.h"
#include "mp_desc.h"

/* A kernel TSS with a complete I/O bitmap.  */
struct task_tss ktss;

void
ktss_fill(struct task_tss *myktss, struct real_descriptor *mygdt)
{
	/* XXX temporary exception stacks */
	/* FIXME: make it per-processor */
	static int exception_stack[1024];
	static int double_fault_stack[1024];

#ifdef	MACH_RING1
	/* Xen won't allow us to do any I/O by default anyway, just register
	 * exception stack */
	if (hyp_stack_switch(KERNEL_DS, (unsigned long)(exception_stack+1024)))
		panic("couldn't register exception stack\n");
#else	/* MACH_RING1 */
	/* Initialize the master TSS descriptor.  */
	_fill_gdt_sys_descriptor(mygdt, KERNEL_TSS,
				kvtolin(myktss), sizeof(struct task_tss) - 1,
				ACC_PL_K|ACC_TSS, 0);

	/* Initialize the master TSS.  */
#ifdef __x86_64__
	myktss->tss.rsp0 = (unsigned long)(exception_stack+1024);
	myktss->tss.ist1 = (unsigned long)(double_fault_stack+1024);
#else /* ! __x86_64__ */
	myktss->tss.ss0 = KERNEL_DS;
	myktss->tss.esp0 = (unsigned long)(exception_stack+1024);
#endif /* __x86_64__ */

	myktss->tss.io_bit_map_offset = IOPB_INVAL;
	/* Set the last byte in the I/O bitmap to all 1's.  */
	myktss->barrier = 0xff;

	/* Load the TSS.  */
	ltr(KERNEL_TSS);
#endif	/* MACH_RING1 */
}

void
ktss_init(void)
{
	ktss_fill(&ktss, gdt);
}

#if NCPUS > 1
void
ap_ktss_init(int cpu)
{
	ktss_fill(&mp_desc_table[cpu]->ktss, mp_gdt[cpu]);
}
#endif
