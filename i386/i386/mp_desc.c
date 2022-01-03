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

#include <kern/cpu_number.h>
#include <kern/debug.h>
#include <kern/printf.h>
#include <mach/machine.h>
#include <mach/xen.h>
#include <vm/vm_kern.h>

#include <i386/mp_desc.h>
#include <i386/lock.h>
#include <i386/apic.h>

#include <i386at/model_dep.h>
#include <machine/ktss.h>
#include <machine/smp.h>
#include <machine/tss.h>
#include <machine/io_perm.h>
#include <machine/vm_param.h>

/*
 * The i386 needs an interrupt stack to keep the PCB stack from being
 * overrun by interrupts.  All interrupt stacks MUST lie at lower addresses
 * than any thread`s kernel stack.
 */

/*
 * Addresses of bottom and top of interrupt stacks.
 */
vm_offset_t	interrupt_stack[NCPUS];
vm_offset_t	int_stack_top[NCPUS];
vm_offset_t	int_stack_base[NCPUS];

/*
 * Addresses of bottom and top of cpu main stacks.
 */
vm_offset_t cpu_stack[NCPUS];
vm_offset_t _cpu_stack_top[NCPUS];

/*
 * Barrier address.
 */
vm_offset_t	int_stack_high;

/*
 * First cpu`s interrupt stack.
 */
extern char		_intstack[];	/* bottom */
extern char		_eintstack[];	/* top */

extern void *apboot, *apbootend;
void* stack_ptr = 0;

#define AP_BOOT_ADDR (0x7000)
#define STACK_SIZE (4096 * 2)

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
		_fill_gdt_sys_descriptor(mpt->gdt, KERNEL_LDT,
			(unsigned)&mpt->ldt,
			LDTSZ * sizeof(struct real_descriptor) - 1,
			ACC_P|ACC_PL_K|ACC_LDT, 0);
		_fill_gdt_sys_descriptor(mpt->gdt, KERNEL_TSS,
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

kern_return_t intel_startCPU(int slot_num)
{
	printf("TODO: intel_startCPU\n");
	mp_desc_init(slot_num);
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
		interrupt_stack[i] = (vm_offset_t) _intstack;
		int_stack_top[i]   = (vm_offset_t) _eintstack;
	    }
	    else if (machine_slot[i].is_cpu) {
		interrupt_stack[i] = stack_start;
		int_stack_top[i]   = stack_start + INTSTACK_SIZE;

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

int
cpu_ap_main()
{
    if(cpu_setup()) return -1;
    return 0;
}


int
cpu_setup()
{

    int i = 1;
    int kernel_id = 0;
    int ncpus = apic_get_numcpus();


    while(i < ncpus && (machine_slot[i].running == TRUE)) i++;

    /* assume Pentium 4, Xeon, or later processors */
    unsigned apic_id = apic_get_current_cpu();

    /* panic? */
    if(i >= ncpus)
        return -1;

    /*TODO: Move this code to a separate function*/

    /* Initialize machine_slot fields with the cpu data */
    machine_slot[i].running = TRUE;
    machine_slot[i].cpu_subtype = CPU_SUBTYPE_AT386;

    int cpu_type = discover_x86_cpu_type ();

    switch (cpu_type)
        {
        default:
            printf("warning: unknown cpu type %d, assuming i386\n", cpu_type);

        case 3:
            machine_slot[i].cpu_type = CPU_TYPE_I386;
            break;

        case 4:
            machine_slot[i].cpu_type = CPU_TYPE_I486;
            break;

        case 5:
            machine_slot[i].cpu_type = CPU_TYPE_PENTIUM;
            break;
        case 6:
        case 15:
            machine_slot[i].cpu_type = CPU_TYPE_PENTIUMPRO;
            break;
        }

    /*
     * Initialize and activate the real i386 protected-mode structures.
     */
    gdt_init();
    idt_init();
    ldt_init();
    ktss_init();

    /* Add cpu to the kernel */
    //slave_main();

    return 0;
}

kern_return_t
cpu_start(int cpu)
{
	if (machine_slot[cpu].running)
		return KERN_FAILURE;

	int apic_id = apic_get_cpu_apic_id(cpu);
	smp_startup_cpu(apic_id, AP_BOOT_ADDR);

	return intel_startCPU(cpu);
}

void
start_other_cpus(void)
{              
	vm_offset_t	stack_start;
	int ncpus = apic_get_numcpus();

	memcpy((void*)phystokv(AP_BOOT_ADDR), (void*) &apboot, (uint32_t)&apbootend - (uint32_t)&apboot);
	interrupt_stack_alloc();

	//Reserve memory for cpu stack
	if (!init_alloc_aligned(STACK_SIZE*(ncpus-1), &stack_start))
	panic("not enough memory for cpu stacks");
	stack_start = phystokv(stack_start);


	int cpu;
	for (cpu = 1; cpu < ncpus; cpu++){
        cpu_stack[cpu-1] = stack_start;
        _cpu_stack_top[cpu-1] = stack_start + STACK_SIZE - 1;

        stack_ptr = cpu_stack[cpu-1];
		cpu_start(cpu);
	}
		
}

#endif	/* NCPUS > 1 */
