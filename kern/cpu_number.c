#include <kern/cpu_number.h>
#include <i386at/acpi_rsdp.h>
#include <imps/apic.h>
#include <mach/machine.h>

unsigned int master_cpu = 0;	/* 'master' processor - keeps time */

extern int lapic_addr;

int
cpu_number()
{
	if(ncpu == 1) return 0;
	else{ 
		//unsigned apic_id = lapic->apic_id.r >>24;
		unsigned apic_id = (((ApicLocalUnit*)phystokv(lapic_addr))->apic_id.r >> 24) & 0xff;

		return apic2kernel[apic_id];

	}
}
