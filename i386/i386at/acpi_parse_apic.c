/*Copyright 2018 Juan Bosco Garcia
 *Copyright 2018 2019 2020 Almudena Garcia Jurado-Centurion
 *This file is part of Min_SMP.
 *Min_SMP is free software: you can redistribute it and/or modify
 *it under the terms of the GNU General Public License as published by
 *the Free Software Foundation, either version 2 of the License, or
 *(at your option) any later version.
 *Min_SMP is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *You should have received a copy of the GNU General Public License
 *along with Min_SMP.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <i386at/acpi_parse_apic.h>
#include <string.h> //memcmp, memcpy...

#include <i386/apic.h> //lapic, ioapic...
#include <kern/printf.h> //printf
#include <include/stdint.h> //uint16_t, uint32_t...
#include <mach/machine.h> //machine_slot
#include <i386/vm_param.h> //phystokv
#include <kern/debug.h>
#include <vm/vm_kern.h>

#define BAD_CHECKSUM -1
#define NO_RSDP -2
#define NO_RSDT -2
#define BAD_SIGNATURE -3
#define NO_APIC -4

struct acpi_apic *apic_madt = NULL;

static struct acpi_rsdp* acpi_get_rsdp(void);
static int acpi_check_rsdt(struct acpi_rsdt *);
static struct acpi_rsdt* acpi_get_rsdt(struct acpi_rsdp *rsdp, int* acpi_rsdt_n);
static struct acpi_apic* acpi_get_apic(struct acpi_rsdt *rsdt, int acpi_rsdt_n);
static int acpi_apic_setup(struct acpi_apic *apic);
static int acpi_apic_add_ioapic(struct acpi_apic_ioapic *ioapic_entry);
static int acpi_apic_add_lapic(struct acpi_apic_lapic *lapic_entry);
static int acpi_apic_parse_table(struct acpi_apic *apic);


/* acpi_apic_init: find the MADT/APIC table in ACPI tables
 *  and parses It to find Local APIC and IOAPIC structures
 *  Each Local APIC stores the info and control structores for a cpu
 *
 * Returns 0 if success, -1 if error
 */

int
acpi_apic_init(void)
{
    struct acpi_rsdp *rsdp = 0;
    struct acpi_rsdt *rsdt = 0;
    int acpi_rsdt_n;
    int ret_acpi_setup;


    /* Try to get rsdp pointer */
    rsdp = acpi_get_rsdp();
    if(rsdp == NULL)
        {
            return NO_RSDP;
        }

    printf("rsdp address %x\n", rsdp);

    /* Try to get rsdt pointer */
    rsdt = acpi_get_rsdt(rsdp, &acpi_rsdt_n);
    if(rsdt == NULL)
        {
            return NO_RSDT;
        }

    printf("rsdt address %x\n", rsdt);

    apic_madt = acpi_get_apic(rsdt, acpi_rsdt_n);
    if(apic_madt == NULL)
        {
            return NO_APIC;
        }

    printf("apic address %x\n", apic_madt);

    acpi_print_info(rsdp, rsdt, acpi_rsdt_n);

    ret_acpi_setup = acpi_apic_setup(apic_madt);

    if(ret_acpi_setup != 0)
        {
            return ret_acpi_setup;
        }

    apic_print_info();

    printf("apic setup finished\n");

    return 0;
}

/* acpi_print_info: shows by screen the ACPI's rsdp and rsdt virtual address
 *    and the number of entries stored in RSDT table
 *
 * Receives as input the references of RSDP and RSDT tables,
 *    and the number of entries stored in RSDT
 */

void
acpi_print_info(struct acpi_rsdp *rsdp, struct acpi_rsdt *rsdt, int acpi_rsdt_n)
{

    printf("ACPI:\n");
    printf(" rsdp = %x; rsdp->rsdt_addr = %x\n", rsdp, rsdp->rsdt_addr);
    printf(" rsdt = %x; rsdt->length = %x (n = %x)\n", rsdt, rsdt->header.length,
           acpi_rsdt_n);
}

/* acpi_checksum: calculates the checksum of an ACPI table
 * Receives as input the virtual address of the table
 *
 * Returns 0 if success, other value if error
 */
static int
acpi_checksum(void *addr, uint32_t length)
{
    char *bytes = addr;
    int checksum = 0;
    unsigned int i;

    /* Sum all bytes of addr */
    for(i = 0; i < length; i++)
        {
            checksum += bytes[i];
        }

    return checksum & 0xff;
}

