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
#include <string.h>
#include <include/stdint.h> //uint16_t, uint32_t_t...
#include <imps/apic.h>

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
 * Address of cpu start routine, to skip to protected mode after startup IPI
 */
extern void* *apboot, *apbootend;
#define AP_BOOT_ADDR (0x7000)	

//cpu stack
extern void* *stack_ptr;
extern void *stack_bsp;

//Little trace
extern unsigned counter;

//ICR Destination mode
#define PHYSICAL 0
#define LOGICAL 1

//ICR Delivery mode
#define STARTUP 6
#define INIT 5

//ICR Level
#define DE_ASSERT 0
#define ASSERT 1

//ICR Trigger mode
#define EDGE 0
#define LEVEL 1

//ICR Destination Shorthand
#define NO_SHORTHAND 0

#define SEND_PENDING 1

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

static void send_IPI(unsigned icr_h, unsigned icr_l){
    lapic->icr_high.r = icr_h;
    lapic->icr_low.r = icr_l;    
}


/*TODO: Add delay between IPI*/
void startup_cpu(uint32_t apic_id){	    
    unsigned icr_h = 0;
    unsigned icr_l = 0;

    //send INIT Assert IPI
    icr_h = (apic_id << 24);
    icr_l = (INIT << 8) | (ASSERT << 14) | (LEVEL << 15); 
    send_IPI(icr_h, icr_l);

    dummyf(lapic->apic_id.r);	

    //wait until IPI is sent
    delay(10000);
    while( ( (lapic->icr_low.r >> 12) & 1) == SEND_PENDING);

    //Send INIT De-Assert IPI
    icr_h = 0; icr_l = 0;
    icr_h = (apic_id << 24);
    icr_l = (INIT << 8) | (DE_ASSERT << 14) | (LEVEL << 15);
    send_IPI(icr_h, icr_l);

    dummyf(lapic->apic_id.r);

    //wait until IPI is sent
    delay(10000);
    //while( ( (lapic->icr_low.r >> 12) & 1) == SEND_PENDING);

    //Send StartUp IPI
    icr_h = 0; icr_l = 0;
    icr_h = (apic_id << 24);
    icr_l = (STARTUP << 8) | ((AP_BOOT_ADDR >>12) & 0xff);
    send_IPI(icr_h, icr_l);

    dummyf(lapic->apic_id.r);

    //wait until IPI is sent
    delay(1000);
    //while( ( (lapic->icr_low.r >> 12) & 1) == SEND_PENDING);

    //Send second StartUp IPI
    icr_h = 0; icr_l = 0;
    icr_h = (apic_id << 24);
    icr_l = (STARTUP << 8) | ((AP_BOOT_ADDR >>12) & 0xff);
    send_IPI(icr_h, icr_l);

    dummyf(lapic->apic_id.r);

    //wait until IPI is sent
    delay(1000);
    //while( ( (lapic->icr_low.r >> 12) & 1) == SEND_PENDING);

}

int
cpu_setup(){

    int i = 0;
    while(i < ncpu && (machine_slot[i].running == TRUE)) i++;

    /* panic? */
    if(i >= ncpu)
	return -1;

  /*TODO: Move this code to a separate function*/

    /* assume Pentium 4, Xeon, or later processors */
	machine_slot[i].apic_id = (lapic->apic_id.r >> 24) & 0xff;
	machine_slot[i].running = TRUE;
	machine_slot[i].is_cpu = TRUE;
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
	
    return 0;
}


void
cpu_ap_main(){

	  unsigned * count = (unsigned *) phystokv(AP_BOOT_ADDR + (unsigned)&counter - (unsigned)&apboot); 
   (*count)++;
   
    for(;;)
        asm volatile("hlt");

    printf("\nstarting cpu: %d\n", cpu_number());

   if(cpu_setup())
        goto idle;

idle:
    for(;;)
        asm volatile("hlt");
}

/*TODO: Reimplement function to send Startup IPI to cpu*/
kern_return_t intel_startCPU(int slot_num)
{
	/*TODO: Get local APIC from cpu*/	
	int lapic_id = machine_slot[slot_num].apic_id;
	unsigned long eFlagsRegister;

	kmutex_init(&mp_cpu_boot_lock);
	printf("Trying to enable: %d\n", lapic_id);


	//assert(lapic != -1);

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

	/*
	 * Perform the processor startup sequence with all running
	 * processors rendezvous'ed. This is required during periods when
	 * the cache-disable bit is set for MTRR/PAT initialization.
	 */
	/*mp_rendezvous_no_intrs(start_cpu, (void *) &start_info);*/
	startup_cpu(lapic_id);	

	/*ml_set_interrupts_enabled(istate);*/
	cpu_intr_restore(eFlagsRegister);
	/*lck_mtx_unlock(&mp_cpu_boot_lock);*/
	kmutex_unlock(&mp_cpu_boot_lock);

  delay(1000000);

	/*if (!cpu_datap(slot_num)->cpu_running) {*/
	if(!machine_slot[slot_num].running){
		printf("Failed to start CPU %02d\n", slot_num);
		printf("Failed to start CPU %02d, rebooting...\n", slot_num);
		delay(1000000);
		unsigned * count = (unsigned *) phystokv(AP_BOOT_ADDR + (unsigned)&counter - (unsigned)&apboot);	
		printf("counter: %x: %x\n", (unsigned)count, *count);
		halt_cpu();
		return KERN_SUCCESS;
	} else {
		printf("Started cpu %d (lapic id %08x)\n", slot_num, lapic_id);
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
	vm_offset_t	stack_start;
	

	/*
	 * Allocate an interrupt stack for each CPU except for
	 * the master CPU (which uses the bootstrap stack)
	 */
	if (!init_alloc_aligned(INTSTACK_SIZE*(ncpu-1), &stack_start))
		panic("not enough memory for interrupt stacks");
	stack_start = phystokv(stack_start);

	/*
	 * Set up pointers to the top of the interrupt stack.
	 */
	for (i = 0; i < ncpu; i++) {
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
	printf("found %d cpus\n", ncpu);
	printf("The current cpu is: %d\n", cpu_number());

	//copy start routine
	memcpy((void*)phystokv(AP_BOOT_ADDR), (void*) &apboot, (uint32_t)&apbootend - (uint32_t)&apboot);

	//Initialize cpu stack
	#define STACK_SIZE (4096 * 2)
	*stack_ptr = kalloc(STACK_SIZE);

	for (cpu = 0; cpu < ncpu; cpu++){
		if (cpu != cpu_number()){
            machine_slot[cpu].running = FALSE;
			cpu_start(cpu);
		}
	}
}

#endif	/* NCPUS > 1 */
