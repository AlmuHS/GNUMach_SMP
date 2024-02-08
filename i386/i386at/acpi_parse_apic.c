/* acpi_parse_apic.h - ACPI-MADT table parser. Source file
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

#include <string.h>         /* memcmp, memcpy... */

#include <stdint.h> /* uint16_t, uint32_t... */

#include <mach/machine.h>   /* machine_slot */

#include <kern/printf.h>    /* printf */
#include <kern/debug.h>
#include <i386/vm_param.h>  /* phystokv */
#include <i386/apic.h>      /* lapic, ioapic... */
#include <i386at/acpi_parse_apic.h>
#include <vm/vm_kern.h>

static struct acpi_apic *apic_madt = NULL;
unsigned lapic_addr;
uint32_t *hpet_addr;

/*
 * acpi_print_info: shows by screen the ACPI's rsdp and rsdt virtual address
 * and the number of entries stored in RSDT table.
 *
 * Receives as input the references of RSDP and RSDT tables,
 * and the number of entries stored in RSDT.
 */
void
acpi_print_info(phys_addr_t rsdp, void *rsdt, int acpi_rsdt_n)
{

    printf("ACPI:\n");
    printf(" rsdp = 0x%lx\n", rsdp);
    printf(" rsdt/xsdt = 0x%p (n = %d)\n", rsdt, acpi_rsdt_n);
}

/*
 * acpi_checksum: calculates the checksum of an ACPI table.
 * Receives as input the virtual address of the table.
 *
 * Returns 0 if success, other value if error.
 */
static uint8_t
acpi_checksum(void *addr, uint32_t length)
{
    uint8_t *bytes = addr;
    uint8_t checksum = 0;
    unsigned int i;

    /* Sum all bytes of addr */
    for (i = 0; i < length; i++)
        checksum += bytes[i];

    return checksum;
}

/*
 * acpi_check_signature: check if a signature match with the signature of its table.
 *
 * Receive as parameter both signatures: table signature, the signature which needs to check,
 * and real signature, the genuine signature of the table.
 *
 * Return 0 if success, other if error.
 */

static int
acpi_check_signature(const uint8_t table_signature[], const char *real_signature, uint8_t length)
{
    return memcmp(table_signature, real_signature, length);
}


/*
 * acpi_check_rsdp:
 * check if the RDSP "candidate" table is the real RSDP table.
 *
 * Compare the table signature with the ACPI signature for this table
 * and check is the checksum is correct.
 *
 * Receives as input the reference of RSDT table.
 *
 * Preconditions: RSDP pointer must not be NULL.
 *
 * Returns 1 if ACPI 1.0 and sets sdt_base
 * Returns 2 if ACPI >= 2.0 and sets sdt_base
 */
static int8_t
acpi_check_rsdp(struct acpi_rsdp2 *rsdp, phys_addr_t *sdt_base)
{
    int is_rsdp;
    uint8_t cksum;

    /* Check if rsdp signature match with the ACPI RSDP signature. */
    is_rsdp = acpi_check_signature(rsdp->v1.signature, ACPI_RSDP_SIG, 8*sizeof(uint8_t));

    if (is_rsdp != ACPI_SUCCESS)
        return ACPI_BAD_SIGNATURE;

    if (rsdp->v1.revision == 0) {
        // ACPI 1.0
        *sdt_base = rsdp->v1.rsdt_addr;
        printf("ACPI v1.0\n");
        cksum = acpi_checksum((void *)(&rsdp->v1), sizeof(struct acpi_rsdp));

        if (cksum != 0)
            return ACPI_BAD_CHECKSUM;

        return 1;

    } else if (rsdp->v1.revision == 2) {
        // ACPI >= 2.0
        *sdt_base = rsdp->xsdt_addr;
        printf("ACPI >= v2.0\n");
        cksum = acpi_checksum((void *)rsdp, sizeof(struct acpi_rsdp2));

        if (cksum != 0)
            return ACPI_BAD_CHECKSUM;

        return 2;
    }

    return ACPI_NO_RSDP;
}

/*
 * acpi_check_rsdp_align: check if the RSDP address is aligned.
 * Preconditions: The address must not be NULL
 *
 * Returns ACPI_SUCCESS (0) if success, ACPI_BAD_ALIGN if error
 */

static int8_t
acpi_check_rsdp_align(void *addr)
{
    /* check alignment. */
    if ((uintptr_t)addr & (ACPI_RSDP_ALIGN-1))
        return ACPI_BAD_ALIGN;

    return ACPI_SUCCESS;
}

