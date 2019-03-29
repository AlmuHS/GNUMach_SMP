#include <kern/cpu_number.h>
#include <i386at/acpi_rsdp.h>

int
cpu_number()
{
	if(ncpu == 1) return 0;
	else if(lapic != 0){ 

		unsigned apic_id = lapic->apic_id.r >>24;
		int i = 0;
		
		while(i< ncpu && machine_slot[i].apic_id != apic_id) i++;
		
		if(i == ncpu) return -1;
		else return i;
	}
	else return 0;
}
