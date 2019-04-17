#include <kern/cpu_number.h>
#include <i386at/acpi_rsdp.h>
#include <imps/apic.h>
#include <mach/machine.h>

unsigned int master_cpu = 0;	/* 'master' processor - keeps time */

int
cpu_number()
{
	if(lapic == 0 | ncpu == 1) return 0;
	else{ 
		unsigned apic_id = lapic->apic_id.r >>24;

		return apic2kernel[apic_id];

	}
}
