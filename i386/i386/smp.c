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

#include <string.h>
#include <i386/apic.h>
#include <i386/smp.h>
#include <i386/cpu.h>
#include <i386/pio.h>
#include <i386/vm_param.h>
#include <i386at/idt.h>
#include <i386at/cram.h>
#include <i386at/acpi_parse_apic.h>
#include <kern/printf.h>
#include <mach/machine.h>

#include <kern/smp.h>

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

static void smp_send_ipi(unsigned apic_id, unsigned vector)
{
    unsigned long flags;

    cpu_intr_save(&flags);

    apic_send_ipi(NO_SHORTHAND, FIXED, PHYSICAL, ASSERT, EDGE, vector, apic_id);

    do {
        cpu_pause();
    } while(lapic->icr_low.delivery_status == SEND_PENDING);

    apic_send_ipi(NO_SHORTHAND, FIXED, PHYSICAL, DE_ASSERT, EDGE, vector, apic_id);

    do {
        cpu_pause();
    } while(lapic->icr_low.delivery_status == SEND_PENDING);

    cpu_intr_restore(flags);
}

void smp_remote_ast(unsigned apic_id)
{
    smp_send_ipi(apic_id, CALL_AST_CHECK);
}

void smp_pmap_update(unsigned apic_id)
{
    smp_send_ipi(apic_id, CALL_PMAP_UPDATE);
}

static void
wait_for_ipi(void)
{
    /* This could have a timeout, but if the IPI
     * is never delivered, its a disaster anyway */
    while (lapic->icr_low.delivery_status == SEND_PENDING) {
	cpu_pause();
    }
}

static int
smp_send_ipi_init(int apic_id)
{
    int err;

    lapic->error_status.r = 0;

    /* Assert INIT IPI:
     *
     * This is EDGE triggered to match the deassert
     */
    apic_send_ipi(NO_SHORTHAND, INIT, PHYSICAL, ASSERT, EDGE, 0, apic_id);

    /* Wait for delivery */
    wait_for_ipi();
    hpet_mdelay(10);

    /* Deassert INIT IPI:
     *
     * NB: This must be an EDGE triggered deassert signal.
     * A LEVEL triggered deassert is only supported on very old hardware
     * that does not support STARTUP IPIs at all, and instead jump
     * via a warm reset vector.
     */
    apic_send_ipi(NO_SHORTHAND, INIT, PHYSICAL, DE_ASSERT, EDGE, 0, apic_id);

    /* Wait for delivery */
    wait_for_ipi();

    err = lapic->error_status.r;
    if (err) {
        printf("ESR error upon INIT 0x%x\n", err);
    }
    return 0;
}

static int
smp_send_ipi_startup(int apic_id, int vector)
{
    int err;

    lapic->error_status.r = 0;

    /* StartUp IPI:
     *
     * Have not seen any documentation for trigger mode for this IPI
     * but it seems to work with EDGE.  (AMD BKDG FAM16h document specifies dont care)
     */
    apic_send_ipi(NO_SHORTHAND, STARTUP, PHYSICAL, ASSERT, EDGE, vector, apic_id);

    /* Wait for delivery */
    wait_for_ipi();

    err = lapic->error_status.r;
    if (err) {
        printf("ESR error upon STARTUP 0x%x\n", err);
    }
    return 0;
}

/* See Intel IA32/64 Software Developer's Manual 3A Section 8.4.4.1 */
int smp_startup_cpu(unsigned apic_id, phys_addr_t start_eip)
{
#if 0
    /* This block goes with a legacy method of INIT that only works with
     * old hardware that does not support SIPIs.
     * Must use INIT DEASSERT LEVEL triggered IPI to use this block.
     * (At least one AMD FCH does not support this IPI mode,
     * See AMD BKDG FAM16h document # 48751 page 461).
     */

    /* Tell CMOS to warm reset through through 40:67 */
    outb(CMOS_ADDR, CMOS_SHUTDOWN);
    outb(CMOS_DATA, CM_JMP_467);

    /* Set warm reset vector to point to AP startup code */
    uint16_t dword[2];
    dword[0] = 0;
    dword[1] = start_eip >> 4;
    memcpy((uint8_t *)phystokv(0x467), dword, 4);
#endif

    /* Local cache flush */
    asm("wbinvd":::"memory");

    printf("Sending IPIs to APIC ID %u...\n", apic_id);

    smp_send_ipi_init(apic_id);
    hpet_mdelay(10);
    smp_send_ipi_startup(apic_id, start_eip >> STARTUP_VECTOR_SHIFT);
    hpet_udelay(200);
    smp_send_ipi_startup(apic_id, start_eip >> STARTUP_VECTOR_SHIFT);
    hpet_udelay(200);

    printf("done\n");
    return 0;
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
