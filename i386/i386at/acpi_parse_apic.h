/* acpi_parse_apic.h - ACPI-MADT table parser. Header file
   Copyright (C) 2018 Juan Bosco Garcia
   Copyright (C) 2019 2020 Almudena Garcia Jurado-Centurion
   Written by Juan Bosco Garcia and Almudena Garcia Jurado-Centurion

   This file is part of Min_SMP.

   Min_SMP is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Min_SMP is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#ifndef __ACPI_H__
#define __ACPI_H__

#include <stdint.h>

enum ACPI_RETURN {
    ACPI_BAD_CHECKSUM = -1,
    ACPI_BAD_ALIGN = -2,
    ACPI_NO_RSDP = -3,
    ACPI_NO_RSDT = -4,
    ACPI_BAD_SIGNATURE = -5,
    ACPI_NO_APIC = -6,
    ACPI_NO_LAPIC = -7,
    ACPI_APIC_FAILURE = -8,
    ACPI_FIT_FAILURE = -9,
    ACPI_SUCCESS = 0,
};

#define ACPI_RSDP_ALIGN 16
#define ACPI_RSDP_SIG "RSD PTR "

struct acpi_rsdp {
    uint8_t  	signature[8];
    uint8_t  	checksum;
    uint8_t  	oem_id[6];
    uint8_t  	revision;
    uint32_t 	rsdt_addr;
} __attribute__((__packed__));

struct acpi_rsdp2 {
    struct acpi_rsdp v1;
    uint32_t	length;
    uint64_t	xsdt_addr;
    uint8_t	checksum;
    uint8_t 	reserved[3];
} __attribute__((__packed__));

/*
 * RSDT Entry Header
 *
 * Header which stores the descriptors of tables pointed from RDSP's Entry Field
 * Includes the signature of the table, to identify each table.
 *
 * In MADT, the signature is 'APIC'.
 */
struct acpi_dhdr {
    uint8_t		signature[4];
    uint32_t	length;
    uint8_t  	revision;
    uint8_t  	checksum;
    uint8_t  	oem_id[6];
    uint8_t  	oem_table_id[8];
    uint32_t 	oem_revision;
    uint8_t  	creator_id[4];
    uint32_t 	creator_revision;
} __attribute__((__packed__));


#define ACPI_RSDT_SIG "RSDT"

struct acpi_rsdt {
    struct acpi_dhdr 	header;
    uint32_t 			entry[0];
} __attribute__((__packed__));

#define ACPI_XSDT_SIG "XSDT"

struct acpi_xsdt {
    struct acpi_dhdr 	header;
    uint64_t 			entry[0];
} __attribute__((__packed__));

struct acpi_address {
    uint8_t	is_io;
    uint8_t	reg_width;
    uint8_t	reg_offset;
    uint8_t	reserved;
    uint64_t	addr64;
} __attribute__((__packed__));

/* APIC table signature. */
#define ACPI_APIC_SIG "APIC"

/* Types value for MADT entries: Local APIC, IOAPIC and IRQ Override. */
enum ACPI_APIC_ENTRY_TYPE {
    ACPI_APIC_ENTRY_LAPIC = 0,
    ACPI_APIC_ENTRY_IOAPIC = 1,
    ACPI_APIC_ENTRY_IRQ_OVERRIDE  = 2,
    ACPI_APIC_ENTRY_NONMASK_IRQ = 4
};

/*
 * APIC descriptor header
 * Define the type of the structure (Local APIC, I/O APIC or others).
 * Type: Local APIC (0), I/O APIC (1).
 */
struct acpi_apic_dhdr {
    uint8_t 	type;
    uint8_t 	length;
} __attribute__((__packed__));


/*
 * Multiple APIC Description Table (MADT)
 *
 * Describes the APIC structures which exist in the machine.
 * Includes the common address where Local APIC is mapped in main memory.
 *
 * Entry field stores the descriptors of APIC structures.
 */
struct acpi_apic {
    struct   	acpi_dhdr header;       /* Header, which stores the descriptor for RDST's Entry field. */
    uint32_t 	lapic_addr;             /* Local Interrupt Controller Address. */
    uint32_t 	flags;
    struct acpi_apic_dhdr entry[0];     /* Interrupt Controller Structure */
} __attribute__((__packed__));

/*
 * Processor Local APIC Structure
 *
 * Stores information about APIC ID, flags and ACPI Processor UID
 */

struct acpi_apic_lapic {
    struct      acpi_apic_dhdr header;
    uint8_t     processor_id;           /* ACPI Processor UID */
    uint8_t     apic_id;
    uint32_t    flags;
} __attribute__((__packed__));


/*
 * I/O APIC Structure
 *
 * Stores information about APIC ID, and I/O APIC tables
 */

struct acpi_apic_ioapic {
    struct      acpi_apic_dhdr header;
    uint8_t     apic_id;
    uint8_t     reserved;
    uint32_t    addr;
    uint32_t    gsi_base;
} __attribute__((__packed__));

/*
 * IRQ Override structure
 *
 * Stores information about IRQ override, with busses and IRQ.
 */

struct acpi_apic_irq_override {
    struct acpi_apic_dhdr header;
    uint8_t     bus;
    uint8_t     irq;
    uint32_t    gsi;
    uint16_t    flags;
} __attribute__((__packed__));


#define ACPI_HPET_SIG "HPET"

/*
 * HPET High Precision Event Timer structure
 */
struct acpi_hpet {
    struct acpi_dhdr header;
    uint32_t	id;
    struct acpi_address	address;
    uint8_t	sequence;
    uint16_t	minimum_tick;
    uint8_t	flags;
} __attribute__((__packed__));

int acpi_apic_init(void);
void acpi_print_info(phys_addr_t rsdp, void *rsdt, int acpi_rsdt_n);

extern unsigned lapic_addr;

#endif /* __ACPI_H__ */
