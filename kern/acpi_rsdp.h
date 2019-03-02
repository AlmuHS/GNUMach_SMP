/*Copyright 2018 Juan Bosco Garcia
 *This file is part of Min_SMP. 
 *Min_SMP is free software: you can redistribute it and/or modify
 *it under the terms of the GNU General Public License as published by
 *the Free Software Foundation, either version 3 of the License, or
 *(at your option) any later version.
 *Min_SMP is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *You should have received a copy of the GNU General Public License
 *along with Min_SMP.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ACPI_H__
#define __ACPI_H__

#include <types.h>


#define ACPI_RSDP_ALIGN 16
#define ACPI_RSDP_SIG "RSD PTR "

struct acpi_rsdp {
    uint8 signature[8];
    uint8 checksum;
    uint8 oem_id[6];
    uint8 revision[1];
    uint32 rsdt_addr;
} __attribute__((__packed__));

struct acpi_dhdr {
    uint8 signature[4];
    uint32 length;
    uint8 revision;
    uint8 checksum;
    uint8 oem_id[6];
    uint8 oem_table_id[8];
    uint32 oem_revision;
    uint8 creator_id[4];
    uint32 creator_revision;
} __attribute__((__packed__));


#define ACPI_RSDT_SIG "RSDT"

struct acpi_rsdt {
    struct acpi_dhdr header;
    uint32 entry[0];
} __attribute__((__packed__));


#define ACPI_APIC_SIG "APIC"

#define ACPI_APIC_ENTRY_LAPIC  0
#define ACPI_APIC_ENTRY_IOAPIC 1

struct acpi_apic_dhdr {
    uint8 type;
    uint8 length;
} __attribute__((__packed__));

struct acpi_apic {
    struct acpi_dhdr header;
    uint32 lapic_addr;
    uint32 flags;
    struct acpi_apic_dhdr entry[0];
} __attribute__((__packed__));


struct acpi_apic_lapic {
    struct acpi_apic_dhdr header;
    uint8 processor_id;
    uint8 apic_id;
    uint32 flags;
} __attribute__((__packed__));


struct acpi_apic_ioapic {
    struct acpi_apic_dhdr header;
    uint8 apic_id;
    uint8 reserved;
    uint32 addr;
    uint32 base;
} __attribute__((__packed__));




int acpi_setup();
void acpi_print_info();


#endif /* __ACPI_H__ */
