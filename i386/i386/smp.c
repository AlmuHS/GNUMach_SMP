/* smp.h - i386 SMP controller for Mach
   Copyright (C) 2020 Free Software Foundation, Inc.
   Written by Almudena Garcia Jurado-Centurion

   This file is part of GNU Mach.

   GNU Mach is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU Mach is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include <i386/apic.h>
#include <i386/smp.h>
#include <i386/cpu.h>
#include <i386/pit.h>
#include <i386at/idt.h>
#include <i386at/acpi_parse_apic.h>
#include <kern/printf.h>
#include <mach/machine.h>

#include <kern/smp.h>

#define pause_memory	asm volatile ("pause" : : : "memory")

/*
 * smp_data_init: initialize smp_data structure
 * Must be called after smp_init(), once all APIC structures
 * has been initialized
 */
static void smp_data_init(void)
{
    uint8_t numcpus = apic_get_numcpus();
    smp_set_numcpus(numcpus);

    for(int i = 0; i < numcpus; i++){
            machine_slot[i].is_cpu = TRUE;
    }

}

void smp_pmap_update(unsigned apic_id)
{
    unsigned long flags;

    cpu_intr_save(&flags);

    printf("Sending IPI(%u) to call TLB shootdown...", apic_id);
    apic_send_ipi(NO_SHORTHAND, FIXED, PHYSICAL, ASSERT, EDGE, CALL_SINGLE_FUNCTION_BASE, apic_id);

    do {
        pause_memory;
    } while(lapic->icr_low.delivery_status == SEND_PENDING);

    printf("done\n");

    cpu_intr_restore(flags);
}

/* See Intel IA32/64 Software Developer's Manual 3A Section 8.4.4.1 */
void smp_startup_cpu(unsigned apic_id, unsigned vector)
{
    /* Clear APIC errors */
    lapic->error_status.r = 0;

    printf("Sending IPIs to APIC ID %u...", apic_id);

    /* Assert INIT IPI */
    apic_send_ipi(NO_SHORTHAND, INIT, PHYSICAL, ASSERT, LEVEL, 0, apic_id);

    /* Wait for delivery */
    do {
        pause_memory;
    } while(lapic->icr_low.delivery_status == SEND_PENDING);

    /* Deassert INIT IPI */
    apic_send_ipi(NO_SHORTHAND, INIT, PHYSICAL, DE_ASSERT, LEVEL, 0, apic_id);

    /* Wait for delivery */
    do {
        pause_memory;
    } while(lapic->icr_low.delivery_status == SEND_PENDING);

    /* Wait 10 msec */
    pit_mdelay(10);

    /* Clear APIC errors */
    lapic->error_status.r = 0;

    /* First StartUp IPI */
    apic_send_ipi(NO_SHORTHAND, STARTUP, PHYSICAL, ASSERT, LEVEL, vector >> 12, apic_id);

    /* Wait 200 usec */
    pit_udelay(200);

    /* Wait for delivery */
    do {
        pause_memory;
    } while(lapic->icr_low.delivery_status == SEND_PENDING);

    /* Second StartUp IPI */
    apic_send_ipi(NO_SHORTHAND, STARTUP, PHYSICAL, ASSERT, LEVEL, vector >> 12, apic_id);

    /* Wait 200 usec */
    pit_udelay(200);

    /* Wait for delivery */
    do {
        pause_memory;
    } while(lapic->icr_low.delivery_status == SEND_PENDING);

    printf("done\n");
}

/*
 * smp_init: initialize the SMP support, starting the cpus searching
 * and enumeration.
 */
int smp_init(void)
{
    smp_data_init();

    return 0;
}
