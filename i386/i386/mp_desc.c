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

#include <kern/cpu_number.h>
#include <kern/debug.h>
#include <kern/printf.h>
#include <kern/smp.h>
#include <kern/kmutex.h>
#include <mach/machine.h>
#include <mach/xen.h>
#include <vm/vm_kern.h>

#include <i386/mp_desc.h>
#include <i386/lock.h>
#include <i386/apic.h>
#include <i386/locore.h>
#include <i386/gdt.h>
#include <i386/cpu.h>

#include <i386at/model_dep.h>
#include <machine/ktss.h>
#include <machine/smp.h>
#include <machine/tss.h>
#include <machine/io_perm.h>
#include <machine/vm_param.h>

#include <i386at/acpi_parse_apic.h>
#include <string.h>

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
vm_offset_t     cpu_stack_high;

/*
 * First cpu`s interrupt stack.
 */
extern char		_intstack[];	/* bottom */
extern char		_eintstack[];	/* top */

/*
 * First cpu`s stack.
 */

char		_cpustack[];	/* bottom */
char		_ecpustack[];	/* top */


extern void *apboot, *apbootend;
extern volatile ApicLocalUnit* lapic;
//extern unsigned stop;

#define AP_BOOT_ADDR (0x7000)
#define STACK_SIZE (4096 * 2)
//#define STACK_SIZE (2*I386_PGBYTES)

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

static struct kmutex mp_cpu_boot_lock;

/*
 * Allocate and initialize the per-processor descriptor tables.
 */

