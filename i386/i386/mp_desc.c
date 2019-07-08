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
#include <i386/locore.h>

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


/*
 * Addresses of bottom and top of cpu main stacks.
 */
vm_offset_t cpu_stack[NCPUS];
vm_offset_t _cpu_stack_top[NCPUS];

/*
 * Barrier address.
 */
vm_offset_t	cpu_stack_high;


static struct kmutex mp_cpu_boot_lock;
static struct kmutex ap_config_lock;

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
 * TODO: Reserve physical page to this
 */
extern void* *apboot, *apbootend;
#define AP_BOOT_ADDR (0x7000)

//cpu stack
void* stack_ptr = 0;

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

extern int lapic_addr;
extern pt_entry_t *kernel_page_dir;


/*
 * Allocate and initialize the per-processor descriptor tables.
 */

struct mp_desc_table *
mp_desc_init(int mycpu)
{
    struct mp_desc_table *mpt;

    if (mycpu == master_cpu)
        {
            /*
             * Master CPU uses the tables built at boot time.
             * Just set the TSS and GDT pointers.
             */
            mp_ktss[mycpu] = (struct task_tss *) &ktss;
            mp_gdt[mycpu] = gdt;
            return 0;
        }
    else
        {
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

static void send_ipi(unsigned icr_h, unsigned icr_l)
{
    lapic->icr_high.r = icr_h;
    lapic->icr_low.r = icr_l;
}


/*TODO: Add delay between IPI*/
void startup_cpu(uint32_t apic_id)
{
    unsigned icr_h = 0;
    unsigned icr_l = 0;

    //send INIT Assert IPI
    icr_h = (apic_id << 24);
    icr_l = (INIT << 8) | (ASSERT << 14) | (LEVEL << 15);
    send_ipi(icr_h, icr_l);

    dummyf(lapic->apic_id.r);

    //wait until IPI is sent
    delay(10000);
    while( ( (lapic->icr_low.r >> 12) & 1) == SEND_PENDING);

    //Send INIT De-Assert IPI
    icr_h = 0;
    icr_l = 0;
    icr_h = (apic_id << 24);
    icr_l = (INIT << 8) | (DE_ASSERT << 14) | (LEVEL << 15);
    send_ipi(icr_h, icr_l);

    dummyf(lapic->apic_id.r);

    //wait until IPI is sent
    delay(10000);
    while( ( (lapic->icr_low.r >> 12) & 1) == SEND_PENDING);

    //Send StartUp IPI
    icr_h = 0;
    icr_l = 0;
    icr_h = (apic_id << 24);
    icr_l = (STARTUP << 8) | ((AP_BOOT_ADDR >>12) & 0xff);
    send_ipi(icr_h, icr_l);

    dummyf(lapic->apic_id.r);

    //wait until IPI is sent
    delay(1000);
    while( ( (lapic->icr_low.r >> 12) & 1) == SEND_PENDING);

    //Send second StartUp IPI
    icr_h = 0;
    icr_l = 0;
    icr_h = (apic_id << 24);
    icr_l = (STARTUP << 8) | ((AP_BOOT_ADDR >>12) & 0xff);
    send_ipi(icr_h, icr_l);

    dummyf(lapic->apic_id.r);

    //wait until IPI is sent
    delay(1000);
    while( ( (lapic->icr_low.r >> 12) & 1) == SEND_PENDING);

}

int
cpu_setup()
{

    int i = 1;
    int kernel_id = 0;

    kmutex_init(&ap_config_lock);
    kmutex_lock(&ap_config_lock, FALSE);

    while(i < ncpu && (machine_slot[i].running == TRUE)) i++;

    /* assume Pentium 4, Xeon, or later processors */
    //unsigned apic_id = (((ApicLocalUnit*)phystokv(lapic_addr))->apic_id.r >> 24) & 0xff;
    unsigned apic_id = lapic->apic_id.r;

    /* panic? */
    if(i >= ncpu)
        return -1;

    /*TODO: Move this code to a separate function*/



    /* Update apic2kernel and machine_slot with the newest apic_id */
    if(apic2kernel[machine_slot[i].apic_id] == i)
        {
            apic2kernel[machine_slot[i].apic_id] = -1;
        }

    apic2kernel[apic_id] = i;
    machine_slot[i].apic_id =  apic_id;

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
    gdt_init();
    idt_init();
    ktss_init();
    slave_main();

    kmutex_unlock(&ap_config_lock);

    kernel_id = cpu_number();
    printf("cpu %d enabled\n", kernel_id);

    return 0;
}

void paging_setup(){

#if PAE
    set_cr3(pdpbase_addr);
#ifndef	MACH_HYP
    if (!CPU_HAS_FEATURE(CPU_FEATURE_PAE))
        set_cr4(get_cr4() | CR4_PAE);
#endif	/* MACH_HYP */
#else
    set_cr3(kernel_page_dir_addr);
#endif	/* PAE */
#ifndef	MACH_HYP
    /* Turn paging on.
     * Also set the WP bit so that on 486 or better processors
     * page-level write protection works in kernel mode.
     */
    set_cr0(get_cr0() | CR0_PG | CR0_WP);
    set_cr0(get_cr0() & ~(CR0_CD | CR0_NW));

    if (CPU_HAS_FEATURE(CPU_FEATURE_PGE))
        set_cr4(get_cr4() | CR4_PGE);

#endif	/* MACH_HYP */

    flush_instr_queue();
    flush_tlb();

}

int
cpu_ap_main()
{

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

    /* Serialize use of the slave boot stack, etc. */
    kmutex_lock(&mp_cpu_boot_lock, FALSE);

    /*istate = ml_set_interrupts_enabled(FALSE);*/
    cpu_intr_save(&eFlagsRegister);
    if (slot_num == cpu_number())
        {
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

    /*
     * Initialize (or re-initialize) the descriptor tables for this cpu.
     * Propagate processor mode to slave.
     */
    /*cpu_desc_init64(cpu_datap(slot_num));*/
    mp_desc_init(slot_num);

    /*ml_set_interrupts_enabled(istate);*/
    cpu_intr_restore(eFlagsRegister);
    /*lck_mtx_unlock(&mp_cpu_boot_lock);*/
    kmutex_unlock(&mp_cpu_boot_lock);

    delay(1000000);

    /*if (!cpu_datap(slot_num)->cpu_running) {*/
    if(!machine_slot[slot_num].running)
        {
            printf("Failed to start CPU %02d, rebooting...\n", slot_num);
            halt_cpu();
            return KERN_SUCCESS;
        }
    else
        {
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
    for (i = 0; i < ncpu; i++)
        {
            if (i == master_cpu)
                {
                    interrupt_stack[i] = (vm_offset_t) intstack;
                    _int_stack_top[i]   = (vm_offset_t) eintstack;
                }
            else if (machine_slot[i].is_cpu)
                {
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
    vm_offset_t	stack_start;
    int apic_id = lapic->apic_id.r;
    extern pt_entry_t *kernel_page_dir;
    extern int nb_direct_value;
    int i = 0;

    printf("found %d cpus\n", ncpu);
    printf("The current cpu is: %d\n", cpu_number());

    //copy start routine
    /*TODO: Copy the routine in a physical page */
    memcpy((void*)phystokv(AP_BOOT_ADDR), (void*) &apboot, (uint32_t)&apbootend - (uint32_t)&apboot);

    //update BSP machine_slot and apic2kernel
    machine_slot[0].apic_id = apic_id;
    apic2kernel[apic_id] = 0;

    //Reserve memory for cpu stack
    if (!init_alloc_aligned(STACK_SIZE*(ncpu-1), &stack_start))
        panic("not enough memory for cpu stacks");
    stack_start = phystokv(stack_start);


    for (cpu = 0; cpu < ncpu; cpu++)
        {
            if (cpu != cpu_number())
                {
                    //Initialize cpu stack
                    cpu_stack[cpu] = stack_start;
                    _cpu_stack_top[cpu] = stack_start + STACK_SIZE;

                    stack_ptr = cpu_stack[cpu];

                    machine_slot[cpu].running = FALSE;
                    cpu_start(cpu);

                    stack_start += STACK_SIZE;
                }
        }

    /* Get rid of the temporary direct mapping and flush it out of the TLB.  */
    for (i = 0 ; i < nb_direct_value; i++){
        kernel_page_dir[lin2pdenum_cont(INIT_VM_MIN_KERNEL_ADDRESS) + i] = 0;
    }
}

#endif	/* NCPUS > 1 */
