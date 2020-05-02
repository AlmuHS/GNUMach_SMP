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

#include <imps/apic.h> //lapic, ioapic...
#include <kern/printf.h> //printf
#include <include/stdint.h> //uint16_t, uint32_t...
#include <mach/machine.h> //machine_slot
#include <i386/vm_param.h> //phystokv
#include <kern/debug.h>
#include <intel/pmap.h>
#include <vm/vm_kern.h>


volatile ApicLocalUnit* lapic = NULL;
struct acpi_apic *apic_madt = NULL;
int ncpu = 1;
int nioapic = 0;


static struct acpi_rsdp* acpi_get_rsdp();
static int acpi_check_rsdt(struct acpi_rsdt *);
static struct acpi_rsdt* acpi_get_rsdt(struct acpi_rsdp *rsdp, int* acpi_rsdt_n);
static struct acpi_apic* acpi_get_apic(struct acpi_rsdt *rsdt, int acpi_rsdt_n);
static int acpi_apic_setup(struct acpi_apic *apic);
static void apic_print_info();

extern vm_offset_t kernel_virtual_end;

/*TODO: Implement ioapic support*/
struct ioapic ioapics[16];

//int cpu_to_lapic[NCPUS];
int cpu_to_lapic[255];

void*
pmap_aligned_table (unsigned long offset, unsigned long size)
{
  vm_offset_t addr;
  kern_return_t ret;
  uintptr_t into_page = offset % PAGE_SIZE;
  uintptr_t nearest_page = (uintptr_t)trunc_page(offset);
  
  size += into_page;

  ret = kmem_alloc_wired (kernel_map, &addr, round_page (size));
  
  if (ret != KERN_SUCCESS)
    return NULL;

  (void) pmap_map_bd (addr, nearest_page, nearest_page + round_page (size),
                      VM_PROT_READ | VM_PROT_WRITE);
                     

  /* XXX remember mapping somewhere so we can free it? */

  return (void *) (addr + into_page);
}


int
acpi_setup()
{
    struct acpi_rsdp *rsdp = 0;
    struct acpi_rsdt *rsdt = 0;
    int acpi_rsdt_n;
    

    printf("The kernel virtual end is %x\n", kernel_virtual_end);

    //Try to get rsdp pointer
    rsdp = acpi_get_rsdp();
    if(rsdp == NULL)
        return -1;
        
    printf("rsdp address %x\n", rsdp);

    //Try to get rsdt pointer
    rsdt = acpi_get_rsdt(rsdp, &acpi_rsdt_n);
    if(rsdt == NULL)
        return -1;

    printf("rsdt address %x\n", rsdt);

    apic_madt = acpi_get_apic(rsdt, acpi_rsdt_n);
    if(apic_madt == NULL) 
        return -1;
    
    printf("apic address %x\n", apic_madt);
    
    acpi_print_info(rsdp, rsdt, acpi_rsdt_n);


    if(acpi_apic_setup(apic_madt))
        return -1;

    apic_print_info();

    return 0;
}

void
acpi_print_info(struct acpi_rsdp *rsdp, struct acpi_rsdt *rsdt, int acpi_rsdt_n){

    printf("ACPI:\n");
    printf(" rsdp = %x; rsdp->rsdt_addr = %x\n", rsdp, rsdp->rsdt_addr);
    printf(" rsdt = %x; rsdt->length = %x (n = %x)\n", rsdt, rsdt->header.length,
           acpi_rsdt_n);
}