/*
 * acpi_search_rsdp: search the rsdp table in a memory range.
 *
 * Receives as input the initial virtual address, and the lenght
 * of memory range.
 *
 * Preconditions: The start address (addr) must be aligned.
 *
 * Returns the physical address of rsdp structure if success, 0 if failure.
 */
static phys_addr_t
acpi_search_rsdp(void *addr, uint32_t length, int *is_64bit)
{
    void *end;
    int version = 0;
    phys_addr_t sdt_base = 0;

    /* Search RDSP in memory space between addr and addr+lenght. */
    for (end = addr+length; addr < end; addr += ACPI_RSDP_ALIGN) {

        /* Check if the current memory block stores the RDSP. */
        if ((addr != NULL) && ((version = acpi_check_rsdp(addr, &sdt_base)) > 0)) {
            /* If yes, return RSDT/XSDT address */
            *is_64bit = (version == 2);
            return sdt_base;
        }
    }

    return 0;
}

/*
 * acpi_get_rsdp: tries to find the RSDP table,
 * searching It in many memory ranges, as It's written in ACPI Specification.
 *
 * Returns the reference to RDSP structure if success, 0 if failure.
 */
static phys_addr_t
acpi_get_rsdp(int *is_64bit)
{
    uint16_t *start = 0;
    phys_addr_t base = 0;
    phys_addr_t rsdp = 0;

    /* EDBA start address. */
    start = (uint16_t*) phystokv(0x040e);
    base = phystokv((*start) << 4); /* address = paragraph number * 16 */

    /* check alignment. */
    if (acpi_check_rsdp_align((void *)base) == ACPI_BAD_ALIGN)
        return 0;
    rsdp = acpi_search_rsdp((void *)base, 1024, is_64bit);

    if (rsdp == 0) {
        /* If RSDP isn't in EDBA, search in the BIOS read-only memory space between 0E0000h and 0FFFFFh */
        rsdp = acpi_search_rsdp((void *)phystokv(0xe0000), 0x100000 - 0x0e0000, is_64bit);
    }

    return rsdp;
}

/*
 * acpi_get_rsdt: Get RSDT table reference from RSDP entries.
 *
 * Receives as input a reference for RSDP table
 * and a reference to store the number of entries of RSDT.
 *
 * Returns the reference to RSDT table if success, NULL if error.
 */
static struct acpi_rsdt*
acpi_get_rsdt(phys_addr_t rsdp_phys, int* acpi_rsdt_n)
{
    struct acpi_rsdt *rsdt = NULL;
    int signature_check;

    rsdt = (struct acpi_rsdt*) kmem_map_aligned_table(rsdp_phys, sizeof(struct acpi_rsdt), VM_PROT_READ);

    /* Check if the RSDT mapping is fine. */
    if (rsdt == NULL)
        return NULL;

    /* Check is rsdt signature is equals to ACPI RSDT signature. */
    signature_check = acpi_check_signature(rsdt->header.signature, ACPI_RSDT_SIG,
                                           4*sizeof(uint8_t));

    if (signature_check != ACPI_SUCCESS)
        return NULL;

    /* Calculated number of elements stored in rsdt. */
    *acpi_rsdt_n = (rsdt->header.length - sizeof(rsdt->header))
                   / sizeof(rsdt->entry[0]);

    return rsdt;
}

/*
 * acpi_get_xsdt: Get XSDT table reference from RSDPv2 entries.
 *
 * Receives as input a reference for RSDPv2 table
 * and a reference to store the number of entries of XSDT.
 *
 * Returns the reference to XSDT table if success, NULL if error.
 */
static struct acpi_xsdt*
acpi_get_xsdt(phys_addr_t rsdp_phys, int* acpi_xsdt_n)
{
    struct acpi_xsdt *xsdt = NULL;
    int signature_check;

    xsdt = (struct acpi_xsdt*) kmem_map_aligned_table(rsdp_phys, sizeof(struct acpi_xsdt), VM_PROT_READ);

    /* Check if the RSDT mapping is fine. */
    if (xsdt == NULL)
        return NULL;

    /* Check is rsdt signature is equals to ACPI RSDT signature. */
    signature_check = acpi_check_signature(xsdt->header.signature, ACPI_XSDT_SIG,
                                           4*sizeof(uint8_t));

    if (signature_check != ACPI_SUCCESS)
        return NULL;

    /* Calculated number of elements stored in rsdt. */
    *acpi_xsdt_n = (xsdt->header.length - sizeof(xsdt->header))
                   / sizeof(xsdt->entry[0]);

    return xsdt;
}

