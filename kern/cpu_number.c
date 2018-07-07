#define MAX_CPUS 32  /* (8*sizeof(long)) */

int kernel2apic[MAX_CPUS];
int apic2kernel[MAX_CPUS];
int cpu_number_start = 0, cpu_number_counter = 0;

int cpu_number(void) {
  int eax = 1, ebx = 0, ecx = 0, edx = 0;
  unsigned int i = 0;
  int apic_id = 0;

  if (!cpu_number_start) {
    for (i = 0; i < MAX_CPUS; i++) {
      apic2kernel[i] = -1;
	  kernel2apic[i] = -1;
    }
    cpu_number_start = 1;
  }

  asm("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (eax));
  apic_id = (char) (ebx >> 24) & 0xff;

  if (apic2kernel[apic_id] != -1) {
    return apic2kernel[apic_id];
  } else {
    apic2kernel[apic_id] = cpu_number_counter;
	kernel2apic[cpu_number_counter] = apic_id;
    cpu_number_counter++;
  }

  return apic2kernel[apic_id];
}
