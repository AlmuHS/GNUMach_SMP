#include <kern/kmutex.h>
#include <kern/cpu_number.h>
#include <i386/gdt.h>
#include <i386/ldt.h>
#include <i386/i386/pcb.h>
#include <i386/i386/tss.h>
#include <i386/i386/cpu.h>
#include <i386/i386/model_dep.h>
#include <kern/printf.h>

#include <stdlib.h>



static struct kmutex mp_cpu_boot_lock;
extern int apic2kernel[];
extern int kernel2apic[];


kern_return_t intel_startCPU(int slot_num)
{
	/*int	lapic = cpu_to_lapic[slot_num];*/
	int lapic = kernel2apic[slot_num];
	unsigned long eFlagsRegister;

	kmutex_init(&mp_cpu_boot_lock);

	assert(lapic != -1);

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
		kprintf("Started cpu %d (lapic id %08x)\n", slot_num, lapic);
		return KERN_SUCCESS;
	}
}
