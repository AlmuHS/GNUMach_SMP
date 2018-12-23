int kernel2apic[NCPUS];
volatile int lapic = 0xFEE00020;/*FEE0 0020H*/

int cpu_number(void) {
  /*int eax = 1, ebx = 0, ecx = 0, edx = 0;
  unsigned int i = 0;
  int apic_id = 0;
	
  asm("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (eax));
  apic_id = (char) (ebx >> 24) & 0xff;

	while(kernel2apic[i] != apic_id && i < numcpus) i++;
	if(i == numcpus){
		kernel2apic[i] = apic_id;
		numcpus++;
	}*/


  int apic_id, i = 0;
	volatile int* apicid_ptr;	

	//Read apic id from the current cpu, using its lapic
	
	/* Each pointer register is 2 bytes (16 bits). Each field fill 16 memory position (1 byte/position)
	   Then, to skip to 3th field (16 bytes, two jumps), we have to multiply number of jumps (2 jumps) x 8 = 16
	*/
	
	apicid_ptr = lapic+16; //2 jumps (1 byte/position) x 8 bits	
	apic_id = *apicid_ptr;

	//Search apic id in cpu2apic vector
	while(kernel2apic[i].apic_id != apic_id && i < NCPUS) i = i+1;

	if(i == NCPUS) return -1;

	else return i;
}
