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

#if	NCPUS > 1

#include <string.h> 

#include <i386/cpu.h>
#include <kern/cpu_number.h>
#include <kern/debug.h>
#include <mach/machine.h>
#include <mach/xen.h>
#include <vm/vm_kern.h>
#include <kern/kmutex.h>
#include <kern/printf.h>
#include <i386/loose_ends.h>

#include <i386at/model_dep.h>
#include <i386/model_dep.h>
#include <i386/mp_desc.h>
#include <i386/lock.h>
#include <machine/ktss.h>
#include <machine/tss.h>
#include <machine/io_perm.h>
#include <machine/vm_param.h>

#include <i386at/acpi_rsdp.h>


/*
 * The i386 needs an interrupt stack to keep the PCB stack from being
 * overrun by interrupts.  All interrupt stacks MUST lie at lower addresses
 * than any thread`s kernel stack.
 */

/*
 * Addresses of bottom and top of interrupt stacks.
 */
vm_offset_t	interrupt_stack[NCPUS];
vm_offset_t	_int_stack_top[NCPUS];

/*
 * Barrier address.
 */
vm_offset_t	int_stack_high;

/*
 * First cpu`s interrupt stack.
 */
char		intstack[];	/* bottom */
char		eintstack[];	/* top */


static struct kmutex mp_cpu_boot_lock;

/*
 * Multiprocessor i386/i486 systems use a separate copy of the
 * GDT, IDT, LDT, and kernel TSS per processor.  The first three
 * are separate to avoid lock contention: the i386 uses locked
 * memory cycles to access the descriptor tables.  The TSS is
 * separate since each processor needs its own kernel stack,
 * and since using a TSS marks it busy.
 */

/*
 * Allocated descriptor tables.
 */
struct mp_desc_table	*mp_desc_table[NCPUS] = { 0 };

/*
 * Pointer to TSS for access in load_context.
 */
struct task_tss		*mp_ktss[NCPUS] = { 0 };

/*
 * Pointer to GDT to reset the KTSS busy bit.
 */
struct real_descriptor	*mp_gdt[NCPUS] = { 0 };

/*
 * Boot-time tables, for initialization and master processor.
 */
extern struct real_gate		idt[IDTSZ];
extern struct real_descriptor	gdt[GDTSZ];
extern struct real_descriptor	ldt[LDTSZ];

/*
 * Allocate and initialize the per-processor descriptor tables.
 */

struct mp_desc_table *
mp_desc_init(int mycpu)
{
	struct mp_desc_table *mpt;

	if (mycpu == master_cpu) {
		/*
		 * Master CPU uses the tables built at boot time.
		 * Just set the TSS and GDT pointers.
		 */
		mp_ktss[mycpu] = (struct task_tss *) &ktss;
		mp_gdt[mycpu] = gdt;
		return 0;
	}
	else {
		/*
		 * Other CPUs allocate the table from the bottom of
		 * the interrupt stack.
		 */
		mpt = (struct mp_desc_table *) interrupt_stack[mycpu];

		mp_desc_table[mycpu] = mpt;
		mp_ktss[mycpu] = &mpt->ktss;
		mp_gdt[mycpu] = mpt->gdt;

		/*
		 * Copy the tables
		 */
		memcpy(mpt->idt,
		  idt,
		  sizeof(idt));
		memcpy(mpt->gdt,
		  gdt,
		  sizeof(gdt));
		memcpy(mpt->ldt,
		  ldt,
		  sizeof(ldt));
		memset(&mpt->ktss, 0, 
		  sizeof(struct task_tss));

		/*
		 * Fix up the entries in the GDT to point to
		 * this LDT and this TSS.
		 */
#ifdef	MACH_RING1
		panic("TODO %s:%d\n",__FILE__,__LINE__);
#else	/* MACH_RING1 */
		fill_descriptor(&mpt->gdt[sel_idx(KERNEL_LDT)],
			(unsigned)&mpt->ldt,
			LDTSZ * sizeof(struct real_descriptor) - 1,
			ACC_P|ACC_PL_K|ACC_LDT, 0);
		fill_descriptor(&mpt->gdt[sel_idx(KERNEL_TSS)],
			(unsigned)&mpt->ktss,
			sizeof(struct task_tss) - 1,
			ACC_P|ACC_PL_K|ACC_TSS, 0);

		mpt->ktss.tss.ss0 = KERNEL_DS;
		mpt->ktss.tss.io_bit_map_offset = IOPB_INVAL;
		mpt->ktss.barrier = 0xFF;
#endif	/* MACH_RING1 */

		return mpt;
	}
}