/* acpi_check_rsdp:
 * check if the RDST "candidate" table is the real RSDT table
 *
 * Compare the table signature with the ACPI signature for this table
 *   and check is the checksum is correct
 *
 * Receives as input the reference of RSDT table
 *
 * Returns 0 if correct
 */

static int
acpi_check_rsdp(struct acpi_rsdp *rsdp)
{
    uint32_t checksum;
    int is_rsdp;
    int ret_value = 0;

    /* Check the integrity of RSDP */
    if(rsdp == NULL)
        {
            ret_value = NO_RSDP;
        }
    else
        {
            /* Check is rsdp signature is equals to ACPI RSDP signature */
            is_rsdp = memcmp(rsdp->signature, ACPI_RSDP_SIG, sizeof(rsdp->signature));

            if(is_rsdp == 0)
                {
                    /* If yes, calculates rdsp checksum and check It */
                    checksum = acpi_checksum(rsdp, sizeof(struct acpi_rsdp));

                    if(checksum != 0)
                        {
                            ret_value = BAD_CHECKSUM;
                        }
                }
            else
                {
                    ret_value = BAD_SIGNATURE;
                }
        }

    return ret_value;
}

/* acpi_search_rsdp: search the rsdp table in a memory range
 *
 * Receives as input the initial virtual address, and the lenght
 *   of memory range
 *
 * Returns the reference to rsdp structure if success, NULL if failure
 */

static struct acpi_rsdp*
acpi_search_rsdp(void *addr, uint32_t length)
{
    struct acpi_rsdp *rsdp = NULL;

    void *end;
    /* check alignment */
    if((uint32_t)addr & (ACPI_RSDP_ALIGN-1))
        return NULL;

    /* Search RDSP in memory space between addr and addr+lenght */
    for(end = addr+length; addr < end; addr += ACPI_RSDP_ALIGN)
        {

            //Check if the current memory block store the RDSP
            if(acpi_check_rsdp(addr) == 0)
                {
                    /* If yes, store RSDP address */
                    rsdp = (struct acpi_rsdp*) addr;
                    break;
                }
        }

    return rsdp;
}

/* acpi_get_rsdp: tries to find the RSDP table,
 *    searching It in many memory ranges, following ACPI docs
 *
 *  Returns the reference to RDSP structure if success, NULL if failure
 */

struct acpi_rsdp*
acpi_get_rsdp(void)
{
    struct acpi_rsdp *rsdp = NULL;
    uint16_t *start = 0x0;
    uint32_t base = 0x0;

    /* EDBA start address */
    start = (uint16_t*) phystokv(0x040e);
    base = *start;

    if(base != 0)   /* Memory check */
        {

            base <<= 4; //base = base * 16

            /* Search RSDP in first 1024 bytes from EDBA */
            rsdp = acpi_search_rsdp((void*)base,1024);
        }

    if(rsdp == NULL)
            {
                /* If RSDP isn't in EDBA, search in the BIOS read-only memory space between 0E0000h and 0FFFFFh */
                rsdp = acpi_search_rsdp((void*) 0x0e0000, 0x100000 - 0x0e0000);
            }

    return rsdp;
}

/* acpi_check_rsdt: check if the RSDT initial address is correct
 *   checking its checksum
 *
 * Receives as input a reference for the RSDT "candidate" table
 * Returns 0 if success
 */

static int
acpi_check_rsdt(struct acpi_rsdt *rsdt)
{
    uint32_t checksum;
    int ret_value = 0;

    if(rsdt == NULL)
        {
            ret_value = NO_RSDT;
        }
    else
        {
            checksum = acpi_checksum(rsdt, rsdt->header.length);

            if (checksum != 0)
                {
                    ret_value = BAD_CHECKSUM;
                }
        }

    return ret_value;
}

/* acpi_get_rsdt: Get RSDT table reference from RSDP entries
 *
 * Receives as input a reference for RSDP table
 *   and a reference to store the number of entries of RSDT
 *
 * Returns the reference to RSDT table if success, NULL if error
 */