static int
acpi_checksum(void *addr, uint32_t length){
    char *bytes = addr;
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


static struct acpi_rsdp*
acpi_search_rsdp(void *addr, uint32_t length){
    struct acpi_rsdp *rsdp = NULL;

    void *end;
    /* check alignment */
    if((uint32_t)addr & (ACPI_RSDP_ALIGN-1))
        return NULL;

    //Search RDSP in memory space between addr and addr+lenght
    for(end = addr+length; addr < end; addr += ACPI_RSDP_ALIGN){

        //Check if the current memory block store the RDSP
        if(acpi_check_rsdp(addr) == 0){

            //If yes, store RSDP address
            rsdp = (struct acpi_rsdp*) addr;
            break;
        }
    }
    
    return rsdp;
}

struct acpi_rsdp*
acpi_get_rsdp(){
    struct acpi_rsdp *rsdp = NULL;
    uint16_t *start = 0x0;
    uint32_t base = 0x0;

    
    //EDBA start address
    start = (uint16_t*) phystokv(0x040e);
    base = *start;

    if(base != 0){  //Memory check

        base <<= 4; //base = base * 16

        //Search RSDP in first 1024 bytes from EDBA
        rsdp = acpi_search_rsdp((void*)base,1024);
        
        if(rsdp != NULL)
            return (struct acpi_rsdp*) rsdp;
    }

    //If RSDP isn't in EDBA, search in the BIOS read-only memory space between 0E0000h and 0FFFFFh
    rsdp = acpi_search_rsdp((void*) 0x0e0000, 0x100000 - 0x0e0000);
    
    return rsdp;
}


static int
acpi_check_rsdt(struct acpi_rsdt *rsdt){

    return acpi_checksum(rsdt, rsdt->header.length);
}

static struct acpi_rsdt*
acpi_get_rsdt(struct acpi_rsdp *rsdp, int* acpi_rsdt_n){
    phys_addr_t rsdt_phys;
    struct acpi_rsdt *rsdt = NULL;

    //Get rsdt address from rsdp
    rsdt_phys = rsdp->rsdt_addr;
    rsdt = (struct acpi_rsdt*) pmap_aligned_table(rsdt_phys, sizeof(struct acpi_rsdt));
    
    printf("found rsdt in address %x\n", rsdt);

    //Check is rsdt signature is equals to ACPI RSDT signature
    if(memcmp(rsdt->header.signature, ACPI_RSDT_SIG,
                sizeof(rsdt->header.signature)) != 0){
        printf("rsdt address checking failed\n");
        return NULL;
    }

    //Check if rsdt is correct
    if(acpi_check_rsdt(rsdt))
        return NULL;

    //Calculated number of elements stored in rsdt
    *acpi_rsdt_n = (rsdt->header.length - sizeof(rsdt->header))
        / sizeof(rsdt->entry[0]);


    return rsdt;
}

static struct acpi_apic*
acpi_get_apic(struct acpi_rsdt *rsdt, int acpi_rsdt_n){
     struct acpi_apic *apic = NULL;

    //Search APIC entries in rsdt array
    int i;
    struct acpi_dhdr *descr_header;
    for(i = 0;i < acpi_rsdt_n; i++){
        descr_header = (struct acpi_dhdr*) pmap_aligned_table(rsdt->entry[i], sizeof(struct acpi_dhdr));

        //Check if the entry contains an APIC
        if(memcmp(descr_header->signature, ACPI_APIC_SIG,
                    sizeof(descr_header->signature)) == 0){

            //If yes, store the entry in apic
            apic = (struct acpi_apic*) pmap_aligned_table(rsdt->entry[i], sizeof(struct acpi_apic));
            
            printf("found apic in address %x\n", apic);
            break;
        }
    }
    return apic;
}

static int
acpi_apic_setup(struct acpi_apic *apic){

    if(apic == 0)
        return -1;

    printf("checking apic checksum\n");

    //Check the checksum of the APIC
    if(acpi_checksum(apic, apic->header.length))
        return -1;

    printf("apic checksum successfull\n");

    ncpu = 0;
    nioapic = 0;

    //map common lapic address
    lapic = pmap_aligned_table(apic->lapic_addr, sizeof(ApicLocalUnit));
    printf("lapic mapped in address %x\n", lapic);
    
    printf("the lapic id of current cpu is %x\n", lapic->apic_id);

    struct acpi_apic_dhdr *apic_entry = apic->entry;
    uint32_t end = (uint32_t) apic + apic->header.length;

    printf("apic table end in address %x\n", end);

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
                if((lapic_entry->flags & 0x1)){

                    //Enumerate CPU and add It to cpu/apic vector
                    cpu_to_lapic[ncpu] = lapic_entry->apic_id;

                    printf("new cpu found with apic id %x\n", lapic_entry->apic_id);
    
                    //Increase number of CPU
                    ncpu++;                    
                    
                }
                break;

            //If APIC entry is an IOAPIC
            case ACPI_APIC_ENTRY_IOAPIC:

                //Store ioapic
               	ioapic_entry = (struct acpi_apic_ioapic*) apic_entry;

                /*Insert ioapic in ioapics array*/
                ioapics[nioapic].apic_id = ioapic_entry->apic_id;
                ioapics[nioapic].addr = ioapic_entry->addr;
                ioapics[nioapic].base = ioapic_entry->base;

                printf("new ioapic found with apic id %x\n", ioapics[nioapic].apic_id);

                //Increase number of ioapic
                nioapic++;
                break;
                
        }

        //Get next APIC entry
        apic_entry = (struct acpi_apic_dhdr*)((uint32_t) apic_entry
                + apic_entry->length);
    }


    if(ncpu == 0 || nioapic == 0)
        return -1;

    printf("%d cpus found. %d ioapics found\n", ncpu, nioapic);

    return 0;
}

static
void apic_print_info(){
    int i;

    printf("CPUS\n");
    printf("-------------------------------------------------\n");
    for(i = 0; i < ncpu; i++){
        printf("CPU %d - APIC ID %x\n", i, cpu_to_lapic[i]);
    }
    
    printf("\nIOAPICS\n");
    printf("-------------------------------------------------\n");
    
    for(i = 0; i < nioapic; i++){
        printf("IOAPIC %d - APIC ID %x\n", i, ioapics[i].apic_id);
    }
}