/*
 * acpi_get_apic: get MADT/APIC table from RSDT entries.
 *
 * Receives as input the RSDT initial address,
 * and the number of entries of RSDT table.
 *
 * Returns a reference to APIC/MADT table if success, NULL if failure.
 * Also sets hpet_addr to base address of HPET.
 */
static struct acpi_apic*
acpi_get_apic(struct acpi_rsdt *rsdt, int acpi_rsdt_n)
{
    struct acpi_dhdr *descr_header;
    struct acpi_apic *madt = NULL;
    int check_signature;
    uint64_t map_addr;

    /* Search APIC entries in rsdt table. */
    for (int i = 0; i < acpi_rsdt_n; i++) {
        descr_header = (struct acpi_dhdr*) kmem_map_aligned_table(rsdt->entry[i], sizeof(struct acpi_dhdr),
                                                                  VM_PROT_READ);

        /* Check if the entry is a MADT */
        check_signature = acpi_check_signature(descr_header->signature, ACPI_APIC_SIG, 4*sizeof(uint8_t));
        if (check_signature == ACPI_SUCCESS)
            madt = (struct acpi_apic*) descr_header;

        /* Check if the entry is a HPET */
        check_signature = acpi_check_signature(descr_header->signature, ACPI_HPET_SIG, 4*sizeof(uint8_t));
        if (check_signature == ACPI_SUCCESS) {
            map_addr = ((struct acpi_hpet *)descr_header)->address.addr64;
            assert (map_addr != 0);
            hpet_addr = (uint32_t *)kmem_map_aligned_table(map_addr, 1024, VM_PROT_READ | VM_PROT_WRITE);
            printf("HPET at physical address 0x%llx\n", map_addr);
        }
    }

    return madt;
}

/*
 * acpi_get_apic2: get MADT/APIC table from XSDT entries.
 *
 * Receives as input the XSDT initial address,
 * and the number of entries of XSDT table.
 *
 * Returns a reference to APIC/MADT table if success, NULL if failure.
 * Also sets hpet_addr to base address of HPET.
 */
static struct acpi_apic*
acpi_get_apic2(struct acpi_xsdt *xsdt, int acpi_xsdt_n)
{
    struct acpi_dhdr *descr_header;
    struct acpi_apic *madt = NULL;
    int check_signature;
    uint64_t map_addr;

    /* Search APIC entries in rsdt table. */
    for (int i = 0; i < acpi_xsdt_n; i++) {
        descr_header = (struct acpi_dhdr*) kmem_map_aligned_table(xsdt->entry[i], sizeof(struct acpi_dhdr),
                                                                  VM_PROT_READ);

        /* Check if the entry is an APIC. */
        check_signature = acpi_check_signature(descr_header->signature, ACPI_APIC_SIG, 4*sizeof(uint8_t));
        if (check_signature == ACPI_SUCCESS)
            madt = (struct acpi_apic *)descr_header;

        /* Check if the entry is a HPET. */
        check_signature = acpi_check_signature(descr_header->signature, ACPI_HPET_SIG, 4*sizeof(uint8_t));
        if (check_signature == ACPI_SUCCESS) {
            map_addr = ((struct acpi_hpet *)descr_header)->address.addr64;
            assert (map_addr != 0);
            hpet_addr = (uint32_t *)kmem_map_aligned_table(map_addr, 1024, VM_PROT_READ | VM_PROT_WRITE);
            printf("HPET at physical address 0x%llx\n", map_addr);
        }
    }

    return madt;
}

/*
 * acpi_add_lapic: add a new Local APIC to cpu_to_lapic array
 * and increase the number of cpus.
 *
 * Receives as input the Local APIC entry in MADT/APIC table.
 */
static void
acpi_apic_add_lapic(struct acpi_apic_lapic *lapic_entry)
{
    /* If cpu flag is correct */
    if (lapic_entry->flags & 0x1) {
        /* Add cpu to processors' list. */
        apic_add_cpu(lapic_entry->apic_id);
    }

}

/*
 * apic_add_ioapic: add a new IOAPIC to IOAPICS array
 * and increase the number of IOAPIC.
 *
 * Receives as input the IOAPIC entry in MADT/APIC table.
 */

