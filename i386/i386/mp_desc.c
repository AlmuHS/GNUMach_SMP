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

#include <kern/cpu_number.h>
#include <kern/debug.h>
#include <kern/printf.h>
#include <kern/smp.h>
#include <kern/startup.h>
#include <kern/kmutex.h>
#include <mach/machine.h>
#include <mach/xen.h>
#include <vm/vm_kern.h>

#include <i386/mp_desc.h>
#include <i386/lock.h>
#include <i386/apic.h>
#include <i386/locore.h>
#include <i386/fpu.h>
#include <i386/gdt.h>
#include <i386at/idt.h>
#include <i386at/int_init.h>
#include <i386/cpu.h>
#include <i386/smp.h>

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
vm_offset_t	int_stack_top[NCPUS];
vm_offset_t	int_stack_base[NCPUS];

/*
 * Whether we are currently handling an interrupt.
 * To catch code erroneously taking non-irq-safe locks.
 */
#ifdef MACH_LDEBUG
unsigned long	in_interrupt[NCPUS];
#endif

/* Interrupt stack allocation */
uint8_t solid_intstack[NCPUS*INTSTACK_SIZE] __aligned(NCPUS*INTSTACK_SIZE);

void
interrupt_stack_alloc(void)
{
	int i;

	/*
	 * Set up pointers to the top of the interrupt stack.
	 */

	for (i = 0; i < NCPUS; i++) {
		int_stack_base[i] = (vm_offset_t) &solid_intstack[i * INTSTACK_SIZE];
		int_stack_top[i] = (vm_offset_t) &solid_intstack[(i + 1) * INTSTACK_SIZE] - 4;
	}
}

#if	NCPUS > 1
/*
 * Flag to mark SMP init by BSP complete
 */
int bspdone;

phys_addr_t apboot_addr;
extern void *apboot, *apbootend;
extern volatile ApicLocalUnit* lapic;

/*
 * Multiprocessor i386/i486 systems use a separate copy of the
 * GDT, IDT, LDT, and kernel TSS per processor.  The first three
 * are separate to avoid lock contention: the i386 uses locked
 * memory cycles to access the descriptor tables.  The TSS is
 * separate since each processor needs its own kernel stack,
 * and since using a TSS marks it busy.
 */

/*
 * Descriptor tables.
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

int
mp_desc_init(int mycpu)
{
	struct mp_desc_table *mpt;
	vm_offset_t mem;

	if (mycpu == 0) {
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
		 * Allocate tables for other CPUs
		 */
		if (!init_alloc_aligned(sizeof(struct mp_desc_table), &mem))
			panic("not enough memory for descriptor tables");
		mpt = (struct mp_desc_table *)phystokv(mem);

		mp_desc_table[mycpu] = mpt;
		mp_ktss[mycpu] = &mpt->ktss;
		mp_gdt[mycpu] = mpt->gdt;

		/*
		 * Zero the tables
		 */
		memset(mpt->idt, 0, sizeof(idt));
		memset(mpt->gdt, 0, sizeof(gdt));
		memset(mpt->ldt, 0, sizeof(ldt));
		memset(&mpt->ktss, 0, sizeof(struct task_tss));

		return mycpu;
	}
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
	smp_pmap_update(apic_get_cpu_apic_id(cpu));
}

static void
paging_enable(void)
{
#ifndef MACH_HYP
    /* Turn paging on.
     * TODO: Why does setting the WP bit here cause a crash?
     */
#if PAE
    set_cr4(get_cr4() | CR4_PAE);
#endif
    set_cr0(get_cr0() | CR0_PG /* | CR0_WP */);
    set_cr0(get_cr0() & ~(CR0_CD | CR0_NW));
    if (CPU_HAS_FEATURE(CPU_FEATURE_PGE))
        set_cr4(get_cr4() | CR4_PGE);
#endif  /* MACH_HYP */
}

void
cpu_setup(int cpu)
{
    pmap_make_temporary_mapping();
    printf("AP=(%u) tempmap done\n", cpu);

    paging_enable();
    flush_instr_queue();
    printf("AP=(%u) paging done\n", cpu);

    init_percpu(cpu);
    mp_desc_init(cpu);
    printf("AP=(%u) mpdesc done\n", cpu);

    ap_gdt_init(cpu);
    printf("AP=(%u) gdt done\n", cpu);

    ap_idt_init(cpu);
    printf("AP=(%u) idt done\n", cpu);

    ap_int_init(cpu);
    printf("AP=(%u) int done\n", cpu);

    ap_ldt_init(cpu);
    printf("AP=(%u) ldt done\n", cpu);

    ap_ktss_init(cpu);
    printf("AP=(%u) ktss done\n", cpu);

    pmap_remove_temporary_mapping();
    printf("AP=(%u) remove tempmap done\n", cpu);

    pmap_set_page_dir();
    flush_tlb();
    printf("AP=(%u) reset page dir done\n", cpu);

    /* Initialize machine_slot fields with the cpu data */
    machine_slot[cpu].cpu_subtype = CPU_SUBTYPE_AT386;
    machine_slot[cpu].cpu_type = machine_slot[0].cpu_type;

    init_fpu();
    lapic_setup();
    lapic_enable();
    cpu_launch_first_thread(THREAD_NULL);
}

void
cpu_ap_main()
{
    int cpu = cpu_number();

    do {
	cpu_pause();
    } while (bspdone != cpu);

    __sync_synchronize();

    cpu_setup(cpu);
}

kern_return_t
cpu_start(int cpu)
{
    int err;

    assert(machine_slot[cpu].running != TRUE);

    uint16_t apic_id = apic_get_cpu_apic_id(cpu);

    printf("Trying to enable: %d at 0x%lx\n", apic_id, apboot_addr);

    err = smp_startup_cpu(apic_id, apboot_addr);

    if (!err) {
        printf("Started cpu %d (lapic id %04x)\n", cpu, apic_id);
        return KERN_SUCCESS;
    }
    printf("FATAL: Cannot init AP %d\n", cpu);
    for (;;);
}

void
start_other_cpus(void)
{
	int ncpus = smp_get_numcpus();

	//Copy cpu initialization assembly routine
	memcpy((void*) phystokv(apboot_addr), (void*) &apboot,
	       (uint32_t)&apbootend - (uint32_t)&apboot);

	unsigned cpu;

	splhigh();

	/* Disable IOAPIC interrupts (IPIs not affected).
	 * Clearing this flag is similar to masking all
	 * IOAPIC interrupts individually.
	 *
	 * This is done to prevent IOAPIC interrupts from
	 * interfering with SMP startup. splhigh() may be enough for BSP,
	 * but I'm not sure.  We cannot control the lapic
	 * on APs because we don't have execution on them yet.
	 */
	lapic_disable();

	bspdone = 0;
	for (cpu = 1; cpu < ncpus; cpu++) {
		machine_slot[cpu].running = FALSE;

		//Start cpu
		printf("Starting AP %d\n", cpu);
		cpu_start(cpu);

		bspdone++;
		do {
			cpu_pause();
		} while (machine_slot[cpu].running == FALSE);

		__sync_synchronize();
	}
	printf("BSP: Completed SMP init\n");

	/* Re-enable IOAPIC interrupts as per setup */
	lapic_enable();
}
#endif	/* NCPUS > 1 */
