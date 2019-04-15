#include <kern/cpu_number.h>
#include <i386at/acpi_rsdp.h>
#include <imps/apic.h>
#include <mach/machine.h>

unsigned int master_cpu = 0;	/* 'master' processor - keeps time */

int
cpu_number()
{
	if(ncpu == 1 | lapic == 0) return 0;
	else{ 
		unsigned apic_id = lapic->apic_id.r >>24;
		int i = 0;
		
		while(i< ncpu && machine_slot[i].apic_id != apic_id) i++;
		
		if(i == ncpu) return -1;
		else return i;

	}
}