static void
acpi_apic_add_ioapic(struct acpi_apic_ioapic *ioapic_entry)
{
    IoApicData io_apic;

    /* Fill IOAPIC structure with its main fields */
    io_apic.apic_id = ioapic_entry->apic_id;
    io_apic.addr = ioapic_entry->addr;
    io_apic.gsi_base = ioapic_entry->gsi_base;
    io_apic.ioapic = (ApicIoUnit *)kmem_map_aligned_table(ioapic_entry->addr,
                                                          sizeof(ApicIoUnit),
                                                          VM_PROT_READ | VM_PROT_WRITE);
    io_apic.ioapic->select.r = APIC_IO_VERSION;
    io_apic.ngsis = ((io_apic.ioapic->window.r >> APIC_IO_ENTRIES_SHIFT) & 0xff) + 1;

    /* Insert IOAPIC in the list. */
    apic_add_ioapic(io_apic);
}


/*
 * apic_add_ioapic: add a new IOAPIC to IOAPICS list
 * and increase the number of IOAPIC.
 *
 * Receives as input the IOAPIC entry in MADT/APIC table.
 */

static void
acpi_apic_add_irq_override(struct acpi_apic_irq_override* irq_override)
{
    IrqOverrideData irq_over;

    /* Fills IRQ override structure with its fields */
    irq_over.bus = irq_override->bus;
    irq_over.irq = irq_override->irq;
    irq_over.gsi = irq_override->gsi;
    irq_over.flags = irq_override->flags;

    /* Insert IRQ override in the list */
    apic_add_irq_override(irq_over);
}


/*
 * apic_parse_table: parse the MADT/APIC table.
 *
 * Read the APIC/MADT table entry to entry,
 * registering the APIC structures (Local APIC, IOAPIC or IRQ override) entries in their lists.
 */

static int
acpi_apic_parse_table(struct acpi_apic *apic)
{
    struct acpi_apic_dhdr *apic_entry = NULL;
    vm_offset_t end = 0;
    uint8_t numcpus = 1;

    /* Get the address of first APIC entry */
    apic_entry = (struct acpi_apic_dhdr*) apic->entry;

    /* Get the end address of APIC table */
    end = (vm_offset_t) apic + apic->header.length;

    printf("APIC entry=0x%p end=0x%x\n", apic_entry, end);

    /* Initialize number of cpus */
    numcpus = apic_get_numcpus();

    /* Search in APIC entry. */
    while ((vm_offset_t)apic_entry < end) {
        struct acpi_apic_lapic *lapic_entry;
        struct acpi_apic_ioapic *ioapic_entry;
        struct acpi_apic_irq_override *irq_override_entry;

        printf("APIC entry=0x%p end=0x%x\n", apic_entry, end);
        /* Check entry type. */
        switch(apic_entry->type) {

        /* If APIC entry is a CPU's Local APIC. */
        case ACPI_APIC_ENTRY_LAPIC:
            if(numcpus < NCPUS) {
                /* Store Local APIC data. */
                lapic_entry = (struct acpi_apic_lapic*) apic_entry;
                acpi_apic_add_lapic(lapic_entry);
            }
            break;

        /* If APIC entry is an IOAPIC. */
        case ACPI_APIC_ENTRY_IOAPIC:

            /* Store IOAPIC data. */
            ioapic_entry = (struct acpi_apic_ioapic*) apic_entry;
            acpi_apic_add_ioapic(ioapic_entry);

            break;

        /* If APIC entry is a IRQ Override. */
        case ACPI_APIC_ENTRY_IRQ_OVERRIDE:

            /* Store IRQ Override data. */
            irq_override_entry = (struct acpi_apic_irq_override*) apic_entry;
            acpi_apic_add_irq_override(irq_override_entry);
            break;

        /* FIXME: There is another unhandled case */
	default:
	    printf("Unhandled APIC entry type 0x%x\n", apic_entry->type);
	    break;
        }

        /* Get next APIC entry. */
        apic_entry = (struct acpi_apic_dhdr*)((vm_offset_t) apic_entry
                                              + apic_entry->length);

        /* Update number of cpus. */
        numcpus = apic_get_numcpus();
    }

    return ACPI_SUCCESS;
}


/*
 * acpi_apic_setup: parses the APIC/MADT table, to find the Local APIC and IOAPIC structures
 * and the common address for Local APIC.
 *
 * Receives as input a reference for APIC/MADT table.
 * Returns 0 if success.
 *
 * Fills the cpu_to_lapic and ioapics array, indexed by Kernel ID
 * with a relationship between Kernel ID and APIC ID,
 * and map the Local APIC common address, to fill the lapic reference.
 *
 * Precondition: The APIC pointer must not be NULL
 */

