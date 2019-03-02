/*Copyright 2018 Juan Bosco Garcia
 *Copyright 2018 2019 Almudena Garcia Jurado-Centurion
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

#include <acpi.h> 
#include <string.h> //memcmp, memcpy...

#include <imps/apic.h> //lapic, ioapic...
#include <kern/printf.h> //printf
#include <stdint.h> //uint16_t, uint32_t...

//#include <mp.h>
//#include <cpu.h>

struct acpi_rsdp *rsdp;
struct acpi_rsdt *rsdt;
int acpi_rsdt_n;
struct acpi_apic *apic;

static int acpi_get_rsdp();

static int acpi_check_rsdt(struct acpi_rsdt *);
static int acpi_get_rsdt();

static int acpi_apic_setup();
extern int ncpu;
extern volatile uint16_t* lapic;
extern struct cpu cpus[];
extern int cpu;

extern int nioapic;
extern struct list ioapics;


int
acpi_setup(){

	//Try to get rsdp pointer
    if(acpi_get_rsdp() || rsdp==0)
        return -1;

	//Try to get rsdt pointer
    if(acpi_get_rsdt() || rsdt==0)
        return -1;
	
	//Search APIC entries in rsdt array
    int i;
    struct acpi_dhdr *descr_header;
    for(i = 0;i < acpi_rsdt_n; i++){
        descr_header = (struct acpi_dhdr*) rsdt->entry[i];

		//Check if the entry contains an APIC
        if(memcmp(descr_header->signature, ACPI_APIC_SIG, 
                    sizeof(descr_header->signature)) == 0){

			//If yes, store the entry in apic
	        apic = (struct acpi_apic*) rsdt->entry[i];

            //continue;
			break;
        }
    }

    if(acpi_apic_setup())
        return -1;

    return 0;
}

void
acpi_print_info(){

    printf("ACPI:\n");
    printf(" rsdp = %x; rsdp->rsdt_addr = %x\n", rsdp, rsdp->rsdt_addr);
    printf(" rsdt = %x; rsdt->length = %x (n = %x)\n", rsdt, rsdt->header.length,
           acpi_rsdt_n);
    int i;
    struct acpi_dhdr *descr_header;
    for(i=0;i<acpi_rsdt_n;i++){
        descr_header = (struct acpi_dhdr*) rsdt->entry[i];
        printf("  %x: %c%c%c%c (%x)\n", i, descr_header->signature[0],
                descr_header->signature[1], descr_header->signature[2],
                descr_header->signature[3], rsdt->entry[i]);
    }

}


static int
acpi_checksum(void *addr, uint32_t length){
    int8 *bytes = addr;
    int checksum = 0;
    unsigned int i;

	//Sum all bytes of addr
    for(i = 0;i < length; i++){
        checksum += bytes[i];
    }

    return checksum & 0xff;
}

static int
acpi_check_rsdp(struct acpi_rsdp *rsdp){

	//Check is rsdp signature is equals to ACPI RSDP signature
    if(memcmp(rsdp->signature, ACPI_RSDP_SIG, sizeof(rsdp->signature)) != 0)
        return -1;

	//If yes, calculates rdsp checksum and check It
    uint32_t checksum;
    checksum = acpi_checksum(rsdp, sizeof(*rsdp));

    if(checksum != 0)
        return -1;

    return 0;
}


static int
acpi_search_rsdp(void *addr, uint32_t length){

    void *end;
    /* check alignment */
    if((uint32_t)addr & (ACPI_RSDP_ALIGN-1))
        return -1;

	//Search RDSP in memory space between addr and addr+lenght
    for(end = addr+length; addr < end; addr += ACPI_RSDP_ALIGN){

		//Check if the current memory block store the RDSP
        if(acpi_check_rsdp(addr) == 0){

			//If yes, store RSDP address
            rsdp = (struct acpi_rsdp*)addr;
            return 0;
        }

    }


    return -1;
}

static int
acpi_get_rsdp(){

    uint32_t base = 0x0;

	//EDBA start address 
    base = *((uint16_t*) 0x040e);

    if(base != 0){	//Memory check

        base <<= 4; //base = base * 16

		//Search RSDP in first 1024 bytes from EDBA
        if(acpi_search_rsdp((void*)base,1024) == 0)
            return 0;
    }

	//If RSDP isn't in EDBA, search in the BIOS read-only memory space between 0E0000h and 0FFFFFh
    if(acpi_search_rsdp((void*)0x0e0000, 0x100000 - 0x0e0000) == 0)
        return 0;

    return -1;
}


static int
acpi_check_rsdt(struct acpi_rsdt *rsdt){

    return acpi_checksum(rsdt, rsdt->header.length);
}

static int
acpi_get_rsdt(){

	//Get rsdt address from rsdp
    rsdt = (struct acpi_rsdt*)rsdp->rsdt_addr;

	//Check is rsdt signature is equals to ACPI RSDT signature
    if(memcmp(rsdt->header.signature, ACPI_RSDT_SIG, 
                sizeof(rsdt->header.signature)) != 0)
        return -1;

	//Check if rsdt is correct
    if(acpi_check_rsdt(rsdt))
        return -1;

	//Calculated number of elements stored in rsdt
    acpi_rsdt_n = (rsdt->header.length - sizeof(rsdt->header)) 
        / sizeof(rsdt->entry[0]);


    return 0;
}


static int
acpi_apic_setup(){

    if(apic == 0)
        return -1;

	//Check the checksum of the APIC
    if(acpi_checksum(apic, apic->header.length))
        return -1;


    ncpu = 0;
    nioapic = 0;
    lapic = (uint16_t*) apic->lapic_addr;
    list_init(&ioapics);
    struct acpi_apic_dhdr *apic_entry = apic->entry;
    uint32_t end = (uint32_t) apic + apic->header.length;

	//Search in APIC entry
    while((uint32_t)apic_entry < end){
        struct acpi_apic_lapic *lapic_entry;
        struct acpi_apic_ioapic *ioapic_entry;

		//Check entry type
        switch(apic_entry->type){

			//If APIC entry is a CPU lapic
            case ACPI_APIC_ENTRY_LAPIC:

				//Store lapic
                lapic_entry = (struct acpi_apic_lapic*) apic_entry;

				//If cpu flag is correct, and the maximum number of CPUs is not reached
                if((lapic_entry->flags & 0x1) && ncpu < NCPU){

					//Enumerate CPU and add It to cpu/apic vector
                    cpus[ncpu].apic_id = lapic_entry->apic_id;

					//Increase number of CPU
                    ncpu++;
                }
                break;

			//If APIC entry is an IOAPIC
            case ACPI_APIC_ENTRY_IOAPIC:

				//Store ioapic
                ioapic_entry = (struct acpi_apic_ioapic*) apic_entry;

				//Initialice ioapic table
                struct ioapic *ioapic_last;

                ioapic_last = malloc(sizeof(struct ioapic));
                list_init_node(&ioapic_last->node);
                ioapic_last->apic_id = ioapic_entry->apic_id;
                ioapic_last->addr = ioapic_entry->addr;
                ioapic_last->base = ioapic_entry->base;

				//Insert ioapic in ioapic's list
                list_insert_tail(&ioapics, &ioapic_last->node);

				//Increase number of ioapic
                nioapic++;
                break;
        }

		//Get next APIC entry
        apic_entry =(struct acpi_apic_dhdr*)((uint32_t) apic_entry 
                + apic_entry->length);
    }


    if(ncpu == 0 || nioapic == 0)
        return -1;


    return 0;
}
