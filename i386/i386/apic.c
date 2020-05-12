#include <i386/apic.h>
#include <string.h>

volatile ApicLocalUnit* lapic = NULL;

int nirqoverride = 0;
struct irq_override_data irq_override_list[24];

struct smp_info smp_data;

void
smp_data_init(void)
    {
        smp_data.ncpus = 0;
        smp_data.nioapics = 0;
    }

void
add_cpu(uint16_t apic_id)
    {
        int numcpus = smp_data.ncpus;
        smp_data.lapic_list[numcpus] = apic_id;
        smp_data.ncpus++;
    }


void
set_lapic(ApicLocalUnit* lapic_ptr)
    {
        lapic = lapic_ptr;
    }

void
add_ioapic(struct ioapic_data ioapic)
    {
        int nioapic = smp_data.nioapics;

        smp_data.ioapic_list[nioapic] = ioapic;

        smp_data.nioapics++;
    }


void
add_irq_override(struct irq_override_data irq_over)
    {
        int nirq = nirqoverride;

        irq_override_list[nirq] = irq_over;
        nirqoverride++;
    }

uint16_t
get_cpu_apic_id(int kernel_id)
    {
        uint16_t apic_id;

        if(kernel_id < 256)
            {
                apic_id = smp_data.lapic_list[kernel_id];
            }
        else
            {
                apic_id = -1;
            }

        return apic_id;
    }

ApicLocalUnit*
get_lapic(void)
    {
        return lapic;
    }


struct ioapic_data
get_ioapic(int kernel_id)
    {
        struct ioapic_data io_apic;

        if(kernel_id < 16)
            {
                io_apic = smp_data.ioapic_list[kernel_id];
            }

        return io_apic;
    }

int
get_numcpus(void)
    {
        return smp_data.ncpus;
    }

int
get_num_ioapics(void)
    {
        return smp_data.nioapics;
    }
