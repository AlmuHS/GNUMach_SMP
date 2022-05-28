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

#include <i386/i386/apic.h>
#include <i386/i386/smp.h>
#include <i386/i386at/acpi_parse_apic.h>
#include <kern/printf.h>
#include <mach/machine.h>

#include <kern/smp.h>

#define AP_BOOT_ADDR (0x7000)

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

void smp_startup_cpu(unsigned apic_id, unsigned vector)
{
    printf("Sending IPI to APIC ID %u\n", apic_id);
     
    /* First INIT IPI */	    
    apic_send_ipi(NO_SHORTHAND, INIT, PHYSICAL, ASSERT, LEVEL, 0 , apic_id);  

    for(int i = 0; i < 10000000; i++);

    /* Second INIT IPI */
    apic_send_ipi(NO_SHORTHAND, INIT, PHYSICAL, ASSERT, LEVEL, 0 , apic_id);

    for(int i = 0; i < 10000000; i++);

    /* First StartUp IPI */
    apic_send_ipi(NO_SHORTHAND, STARTUP, PHYSICAL, ASSERT, LEVEL, vector >> 12, apic_id);

    for(int i = 0; i < 10000000; i++);

    /* Second StartUp IPI */
    //apic_send_ipi(NO_SHORTHAND, STARTUP, PHYSICAL, ASSERT, LEVEL, vector >> 12, apic_id);
}


/*
 * smp_init: initialize the SMP support, starting the cpus searching
 * and enumeration.
 */
int smp_init(void)
{
    int apic_success;

    apic_success = acpi_apic_init();
    if (apic_success == ACPI_SUCCESS) {
        smp_data_init();
    }

    return apic_success;
}
