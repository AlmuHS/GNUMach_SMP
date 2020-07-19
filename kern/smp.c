#include <kern/smp.h>

struct smp_data smp_info;

#ifdef __i386__
    #include <i386/i386/apic.h>
    #include <i386/i386at/acpi_parse_apic.h>

    void smp_data_init(void)
        {
                smp_info.num_cpus = apic_get_numcpus();
        }

    int get_numcpus(void)
        {
            return smp_info.num_cpus;
        }

    int get_current_cpu(void)
        {
            return apic_get_current_cpu();
        }

    int smp_init(void)
        {
            int apic_success = acpi_apic_init();
            if (apic_success)
                {
                    smp_data_init();
                }

            return apic_success;
        }

#endif
