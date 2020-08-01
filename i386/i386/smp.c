#include <i386/i386/apic.h>
#include <i386/i386/smp.h>
#include <i386/i386at/acpi_parse_apic.h>

#include <kern/smp.h>

struct smp_data smp_info;

/*
 * smp_data_init: initialize smp_data structure
 * Must be called after smp_init(), once all APIC structures
 * has been initialized
 */
void smp_data_init(void)
{
    smp_info.num_cpus = apic_get_numcpus();
}

/*
 * smp_get_current_cpu: return the hardware identifier (APIC ID in x86)
 * of current CPU
 */
int smp_get_current_cpu(void)
{
    return apic_get_current_cpu();
}

/*
 * smp_init: initialize the SMP support, starting the cpus searching
 * and enumeration.
 */
int smp_init(void)
{
    int apic_success;

    apic_success = acpi_apic_init();
    if (apic_success) {
        smp_data_init();
    }

    return apic_success;
}

/*
 * smp_get_numcpus: returns the number of cpus existing in the machine
 */
int smp_get_numcpus(void)
{
    return smp_info.num_cpus;
}