struct mp_desc_table *
mp_desc_init(int mycpu)
{
	struct mp_desc_table *mpt;

        const int master_cpu = 0;

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

kern_return_t intel_startCPU(int cpu)
{
    /*TODO: Get local APIC from cpu*/
    unsigned apic_id = apic_get_cpu_apic_id(cpu);
    unsigned long eFlagsRegister;

    kmutex_init(&mp_cpu_boot_lock);
    printf("Trying to enable: %d\n", apic_id);

    /* Serialize use of the slave boot stack, etc. */
    kmutex_lock(&mp_cpu_boot_lock, FALSE);

    /*istate = ml_set_interrupts_enabled(FALSE);*/
    cpu_intr_save(&eFlagsRegister);
    if (cpu == cpu_number())
        {
            cpu_intr_restore(eFlagsRegister);
            kmutex_unlock(&mp_cpu_boot_lock);
            return KERN_SUCCESS;
        }

    /*
     * Perform the processor startup sequence with all running
     * processors rendezvous'ed. This is required during periods when
     * the cache-disable bit is set for MTRR/PAT initialization.
     */
    /*mp_rendezvous_no_intrs(start_cpu, (void *) &start_info);*/
    smp_startup_cpu(apic_id, AP_BOOT_ADDR);

    cpu_intr_restore(eFlagsRegister);
    kmutex_unlock(&mp_cpu_boot_lock);

    delay(1000000000000000);

    /*
     * Initialize (or re-initialize) the descriptor tables for this cpu.
     * Propagate processor mode to slave.
     */
    mp_desc_init(cpu);

    /*if (!cpu_datap(slot_num)->cpu_running) {*/
    if(machine_slot[cpu].running == FALSE)
        {
            printf("Failed to start CPU %02d, rebooting...\n", cpu);
            //halt_cpu();
            return KERN_SUCCESS;
        }
    else
        {
            printf("Started cpu %d (lapic id %08x)\n", cpu, apic_id);
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

	cpu_count = smp_get_numcpus();
	
	/*
	 * Allocate an interrupt stack for each CPU except for
	 * the master CPU (which uses the bootstrap stack)
	 */
	if(cpu_count > 1){
	        if (!init_alloc_aligned(INTSTACK_SIZE*(cpu_count-1), &stack_start))
		        panic("not enough memory for interrupt stacks");
	        stack_start = phystokv(stack_start);
        }
	/*
	 * Set up pointers to the top of the interrupt stack.
	 */
	 
	for (i = master_cpu; i < cpu_count; i++) {
	    if (i == 0) {
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
	if(cpu_count > 1) int_stack_high = stack_start;
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
cpu_setup()
{

    int i = 0;
    int ncpus = smp_get_numcpus();

    //unsigned apic_id = apic_get_current_cpu();
    unsigned apic_id = (((ApicLocalUnit*)phystokv(lapic_addr))->apic_id.r >> 24) & 0xff;
    printf("Starting cpu with APIC ID %u setup\n", apic_id);
    
    printf("ncpus = %d\n", ncpus);
    
    int cpu = apic_get_cpu_kernel_id(apic_id);
    machine_slot[cpu].running = TRUE;

    /*TODO: Move this code to a separate function*/

    /* Initialize machine_slot fields with the cpu data */
    machine_slot[cpu].cpu_subtype = CPU_SUBTYPE_AT386;

    int cpu_type = discover_x86_cpu_type ();

    switch (cpu_type)
        {
        default:
            printf("warning: unknown cpu type %d, assuming i386\n", cpu_type);

        case 3:
            machine_slot[cpu].cpu_type = CPU_TYPE_I386;
            break;

        case 4:
            machine_slot[cpu].cpu_type = CPU_TYPE_I486;
            break;

        case 5:
            machine_slot[cpu].cpu_type = CPU_TYPE_PENTIUM;
            break;
        case 6:
        case 15:
            machine_slot[cpu].cpu_type = CPU_TYPE_PENTIUMPRO;
            break;
        }
        
   printf("Configuring GDT and IDT\n");

    /*
     * Initialize and activate the real i386 protected-mode structures.
     */
    gdt_init();
    printf("GDT configured\n");
    
    idt_init();
    printf("IDT configured\n");
    
    ldt_init();
    printf("LDT configured\n");
    
    ktss_init();
    printf("KTSS configured\n");
    printf("Configured GDT and IDT\n");

    
    
    printf("started cpu %d\n", i);

    /* Add cpu to the kernel */
    //slave_main();

    return 0;
}

int
cpu_ap_main()
{
    //unsigned lapic_addr = apic_madt->lapic_addr;
    unsigned cpu = (((ApicLocalUnit*)phystokv(lapic_addr))->apic_id.r >> 24) & 0xff;
    printf("Enabling cpu %d\n", cpu);

    if(cpu_setup()) return -1;
    return 0;
}

kern_return_t
cpu_start(int cpu)
{
    if (machine_slot[cpu].running == TRUE)
        return KERN_FAILURE;

    return intel_startCPU(cpu);
}



void
cpus_stack_alloc(void)
{
        vm_offset_t stack_start;
        int ncpus = smp_get_numcpus();
        
        
        if(ncpus > 1){
                if (!init_alloc_aligned(STACK_SIZE*(ncpus-1), &stack_start))
                        panic("not enough memory for cpu stacks");
                stack_start = phystokv(stack_start);
        }
        
        
        for (int i = 1; i < ncpus; i++)
        {
            if (i == master_cpu)
                {
                     cpu_stack[i] = (vm_offset_t) _cpustack;
                     _cpu_stack_top[i] = (vm_offset_t) _cpustack + STACK_SIZE;
                }
            else
                {
                    cpu_stack[i] = stack_start;
                    _cpu_stack_top[i]  = stack_start + STACK_SIZE;

                    stack_start += STACK_SIZE;
                }
        }

    /*
     * Set up the barrier address.  All thread stacks MUST
     * be above this address.
     */
    if(ncpus > 1) cpu_stack_high = stack_start;
}

extern vm_offset_t* *stack_ptr;

void
start_other_cpus(void)
{              
	int ncpus = smp_get_numcpus();

        machine_slot[0].running = TRUE;

        //Copy cpus initialization assembly routine
	memcpy((void*)phystokv(AP_BOOT_ADDR), (void*) &apboot, (uint32_t)&apbootend - (uint32_t)&apboot);
	
	//Reserve memory for interrupt stacks
	interrupt_stack_alloc();

        //Reserve memory for cpu stack
        cpus_stack_alloc();
        printf("cpu stacks reserved\n");

        printf("starting cpus\n");
	unsigned cpu;
	for (cpu = 1; cpu < ncpus; cpu++){
                //Initialize stack pointer for current cpu
                *stack_ptr = (vm_offset_t*) cpu_stack[cpu];
              
                machine_slot[cpu].running = FALSE;
                
                //Start cpu
                printf("starting cpu %d\n", cpu);
                cpu_start(cpu);
                
                while(machine_slot[cpu].running != TRUE);
	}	
}

#endif	/* NCPUS > 1 */
