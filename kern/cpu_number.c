int kernel2apic[NCPUS];
volatile int lapic = 0xFEE00020;/*FEE0 0020H*/

int cpu_number(void) {
  	int apic_id, i = 0;
	volatile int* apicid_ptr;	

	//Read apic id from the current cpu, using its lapic
	
	/* Each pointer register is 2 bytes (16 bits). Each field fill 16 memory position (1 byte/position)
	   Then, to skip to 3th field (16 bytes, two jumps), we have to multiply number of jumps (2 jumps) x 8 = 16
	*/
	
	apicid_ptr = (int*)lapic+16; //2 jumps (1 byte/position) x 8 bits	
	apic_id = *apicid_ptr;

	//Search apic id in cpu2apic vector
	while(kernel2apic[i] != apic_id && i < NCPUS) i = i+1;

	if(i == NCPUS) return -1;

	else return i;
}