static struct acpi_rsdt*
acpi_get_rsdt(struct acpi_rsdp *rsdp, int* acpi_rsdt_n)
{
    phys_addr_t rsdt_phys;
    struct acpi_rsdt *rsdt = NULL;
    int acpi_check;
    int signature_check;

    if(rsdp != NULL)
        {
            /* Get rsdt address from rsdp */
            rsdt_phys = rsdp->rsdt_addr;
            rsdt = (struct acpi_rsdt*) kmem_map_aligned_table(rsdt_phys, sizeof(struct acpi_rsdt), VM_PROT_READ);

            printf("found rsdt in address %x\n", rsdt);

            /* Check is rsdt signature is equals to ACPI RSDT signature */
            signature_check = memcmp(rsdt->header.signature, ACPI_RSDT_SIG,
                                     sizeof(rsdt->header.signature));

            if(signature_check == 0)
                {
                    /* Check if rsdt is correct */
                    acpi_check = acpi_check_rsdt(rsdt);

                    if(acpi_check == 0)
                        {
                            /* Calculated number of elements stored in rsdt */
                            *acpi_rsdt_n = (rsdt->header.length - sizeof(rsdt->header))
                                           / sizeof(rsdt->entry[0]);

                        }
                }

            if (signature_check != 0 || acpi_check != 0)
                {
                    rsdt = NULL;
                }
        }

    return rsdt;
}

/* acpi_get_apic: get MADT/APIC table from RSDT entries
 *
 * Receives as input the RSDT initial address,
 *   and the number of entries of RSDT table
 *
 * Returns a reference to APIC/MADT table if success, NULL if failure
 */

static struct acpi_apic*
acpi_get_apic(struct acpi_rsdt *rsdt, int acpi_rsdt_n)
{
    struct acpi_apic *apic = NULL;
    struct acpi_dhdr *descr_header;
    int check_signature;

    int i;

    if(rsdt != NULL)
        {
            /* Search APIC entries in rsdt array */
            for(i = 0; i < acpi_rsdt_n; i++)
                {
                    descr_header = (struct acpi_dhdr*) kmem_map_aligned_table(rsdt->entry[i], sizeof(struct acpi_dhdr), VM_PROT_READ);

                    /* Check if the entry contains an APIC */
                    check_signature = memcmp(descr_header->signature, ACPI_APIC_SIG, sizeof(descr_header->signature));

                    if(check_signature == 0)
                        {
                            /* If yes, store the entry in apic */
                            apic = (struct acpi_apic*) kmem_map_aligned_table(rsdt->entry[i], sizeof(struct acpi_apic), VM_PROT_READ | VM_PROT_WRITE);

                            printf("found apic in address %x\n", apic);
                            break;
                        }
                }
        }

    return apic;
}

/* acpi_add_lapic: add a new Local APIC to cpu_to_lapic array
 *    and increase the number of cpus
 *
 *  Receives as input the Local APIC entry in MADT/APIC table
 */


static int
acpi_apic_add_lapic(struct acpi_apic_lapic *lapic_entry)
{
    int ret_value = 0;
    int lapic_id;


    if(lapic_entry == NULL)
        {
            ret_value = -1;
        }
    else
        {
            /* If cpu flag is correct */
            if(lapic_entry->flags & 0x1)
                {
                    /* Enumerate CPU and add It to cpu/apic vector */
                    lapic_id = lapic_entry->apic_id;

                    apic_add_cpu(lapic_id);

                    printf("new cpu found with apic id %x\n", lapic_entry->apic_id);;
                }
        }

    return ret_value;
}

/* apic_add_ioapic: add a new IOAPIC to IOAPICS array
 *   and increase the number of IOAPIC
 *
 * Receives as input the IOAPIC entry in MADT/APIC table
 */

static int
acpi_apic_add_ioapic(struct acpi_apic_ioapic *ioapic_entry)
{
    int ret_value = 0;
    struct ioapic_data io_apic;

    if(ioapic_entry == NULL)
        {
            ret_value = -1;
        }
    else
        {
            /* Insert ioapic in ioapics array */
            io_apic.apic_id = ioapic_entry->apic_id;
            io_apic.addr = ioapic_entry->addr;
            io_apic.base = ioapic_entry->base;

            apic_add_ioapic(io_apic);

            printf("new ioapic found with apic id %x\n", io_apic.apic_id);
        }

    return ret_value;
}


/* apic_add_ioapic: add a new IOAPIC to IOAPICS array
 *   and increase the number of IOAPIC
 *
 * Receives as input the IOAPIC entry in MADT/APIC table
 */

