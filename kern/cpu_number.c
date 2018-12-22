int kernel2apic[MAX_CPUS];

int cpu_number_start = 0, numcpus = 0;

int cpu_number(void) {
  int eax = 1, ebx = 0, ecx = 0, edx = 0;
  unsigned int i = 0;
  int apic_id = 0;
	
  asm("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (eax));
  apic_id = (char) (ebx >> 24) & 0xff;

	while(kernel2apic[i] != apic_id && i < numcpus) i++;
	if(i == numcpus){
		kernel2apic[i] = apic_id;
		numcpus++;
	}

  return i;
}
