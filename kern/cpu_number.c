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
		int apic_id = lapic->apic_id.r >>24;
		return apic2kernel[apic_id];
	}
}
