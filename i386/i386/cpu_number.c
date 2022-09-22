#include <i386/cpu_number.h>
#include <i386/apic.h>
#include <i386/smp.h>

int cpu_number(){
        int apic_id = apic_get_current_cpu();
        int num_cpus = smp_get_numcpus();
        
        //printf("num_cpus: %d\n", num_cpus);
        
        int kernel_id = 0;
        while(apic_id != apic_get_cpu_apic_id(kernel_id) && kernel_id < num_cpus) kernel_id++;
        
        if(kernel_id >= num_cpus) return 0;
        
        return kernel_id;
}
