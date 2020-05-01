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
//#include <vm/vm_map_physical.h>
#include <kern/debug.h>
#include <intel/pmap.h>
#include <vm/vm_kern.h>

#define INTEL_PTE_R(p) (INTEL_PTE_VALID | INTEL_PTE_REF | pa_to_pte(p))

volatile ApicLocalUnit* lapic = (void*) 0;
uint32_t lapic_addr = 0;
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

int cpu_to_lapic[NCPUS];

void*
pmap_aligned_table (unsigned long offset, unsigned long size)
{
  vm_offset_t addr;
  kern_return_t ret;
  uintptr_t into_page = offset % PAGE_SIZE;
  uintptr_t nearest_page = (uintptr_t)trunc_page(offset);
  
  printf("into page %x, nearest page %x\n", into_page, nearest_page);
  
  size += into_page;
  
  printf("page size %x\n", size);

  ret = kmem_alloc_wired (kernel_map, &addr, round_page (size));
  
  printf("virtual address: &x\n", addr);
  
  if (ret != KERN_SUCCESS)
    return NULL;
    
  printf("alloc success\n");

  (void) pmap_map_bd (addr, nearest_page, nearest_page + round_page (size),
                      VM_PROT_READ | VM_PROT_WRITE);
                      
  printf("new address: %x\n", addr);

  /* XXX remember mapping somewhere so we can free it? */

  return (void *) (addr + into_page);
}


int
acpi_setup()
{
    struct acpi_rsdp *rsdp = 0;
    struct acpi_rsdt *rsdt = 0;
    int acpi_rsdt_n;
    struct acpi_apic *apic = 0;

    printf("The kernel virtual end is %x\n", kernel_virtual_end);

    //Try to get rsdp pointer
    rsdp = acpi_get_rsdp();
    if(rsdp == 0)
        return -1;
        
    printf("rsdp address %x\n", rsdp);

    //Try to get rsdt pointer
    rsdt = acpi_get_rsdt(rsdp, &acpi_rsdt_n);
    if(rsdt == 0)
        return -1;

    printf("rsdt address %x\n", rsdt);

    apic = acpi_get_apic(rsdt, acpi_rsdt_n);
    if(apic == 0) 
        return -1;
    
    acpi_print_info(rsdp, rsdt, acpi_rsdt_n);

    if(acpi_apic_setup(apic))
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
    int i;
    struct acpi_dhdr *descr_header;
    for(i = 0; i < acpi_rsdt_n; i++){
        descr_header = (struct acpi_dhdr*) rsdt->entry[i];
        printf("  %x: %c%c%c%c (%x)\n", i, descr_header->signature[0],
                descr_header->signature[1], descr_header->signature[2],
                descr_header->signature[3], rsdt->entry[i]);
    }

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
    struct acpi_rsdp *rsdp = (struct acpi_rsdp *) 0;

    void *end;
    /* check alignment */
    if((uint32_t)addr & (ACPI_RSDP_ALIGN-1))
        return (struct acpi_rsdp *) 0;

    //Search RDSP in memory space between addr and addr+lenght
    for(end = addr+length; addr < end; addr += ACPI_RSDP_ALIGN){

        //Check if the current memory block store the RDSP
        if(acpi_check_rsdp(addr) == 0){

            //If yes, store RSDP address
            rsdp = (struct acpi_rsdp*) addr;
        }
    }
    
    return rsdp;
}

struct acpi_rsdp*
acpi_get_rsdp(){
    struct acpi_rsdp *rsdp = (struct acpi_rsdp*) 0;
    uint16_t *start = 0x0;
    uint32_t base = 0x0;

    
    //EDBA start address
    start = (uint16_t*) phystokv(0x040e);
    base = *start;

    if(base != 0){  //Memory check

        base <<= 4; //base = base * 16

        //Search RSDP in first 1024 bytes from EDBA
        rsdp = acpi_search_rsdp((void*)base,1024);
        
        if(rsdp != 0)
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
    struct acpi_rsdt *rsdt = (struct acpi_rsdt*) 0;

    //Get rsdt address from rsdp
    rsdt_phys = rsdp->rsdt_addr;
    //rsdt = (struct acpi_rsdt*) pmap_get_mapwindow(INTEL_PTE_R(rsdt_phys))->vaddr;
    rsdt = (struct acpi_rsdt*) pmap_aligned_table(rsdt_phys, sizeof(struct acpi_rsdt));
    
    printf("found rsdt in address %x\n", rsdt);

    //Check is rsdt signature is equals to ACPI RSDT signature
    if(memcmp(rsdt->header.signature, ACPI_RSDT_SIG,
                sizeof(rsdt->header.signature)) != 0){
        printf("rsdt address checking failed\n");
        return (struct acpi_rsdt*) 0;
    }
    printf("rsdt address check finished\n");    

    //Check if rsdt is correct
    if(acpi_check_rsdt(rsdt))
        return (struct acpi_rsdt*) 0;

    //Calculated number of elements stored in rsdt
    *acpi_rsdt_n = (rsdt->header.length - sizeof(rsdt->header))
        / sizeof(rsdt->entry[0]);


    return rsdt;
}

static struct acpi_apic*
acpi_get_apic(struct acpi_rsdt *rsdt, int acpi_rsdt_n){
     struct acpi_apic *apic = (struct acpi_apic*) 0;

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
        }
    }
    return apic;
}

static int
acpi_apic_setup(struct acpi_apic *apic){

    if(apic == 0)
        return -1;

    //Check the checksum of the APIC
    if(acpi_checksum(apic, apic->header.length))
        return -1;

    ncpu = 0;
    nioapic = 0;


    /*
     * save lapic_addr in order to use it later for updating lapic,
     * in extra_setup()
     */
    lapic_addr = apic->lapic_addr;

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
                if((lapic_entry->flags & 0x1) && ncpu < NCPUS){

                    //Enumerate CPU and add It to cpu/apic vector
                    cpu_to_lapic[ncpu] = lapic_entry->apic_id;

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
    
    printf("IOAPICS\n");
    printf("-------------------------------------------------\n");
    
    for(i = 0; i < nioapic; i++){
        printf("IOAPIC %d - APIC ID %x", i, ioapics[i].apic_id);
    }
}

#if 0
int extra_setup()
{
  if (lapic_addr == 0)
  {
    printf("LAPIC mapping skipped\n");
    return 1;
  }
  vm_offset_t virt = 0;
  // TODO: FIX: it might be desirable to map LAPIC memory with attribute PCD
  //            (Page Cache Disable)
  long ret = vm_map_physical(&virt, lapic_addr, sizeof(ApicLocalUnit), 0);
  if (ret)
  {
    panic("Could not map LAPIC");
    return -1;
  }
  else
  {
    lapic = (ApicLocalUnit*)virt;
    printf("LAPIC mapped: physical: 0x%lx virtual: 0x%lx version: 0x%x\n",
           (unsigned long)lapic_addr, (unsigned long)virt,
           (unsigned)lapic->version.r);
    return 0;
  }
}
#endif