static int
acpi_apic_add_irq_override(struct acpi_apic_irq_override* irq_override)
{
    int ret_value = 0;
    struct irq_override_data irq_over;


    if(irq_override == NULL)
        {
            ret_value = -1;
        }
    else
        {
            /* Insert ioapic in ioapics array */
            irq_over.bus = irq_override->bus;
            irq_over.irq = irq_override->irq;
            irq_over.gsi = irq_override->gsi;
            irq_over.flags = irq_override->flags;

            apic_add_irq_override(irq_over);
        }

    return ret_value;
}


/* apic_parse_table: parse the MADT/APIC table
 *   Read the APIC/MADT table entry to entry,
 *      registering the Local APIC or IOAPIC entries
 */

static int
apic_parse_table(struct acpi_apic *apic)
{
    int ret_value = 0;
    struct acpi_apic_dhdr *apic_entry = NULL;
    uint32_t end = 0;

    if(apic != NULL)
        {
            apic_entry = apic->entry;
            end = (uint32_t) apic + apic->header.length;

            /* Search in APIC entry */
            while((uint32_t)apic_entry < end)
                {
                    struct acpi_apic_lapic *lapic_entry;
                    struct acpi_apic_ioapic *ioapic_entry;
                    struct acpi_apic_irq_override *irq_override_entry;

                    /* Check entry type */
                    switch(apic_entry->type)
                        {

                        /* If APIC entry is a CPU lapic */
                        case ACPI_APIC_ENTRY_LAPIC:

                            /* Store lapic */
                            lapic_entry = (struct acpi_apic_lapic*) apic_entry;

                            acpi_apic_add_lapic(lapic_entry);

                            break;

                        /* If APIC entry is an IOAPIC */
                        case ACPI_APIC_ENTRY_IOAPIC:

                            /* Store ioapic */
                            ioapic_entry = (struct acpi_apic_ioapic*) apic_entry;

                            acpi_apic_add_ioapic(ioapic_entry);

                            break;

                        /* If APIC entry is a IRQ */
                        case ACPI_APIC_IRQ_OVERRIDE:
                             irq_override_entry = (struct acpi_apic_irq_override*) apic_entry;

                             acpi_apic_add_irq_override(irq_override_entry);
                             break;

                        }

                    /* Get next APIC entry */
                    apic_entry = (struct acpi_apic_dhdr*)((uint32_t) apic_entry
                                                          + apic_entry->length);
                }
        }
    else /* apic == NULL */
        {
            ret_value = -1;
        }

    return ret_value;
}


/* acpi_apic_setup: parses the APIC/MADT table.
 *    to find the Local APIC and IOAPIC structures
 *    and the common address for Local APIC
 *
 *  Receives as input a reference for APIC/MADT table
 *  Returns 0 if success
 *
 * Fills the cpu_to_lapic and ioapics array, indexed by Kernel ID
 *   with a relationship between Kernel ID and APIC ID,
 *   and map the Local APIC common address, to fill the lapic reference
 */

static int
acpi_apic_setup(struct acpi_apic *apic)
{
    int apic_checksum;
    int ret_value = 0;
    ApicLocalUnit* lapic;
    int ncpus, nioapics;
    int init_success = 0;


    if(apic != NULL)
        {

            /* Check the checksum of the APIC */
            apic_checksum = acpi_checksum(apic, apic->header.length);

            if(apic_checksum != 0)
                {
                    ret_value = BAD_CHECKSUM;
                }
            else
                {
                    init_success = apic_data_init();
                    if(init_success == 0)
                        {

                            printf("lapic found in address %x\n", apic->lapic_addr);

                            /* map common lapic address */
                            lapic = kmem_map_aligned_table(apic->lapic_addr, sizeof(ApicLocalUnit), VM_PROT_READ);

                            if(lapic != NULL){
                                printf("lapic mapped in address %x\n", lapic);
                                apic_lapic_init(lapic);
                            }

                            apic_parse_table(apic);

                            ncpus = apic_get_numcpus();
                            nioapics = apic_get_num_ioapics();

                            if(ncpus == 0 || nioapics == 0)
                                {
                                    ret_value = -1;
                                }
                            else
                                {
                                    apic_refit_cpulist();
                                    printf("%d cpus found. %d ioapics found\n", ncpus, nioapics);
                                }
                        }
                        else{
                            ret_value = -1;
                        }
                }
        }
    else /* apic == NULL */
        {
            ret_value = NO_APIC;
        }

    return ret_value;
}