static int
acpi_apic_setup(struct acpi_apic *apic)
{
    ApicLocalUnit* lapic_unit;
    uint8_t ncpus, nioapics;

    /* map common lapic address */
    lapic_addr = apic->lapic_addr;
    lapic_unit = kmem_map_aligned_table(apic->lapic_addr, sizeof(ApicLocalUnit),
                                        VM_PROT_READ | VM_PROT_WRITE);

    if (lapic_unit == NULL)
        return ACPI_NO_LAPIC;

    apic_lapic_init(lapic_unit);
    acpi_apic_parse_table(apic);

    ncpus = apic_get_numcpus();
    nioapics = apic_get_num_ioapics();

    if (ncpus == 0 || nioapics == 0 || ncpus > NCPUS)
        return ACPI_APIC_FAILURE;

    /* Refit the apic-cpu array. */
    if(ncpus < NCPUS) {
        int refit = apic_refit_cpulist();
        if (refit != 0)
            return ACPI_FIT_FAILURE;
    }

    apic_generate_cpu_id_lut();

    return ACPI_SUCCESS;
}

/*
 * acpi_apic_init: find the MADT/APIC table in ACPI tables
 * and parses It to find Local APIC and IOAPIC structures.
 * Each Local APIC stores the info and control structores for a cpu.
 * The IOAPIC controls the communication of the processors with the I/O devices.
 *
 * Returns 0 if success, -1 if error.
 */
int
acpi_apic_init(void)
{
    phys_addr_t rsdp = 0;
    struct acpi_rsdt *rsdt = 0;
    struct acpi_xsdt *xsdt = 0;
    int acpi_rsdt_n;
    int ret_acpi_setup;
    int apic_init_success = 0;
    int is_64bit = 0;
    uint8_t checksum;

    /* Try to get the RSDP physical address. */
    rsdp = acpi_get_rsdp(&is_64bit);
    if (rsdp == 0)
        return ACPI_NO_RSDP;

    if (!is_64bit) {
        /* Try to get the RSDT pointer. */
        rsdt = acpi_get_rsdt(rsdp, &acpi_rsdt_n);
        if (rsdt == NULL)
            return ACPI_NO_RSDT;

        checksum = acpi_checksum((void *)rsdt, rsdt->header.length);
        if (checksum != 0)
            return ACPI_BAD_CHECKSUM;

        /* Try to get the APIC table pointer. */
        apic_madt = acpi_get_apic(rsdt, acpi_rsdt_n);
        if (apic_madt == NULL)
            return ACPI_NO_APIC;

        checksum = acpi_checksum((void *)apic_madt, apic_madt->header.length);
        if (checksum != 0)
            return ACPI_BAD_CHECKSUM;

        /* Print the ACPI tables addresses. */
        acpi_print_info(rsdp, rsdt, acpi_rsdt_n);

    } else {
	/* Try to get the XSDT pointer. */
        xsdt = acpi_get_xsdt(rsdp, &acpi_rsdt_n);
        if (xsdt == NULL)
            return ACPI_NO_RSDT;

        checksum = acpi_checksum((void *)xsdt, xsdt->header.length);
        if (checksum != 0)
            return ACPI_BAD_CHECKSUM;

        /* Try to get the APIC table pointer. */
        apic_madt = acpi_get_apic2(xsdt, acpi_rsdt_n);
        if (apic_madt == NULL)
            return ACPI_NO_APIC;

        checksum = acpi_checksum((void *)apic_madt, apic_madt->header.length);
        if (checksum != 0)
            return ACPI_BAD_CHECKSUM;

        /* Print the ACPI tables addresses. */
        acpi_print_info(rsdp, xsdt, acpi_rsdt_n);
    }

    apic_init_success = apic_data_init();
    if (apic_init_success != ACPI_SUCCESS)
        return ACPI_APIC_FAILURE;

    /*
     * Starts the parsing of APIC table, to find the APIC structures.
     * and enumerate them. This function also find the common Local APIC address.
     */
    ret_acpi_setup = acpi_apic_setup(apic_madt);
    if (ret_acpi_setup != ACPI_SUCCESS)
        return ret_acpi_setup;

    /* Prints a table with the list of each cpu and each IOAPIC with its APIC ID. */
    apic_print_info();

    return ACPI_SUCCESS;
}