/*TODO: Reimplement function to send Startup IPI to cpu*/
kern_return_t intel_startCPU(int slot_num)
{
	/*TODO: Get local APIC from cpu*/	
	/*int	lapic = cpu_to_lapic[slot_num];*/
	unsigned long eFlagsRegister;

	kmutex_init(&mp_cpu_boot_lock);

	//assert(lapic != -1);

	/*DBGLOG_CPU_INIT(slot_num);*/

	/*DBG("intel_startCPU(%d) lapic_id=%d\n", slot_num, lapic);
	 *DBG("IdlePTD(%p): 0x%x\n", &IdlePTD, (int) (uintptr_t)IdlePTD);
	 */

	/*
	 * Initialize (or re-initialize) the descriptor tables for this cpu.
	 * Propagate processor mode to slave.
	 */
	/*cpu_desc_init64(cpu_datap(slot_num));*/
	mp_desc_init(slot_num);

	/* Serialize use of the slave boot stack, etc. */
	kmutex_lock(&mp_cpu_boot_lock, FALSE);

	/*istate = ml_set_interrupts_enabled(FALSE);*/
	cpu_intr_save(&eFlagsRegister);
	if (slot_num == cpu_number()) {
		/*ml_set_interrupts_enabled(istate);*/
		cpu_intr_restore(eFlagsRegister);
		/*lck_mtx_unlock(&mp_cpu_boot_lock);*/
		kmutex_unlock(&mp_cpu_boot_lock);
		return KERN_SUCCESS;
	}

	/*start_info.starter_cpu  = cpu_number();
	 *start_info.target_cpu   = slot_num;
	 *start_info.target_lapic = lapic;
	 *tsc_entry_barrier = 2;
	 *tsc_exit_barrier = 2;
     */

	/*
	 * Perform the processor startup sequence with all running
	 * processors rendezvous'ed. This is required during periods when
	 * the cache-disable bit is set for MTRR/PAT initialization.
	 */
	/*mp_rendezvous_no_intrs(start_cpu, (void *) &start_info);*/

	/*start_info.target_cpu = 0;*/

	/*ml_set_interrupts_enabled(istate);*/
	cpu_intr_restore(eFlagsRegister);
	/*lck_mtx_unlock(&mp_cpu_boot_lock);*/
	kmutex_unlock(&mp_cpu_boot_lock);

	/*if (!cpu_datap(slot_num)->cpu_running) {*/
	if(!machine_slot[slot_num].running){
		printf("Failed to start CPU %02d\n", slot_num);
		printf("Failed to start CPU %02d, rebooting...\n", slot_num);
		delay(1000000);
		halt_cpu();
		return KERN_SUCCESS;
	} else {
		//printf("Started cpu %d (lapic id %08x)\n", slot_num, lapic);
		return KERN_SUCCESS;
	}
}
/*
 * Called after all CPUs have been found, but before the VM system
 * is running.  The machine array must show which CPUs exist.
 */
void
interrupt_stack_alloc(void)
{
	int		i;
	int		cpu_count;
	vm_offset_t	stack_start;

	/*
	 * Count the number of CPUs.
	 */
	cpu_count = 0;
	for (i = 0; i < NCPUS; i++)
	    if (machine_slot[i].is_cpu)
		cpu_count++;

	/*
	 * Allocate an interrupt stack for each CPU except for
	 * the master CPU (which uses the bootstrap stack)
	 */
	if (!init_alloc_aligned(INTSTACK_SIZE*(cpu_count-1), &stack_start))
		panic("not enough memory for interrupt stacks");
	stack_start = phystokv(stack_start);

	/*
	 * Set up pointers to the top of the interrupt stack.
	 */
	for (i = 0; i < NCPUS; i++) {
	    if (i == master_cpu) {
		interrupt_stack[i] = (vm_offset_t) intstack;
		_int_stack_top[i]   = (vm_offset_t) eintstack;
	    }
	    else if (machine_slot[i].is_cpu) {
		interrupt_stack[i] = stack_start;
		_int_stack_top[i]   = stack_start + INTSTACK_SIZE;

		stack_start += INTSTACK_SIZE;
	    }
	}

	/*
	 * Set up the barrier address.  All thread stacks MUST
	 * be above this address.
	 */
	int_stack_high = stack_start;
}

/* XXX should be adjusted per CPU speed */
int simple_lock_pause_loop = 100;

unsigned int simple_lock_pause_count = 0;	/* debugging */

void
simple_lock_pause(void)
{
	static volatile int dummy;
	int i;

	simple_lock_pause_count++;

	/*
	 * Used in loops that are trying to acquire locks out-of-order.
	 */

	for (i = 0; i < simple_lock_pause_loop; i++)
	    dummy++;	/* keep the compiler from optimizing the loop away */
}

kern_return_t
cpu_control(int cpu, const int *info, unsigned int count)
{
	printf("cpu_control(%d, %p, %d) not implemented\n",
	       cpu, info, count);
	return KERN_FAILURE;
}

void
interrupt_processor(int cpu)
{
	printf("interrupt cpu %d\n",cpu);
}

kern_return_t
cpu_start(int cpu)
{
	if (machine_slot[cpu].running)
		return KERN_FAILURE;

	return intel_startCPU(cpu);
}

void
start_other_cpus(void)
{
	int cpu;

	/*TODO: Change NCPUS to ncpus variable*/
	for (cpu = 0; cpu < ncpu; cpu++)
		if (cpu != cpu_number())
			cpu_start(cpu);
}

#endif	/* NCPUS > 1 */
