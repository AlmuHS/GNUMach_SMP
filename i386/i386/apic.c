/* apic.c - APIC controller management for Mach.
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
#include <string.h>
#include <vm/vm_kern.h>
#include <kern/printf.h>
#include <kern/kalloc.h>


volatile ApicLocalUnit* lapic = NULL;

ApicInfo apic_data;

/*
 * apic_data_init: initialize the apic_data structures to preliminary values.
 * Reserve memory to the lapic list dynamic vector.
 * Returns 0 if success, -1 if error.
 */
int
apic_data_init(void)
{
    apic_data.cpu_lapic_list = NULL;
    apic_data.ncpus = 0;
    apic_data.nioapics = 0;
    apic_data.nirqoverride = 0;

    /* Reserve the vector memory for the maximum number of processors. */
    apic_data.cpu_lapic_list = (uint16_t*) kalloc(NCPUS*sizeof(uint16_t));

    /* If the memory reserve fails, return -1 to advice about the error. */
    if (apic_data.cpu_lapic_list == NULL)
        return -1;

    return 0;
}

/*
 * apic_lapic_init: initialize lapic pointer to the memory common address.
 * Receives as input a pointer to the virtual memory address, previously mapped in a page.
 */
void
apic_lapic_init(ApicLocalUnit* lapic_ptr)
{
    lapic = lapic_ptr;
}

/*
 * apic_add_cpu: add a new lapic/cpu entry to the cpu_lapic list.
 * Receives as input the lapic's APIC ID.
 */
void
apic_add_cpu(uint16_t apic_id)
{
    apic_data.cpu_lapic_list[apic_data.ncpus] = apic_id;
    apic_data.ncpus++;
}

/*
 * apic_add_ioapic: add a new ioapic entry to the ioapic list.
 * Receives as input an ioapic_data structure, filled with the IOAPIC entry's data.
 */
void
apic_add_ioapic(IoApicData ioapic)
{
    apic_data.ioapic_list[apic_data.nioapics] = ioapic;
    apic_data.nioapics++;
}

/*
 * apic_add_irq_override: add a new IRQ to the irq_override list.
 * Receives as input an irq_override_data structure, filled with the IRQ entry's data.
 */
void
apic_add_irq_override(IrqOverrideData irq_over)
{
    apic_data.irq_override_list[apic_data.nirqoverride] = irq_over;
    apic_data.nirqoverride++;
}

/*
 * apic_get_cpu_apic_id: returns the apic_id of a cpu.
 * Receives as input the kernel ID of a CPU.
 */
uint16_t
apic_get_cpu_apic_id(int kernel_id)
{
    if (kernel_id >= NCPUS)
        return -1;

    return apic_data.cpu_lapic_list[kernel_id];
}

/* apic_get_lapic: returns a reference to the common memory address for Local APIC. */
volatile ApicLocalUnit*
apic_get_lapic(void)
{
    return lapic;
}

/*
 * apic_get_ioapic: returns the IOAPIC identified by its kernel ID.
 * Receives as input the IOAPIC's Kernel ID.
 * Returns a ioapic_data structure with the IOAPIC's data.
 */
struct IoApicData
apic_get_ioapic(int kernel_id)
{
    IoApicData io_apic = {};

    if (kernel_id < MAX_IOAPICS)
        return apic_data.ioapic_list[kernel_id];
    return io_apic;
}

/* apic_get_numcpus: returns the current number of cpus. */
uint8_t
apic_get_numcpus(void)
{
    return apic_data.ncpus;
}

/* apic_get_num_ioapics: returns the current number of ioapics. */
uint8_t
apic_get_num_ioapics(void)
{
    return apic_data.nioapics;
}

/*
 * apic_get_current_cpu: returns the apic_id of current cpu.
 */
uint16_t
apic_get_current_cpu(void)
{
    uint16_t apic_id;

    if(lapic == NULL)
        apic_id = 0;
    else
        apic_id = lapic->apic_id.r;

    return apic_id;
}


/*
 * apic_refit_cpulist: adjust the size of cpu_lapic array to fit the real number of cpus
 * instead the maximum number.
 *
 * Returns 0 if success, -1 if error.
 */
int apic_refit_cpulist(void)
{
    uint16_t* old_list = apic_data.cpu_lapic_list;
    uint16_t* new_list = NULL;

    if (old_list == NULL)
        return -1;

    new_list = (uint16_t*) kalloc(apic_data.ncpus*sizeof(uint16_t));

    if (new_list == NULL)
        return -1;

    for (int i = 0; i < apic_data.ncpus; i++)
        new_list[i] = old_list[i];

    apic_data.cpu_lapic_list = new_list;
    kfree((vm_offset_t) old_list, NCPUS*sizeof(uint16_t));

    return 0;
}

/*
 * apic_print_info: shows the list of Local APIC and IOAPIC.
 * Shows each CPU and IOAPIC, with Its Kernel ID and APIC ID.
 */
void apic_print_info(void)
{
    int i;
    int ncpus, nioapics;

    ncpus = apic_get_numcpus();
    nioapics = apic_get_num_ioapics();

    uint16_t lapic_id;
    uint16_t ioapic_id;

    IoApicData ioapic;

    printf("CPUS:\n");
    for (i = 0; i < ncpus; i++) {
        lapic_id = apic_get_cpu_apic_id(i);
        printf(" CPU %d - APIC ID %x\n", i, lapic_id);
    }

    printf("IOAPICS:\n");
    for (i = 0; i < nioapics; i++) {
        ioapic = apic_get_ioapic(i);
        ioapic_id = ioapic.apic_id;
        printf(" IOAPIC %d - APIC ID %x\n", i, ioapic_id);
    }
}
