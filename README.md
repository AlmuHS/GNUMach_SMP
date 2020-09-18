# Hurd_SMP project

## Objective
The objective of this project is to fix and complete SMP support (multiprocessing) in GNU/Hurd. This support must be implemented in GNU/Hurd's microkernel (aka GNU Mach) as opposed to the system servers.

### Original status:
 GNU/Hurd includes some SMP support, as this [FAQ](https://www.gnu.org/software/hurd/faq/smp.html) explains.  
 The GNU Mach source code includes many special cases for multiprocessing, controlled by `#if NCPUS > 1` macro. 
 
 But this support is very limited:

   - GNU Mach cannot detect CPUs at runtime: The number of CPUs must be hardcoded at compilation time.  
    The number of cpus is set in `mach_ncpus` configuration variable, and is set to 1 by default in the `configfrag.ac` file.
    This variable will generate the `NCPUS` macro, used by gnumach to control the special cases for multiprocessing.  
    If `NCPUS > 1`, gnumach will enable multiprocessor support, with the number of cpus set by the user in `mach_ncpus` variable.
    In other cases, this support will be disabled. 

   - The special cases for multicore processing in gnumach source code have never been tested, you may encounter many errors while testing them.
  Furthermore, these special case are incomplete: many functions, such as `cpu_number()` or `intel_startCPU()` are not yet written. 

   -  GNU Mach doesn't initialize the processor with the proper options for multiprocessing. For this reason, the current support is only for multithreading, not real multiprocessing.

  In addition to this, there are other problems: many drivers included in the Hurd aren't thread-safe, and these could crash in an SMP environment. So, it's necessary to isolate these drivers, to avoid concurrency problems.


### Solution

To solve this, we need to implement some routines to detect the number of processors, assign an identifier to each processor, and configure the lapic and IPI support. 
These routines must been executed during Mach boot.
   
   > "Really, all the support you want to get from the hardware is just getting the number of processors, initializing them, and   support for interprocessor interrupts (IPI) for signaling." - Samuel Thibault
    [link](https://lists.gnu.org/archive/html/bug-hurd/2018-08/msg00071.html)
    
   > "The process scheduler probably already has the support. What is missing
is the hardware driver for SMP: enumeration and initialization." - Samuel Thibault
   [link](https://lists.gnu.org/archive/html/bug-hurd/2018-08/msg00083.html)


   The current necessary functions are `cpu_number()` (in kern/cpu_number.h) and `intel_startCPU()`.
   Another not-implemented function, but not as critical, is `cpu_control()` [*Reference*](https://www.gnu.org/software/hurd/gnumach-doc/Processor-Control.html#Processor-Control)
   
   Other interesting files are `pmap.c` and `sched_prim.c`
 
   Added to this, we have to build an isolated environment to execute the non-thread-safe drivers.

   > "Yes, this is a real concern. For the Linux drivers, the long-term goal is to move them to userland anyway. For Mach drivers, quite often they are not performance-sensitive, so big locks would be enough." - Samuel Thibault
   [link](https://lists.gnu.org/archive/html/bug-hurd/2018-08/msg00073.html)
  
## Project draft  
  
  You can read the full project draft in [*Hurd SMP Project draft*](https://gitlab.com/snippets/1756024)
  
      
## How to test

To test the software you will need:
	
- **Debian GNU/Hurd installation**:  The Debian GNU/Hurd installer is pretty similar to a standard Debian installer.  

	+ You can follow this guide to learn how to install Debian GNU/Hurd  
	     [*Install Debian GNU/Hurd in real hardware*](https://gist.github.com/AlmuHS/f0c036631881756e817504d28217a910)

	+ If you prefer to use a virtual machine such as Qemu, you can use this script: [*qemu_hurd script*](https://gist.github.com/AlmuHS/73bae6dadf19b0482a34eaab567bfdfa). 
		-  Also, you can install It in VirtualBox!! ;)
		

- **Compile the sources**: In a Debian GNU/Hurd environment, follow these steps:

	1. Clone the repository:  
	
		   git clone https://github.com/AlmuHS/GNUMach_SMP
	2. Install the dependencies  
	  
	  		apt-get install build-essential fakeroot
	  		apt-get build-dep gnumach
	  		apt-get install mig
	  
	3. Configure preliminary steps
	  
		   cd GNUMach_SMP
		   autoreconf --install
			  
		   #create build directory
		   mkdir build
		   cd build
			  
		   ../configure --prefix=
	  
	4. Compile!!
	  
	       make gnumach.gz
	       
	 5. Copy the new image to /boot directory (as root)
	   
	   	    cp gnumach.gz /boot/gnumach-smp.gz
	   	    
	 6. Update grub (as root)
	   
	        update-grub
	       
	 7. Reboot
	   
	        reboot
	 
	 	After reboot, you must to select gnumach-smp.gz in GRUB menu

More info in: <https://www.gnu.org/software/hurd/microkernel/mach/gnumach/building.html>

## Completed tasks

- Recovered and updated old APIC headers from Mach 4
- Modified `configfrag.ac`
	+ Now, if `mach_ncpus > 1`, `NCPUS` will be set to 255
- Integrated cpu detection and enumeration from acpi tables
- Solved memory mapping for `*lapic`. Now It's possible to read the Local APIC of the current processor.
- Implemented `cpu_number()` function
- Solved ioapic enumeration: changed linked list to array
- Initialized *master_cpu* variable to 0
- Initialized *ktss* for master_cpu
- Enabled cpus using StartUp IPI, and switched them to protected mode
	+ Loaded temporary GDT and IDT
- Implemented assembly `CPU_NUMBER()`
- Refactorized `cpu_number()` with a more efficient implementation
- Added interrupt stack to cpus
- Improve memory reserve to cpu stack, using Mach style (similar to interrupt stack)
- Enabled paging in AP processors
- Loaded final GDT and IDT
- Added cpus to scheduler


## Current status

- In the [Min_SMP](https://github.com/AlmuHS/Min_SMP/) test environment, the cpus are detected and started correctly
	+ I need to implement APIC configuration
- In *gnumach*, the number of cpus and its lapic structures are detected and enumerated correctly
- ioapic enumeration fails
	+ Mach use PIC 8259 controller, so ioapic is not necessary. Migrate Mach to ioapic is a future TODO
- *gnumach* enable all cpus during the boot successfully
- The cpus are added successfully to the kernel
- *gnumach* boots with 2 cpu
	+ It fails with more than 2 cpus, and with only one cpu. TODO: fix It
- Some Hurd servers fail
	+ DHCP client crash during the boot
	+ Login screen does not receive keyboard input

## Implementation 

### Summary

- The cpu detection and enumeration are implemented in [`acpi_rdsp.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/acpi_rsdp.c) and [`acpi_rdsp.h`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/acpi_rsdp.h).  
	+ The main function [`acpi_setup()`](https://github.com/AlmuHS/GNUMach_SMP/blob/444206e0cd7ddc13bbf785382700c64db2e76f7c/i386/i386at/acpi_rsdp.c#L47) is called from [`model_dep.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/444206e0cd7ddc13bbf785382700c64db2e76f7c/i386/i386at/model_dep.c#L411)
	+ This function generates some structures:
		* 	[`*lapic`](https://github.com/AlmuHS/GNUMach_SMP/blob/43f56f0ad67f3c27a15778f311579c91f0b0775c/i386/i386at/acpi_rsdp.c#L27): pointer to the local apic of the current processor. Store the registers of the local apic.  
		* [	`ncpu`](https://github.com/AlmuHS/GNUMach_SMP/blob/43f56f0ad67f3c27a15778f311579c91f0b0775c/i386/i386at/acpi_rsdp.c#L29): variable which store the number of cpus 
	+ 	The `apic_id` is stored in [`machine_slot`](https://github.com/AlmuHS/GNUMach_SMP/blob/444206e0cd7ddc13bbf785382700c64db2e76f7c/include/mach/machine.h#L75)
		
- 	The APIC structures, recovered from old *gnumach* code, are stored in [`apic.h`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/imps/apic.h)
- 	[`cpu_number()`](https://github.com/AlmuHS/GNUMach_SMP/blob/444206e0cd7ddc13bbf785382700c64db2e76f7c/kern/cpu_number.h#L46) C implementation was added to `kern/cpu_number()`.
- 	The [`CPU_NUMBER()`](https://github.com/AlmuHS/GNUMach_SMP/blob/444206e0cd7ddc13bbf785382700c64db2e76f7c/i386/i386/cpu_number.h#L48) assembly implementation was added to `i386/i386/cpu_number.h`
- 	Function [`start_other_cpus()`](https://github.com/AlmuHS/GNUMach_SMP/blob/444206e0cd7ddc13bbf785382700c64db2e76f7c/i386/i386/mp_desc.c#L351) was modified, to change `NCPUS` macro to `ncpu` variable
- 	The memory mapping is implemented in [`vm_map_physical.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/vm/vm_map_physical.c) and [`vm_map_physical.h`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/vm/vm_map_physical.h)
	+ 	The lapic mapping is in [`extra_setup()`](https://github.com/AlmuHS/GNUMach_SMP/blob/0d31cc80e8f1e4f041568508b6b165b0174b4334/i386/i386at/acpi_rsdp.c#L297)
	+ 	This call requires that paging is configured, so the call is added in [`kern/startup.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/0d31cc80e8f1e4f041568508b6b165b0174b4334/kern/startup.c#L133), after paging configuration
- 	The cpus enabling is implemented in [`mp_desc.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/mp_desc.c)
	+ 	The routine to switch the cpus to protected mode is [`cpuboot.S`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/cpuboot.S	)
- 	[`cpu_number()`](https://github.com/AlmuHS/GNUMach_SMP/blob/44c79ab18042c94996114ebeb233b8bd0033411d/kern/cpu_number.c#L9) has been refactorized, replacing the while loop with the array [`apic2kernel[]`](https://github.com/AlmuHS/GNUMach_SMP/blob/44c79ab18042c94996114ebeb233b8bd0033411d/i386/i386at/acpi_rsdp.c#L45), indexed by apic_id
- 	[`CPU_NUMBER() `](https://github.com/AlmuHS/GNUMach_SMP/blob/44c79ab18042c94996114ebeb233b8bd0033411d/i386/i386/cpu_number.h#L48) assembly function has been implemented using `apic2kernel[]` array
- 	Added call to `interrupt_stack_alloc()` before `mp_desc_init()`
- 	Added paging configuration in [`cpuboot.S`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/cpuboot.S	)
- 	Added calls to `gdt_init()` and `idt_init()` before call to `slave_main()`, to load final GDT and IDT.
- 	Enabled call to `slave_main()`, to add AP processors to the kernel
- 	Moved paging configuration to `paging_setup()` function
- 	Solved little problem with AP stack: now each AP has their own stack

### Recover old *gnumach* APIC headers

We have recovered the [`apic.h`](http://git.savannah.gnu.org/cgit/hurd/gnumach.git/commit/i386/imps/apic.h?id=0266d331d780ff0e595eda337a3501ffbfea9330) header, original from Mach 4, with Local APIC and IOAPIC structs, and an old implementation of [`cpu_number()`](http://git.savannah.gnu.org/cgit/hurd/gnumach.git/diff/i386/imps/cpu_number.h?id=0266d331d780ff0e595eda337a3501ffbfea9330).

- `cpu_number()` C implementation was moved to [`kern/cpu_number.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/kern/cpu_number.c), and the assembly `CPU_NUMBER()` implementation was moved to [`i386/i386/cpu_number.h`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/cpu_number.h)

- `struct ApicLocalUnit` was updated to the latest Local APIC fields, and stored in [`imps/apic.h`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/imps/apic.h)


### CPU detection and enumeration

In this step, we find the Local APIC and IOAPIC registers in the ACPI tables, and enumerate them.

The implementation of this step is based in [Min_SMP acpi.c](https://github.com/AlmuHS/Min_SMP/blob/master/acpi.c) implementation. The main function is `acpi_setup()`, who call to other functions to go across ACPI tables. 

To adapt the code to *gnumach* it was necessary to make some changes:

- **Copy and rename files**

	The [`acpi.c`](https://github.com/AlmuHS/Min_SMP/blob/master/acpi.c) and [`acpi.h`](https://github.com/AlmuHS/Min_SMP/blob/master/acpi.h) files were renamed to [`acpi_rsdp.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/acpi_rsdp.c) and [`acpi_rsdp.h`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/acpi_rsdp.h)  
	  
	These files were copied in `i386/i386at/..` directory.

- **Change headers and move variables**
  
  The #include headers must be changed to the *gnumach* equivalent.
  Some variables declared in [`cpu.c`](https://github.com/AlmuHS/Min_SMP/blob/master/cpu.c) were moved to [`acpi_rsdp.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/acpi_rsdp.c) or other files:
  
  - The number of cpus, `ncpu`, was moved to [`acpi_rsdp.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/acpi_rsdp.c)
  - The lapic ID, stored in [`cpus[]`](https://github.com/AlmuHS/Min_SMP/blob/master/acpi.c) array, was added to [`machine_slot[NCPUS]`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/kern/machine.c) array, and the cpus[] array was removed.
  - The lapic pointer extern declaration was added to `kern/machine.h`
  - The `struct list ioapics` was changed to `ioapics[16]` array, in [`acpi_rsdp.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/acpi_rsdp.c)
  - `struct ioapic` was moved to [`imps/apic.h`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/imps/apic.h)
   
- **Replace physical address with logical address**

	The most important modification is to replace the physical address with the equivalent logical address. To ease this task, this function is called before  paging is configured.
	
	The memory address below 0xc0000000 are mapped directly by the kernel, and their logical address can be got using the macro [`phystokv(address)`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/vm_param.h). This way is used to get the logical address of ACPI tables pointers. 
	
	But the lapic pointer is placed in a high memory position, up to 0xf0000000, so It must be mapped manually. To map this address, we need to use paging, which is not configured yet. To solve this, we split the process into two steps:
	
	- In APIC enumeration step, we store the lapic address in a temporary variable: `lapic_addr`
	- After paging is configured, we [call](https://github.com/AlmuHS/GNUMach_SMP/blob/434cf68e9daacdc3bb2b6e1d37c895c5045f8eb5/kern/startup.c#L133) to function [`extra_setup()`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/acpi_rsdp.c) which reserves the memory address to the lapic pointer and initialize the real pointer, `*lapic`.

### Implementation of `cpu_number()` function

Once get the lapic pointer, we could use this pointer to access to the Local APIC of the current processor. Using this, we have implemented `cpu_number()` function, which searches in `machine_slot[]` array for the apic_id of the current processor, and returns the index as kernel ID. 

A newer implementation gets the Kernel ID from the `apic2kernel[]` array, using the apic_id as index.

This function will be used later to get the cpu working.

### CPU enabling using StartUp IPI

In this step, we enable the cpus using the StartUp IPI. To do this, we need to write the ICR register in the Local APIC of the processor who raised the IPI (in this case, the BSP raises the IPI to each processor).

To implement this step, we have been inspired in Min_SMP [`mp.c`](https://github.com/AlmuHS/Min_SMP/blob/master/mp.c) and [`cpu.c`](https://github.com/AlmuHS/Min_SMP/blob/master/cpu.c) files, and based in the existent work in [`i386/i386/mp_desc.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/mp_desc.c)

We have split this task into several steps:

- **Modify `start_other_cpus()`**

  The `start_other_cpus()` function calls to `cpu_start(cpu)` for each cpu, to enable It.
  We have modified this function to change the `NCPUS` macro to `ncpu` variable, reserve
  memory to the cpu stack, and then initialize the `machine_slot[]` to indicate cpu is enabled.  
    
  Furthermore, we have added some printfs to show the number of cpus and the kernel ID of current cpu. 
  
  - **Reserve memory for cpu stack**
	  
	  To implement this step, we token the interrupt stack code as base, using the function  `interrupt_stack_alloc()` .
	  
	We have added two new arrays, to store the pointer to the stack of each cpu.
	
	- `cpu_stack[]` store the pointer to the stack
	- `_cpu_stack_top[]` store the address of stack top
	
	All stack use a single memory reserve. In this way, we only reserve a single memory block, which will be split between each cpu stack. To reserve the memory, we call to `init_alloc_aligned()`, which reserves memory from the BIOS area. This function returns the initial address of the memory block, which is stored in `stack_start`.
	
	All stacks have the same size, which is defined in the `STACK_SIZE` macro.
	
	Once the memory is reserved, we assign the slides to each cpu using `stack_start` as the base address. In each step, we assign `stack_start` to `cpu_stack[cpu]`, `stack_start+STACK_SIZE` to `_cpu_stack_top[cpu]`, and increase `stack_size` with `STACK_SIZE`

	To ease the stack loading to each cpu, we have added a unique stack pointer, called `stack_ptr`. Before enable each cpu, this pointer is updated to the `cpu_stack` of the current cpu. This pointer will be used in the `cpuboot.S` assembly routine to load the stack in the current cpu.
	
	  
- **Complete `intel_startCPU()`**
  
  The `intel_startCPU()` function has the purpose of enabling the cpu indicated by parameter, calling to `startup_cpu()` to raise the Startup IPI, and check if the cpu has been enabled correctly.
  
  This function is based on [XNU's `intel_startCPU()` function](https://github.com/nneonneo/osx-10.9-opensource/blob/f5a0b24e4d98574462c8f5e5dfcf0d37ef7e0764/xnu-2422.1.72/osfmk/i386/mp.c#L423), replacing its calls to the *gnumach* equivalent, and removing garbage code blocks.
  
- **Raise Startup IPI and initialize cpu**

  *gnumach* doesn't include any function to raise the Startup IPI, so we have implemented these functionalities in Min_SMP [`cpu.c`](https://github.com/AlmuHS/Min_SMP/blob/master/cpu.c) and [`mp.c`](https://github.com/AlmuHS/Min_SMP/blob/master/mp.c ): 
  - `startup_cpu()`: This function is called by `intel_startCPU()` to begin the Startup IPI sequence in the cpu. 
  - `send_ipi()`: function to write the IPI fields in the ICR register of the current cpu
  - `cpu_ap_main()`: The first function executed by the new cpu after startup. Calls to `cpu_setup()` and check errors.
  - `cpu_setup()`: Initialize the `machine_slot` fields of the cpu
  
  These functions has been added to [`i386/i386/mp_desc.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/mp_desc.c)
  
- **Implement assembly routine to switch the cpu to protected mode**

	After Startup IPI is raised the cpu starts in real mode, so we need to add a routine to switch the cpu to protected mode. Because real mode is 16 bit, we can't use C instructions (32 bit), so this routine must be written in assembly.
	
	This routine loads the GDT and IDT registers in the cpu, and calls to `cpu_ap_main()` to initialize the `machine_slot` of the cpu.
	
	To write the routine, we have taken Min_SMP [`boot.S`](https://github.com/AlmuHS/Min_SMP/blob/master/boot.S) as a base, with a few modifications:
	
	- The GDT descriptors are replaced with *gnumach* GDT descriptors (`boot_gdt:` and `boot_gdt_descr:`), taken from [`boothdr.S`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/boothdr.S).
	We also copied the register initialization after GDT loading.
	
	- The `_start` routine is unnecessary and has been removed 
	- The physical address has been replaced with their equivalent logical address, using the same shift used in `boothdr.S`
	- We have removed the `hlt` instruction after `call cpu_ap_main`
	
   The final code is stored in [`i386/i386/cpuboot.S`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/cpuboot.S)

### Add interrupt stack to cpus
  
  To allow cpus to execute interrupt handlers an interrupt stack is needed.
  Each cpu has its own interrupt stack. 
  
  To get this, we've added a call to `interrupt_stack_alloc()` to initialize the cpus interrupt stack array before call to `mp_desc_init()`.
 
  This step has not shown any noticeable changes yet.

### Enable paging in the cpus (WIP)

Before we can add the cpus to the kernel we need to configure paging  to fully allow access to memory.

To enable paging, we need to initialize CR0, CR3 and CR4 registers. In a similar way to [this](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386at/model_dep.c#L477-L500).

This code has been copied in `paging_setup()` function, in `mp_desc.c`. The processor, at startup, isn't capable of reading the content from a pointer, so we copied the memory address of `kernel_page_dir` and `pdpbase` into two temporary integer variables: `kernel_page_dir_addr`, and `pdpbase_addr`.

The paging initialization also requires a temporary mapping in some low memory address.
We keep the temporary mapping done in BSP processor until all APs have be enabled.

## Add AP processors to the kernel

Once paging is enabled, each cpu will can to read its own Local APIC, using the `*lapic` pointer. It also allows for the execution of `cpu_number()`, which is necessary to execute the [`slave_main()`](https://github.com/AlmuHS/GNUMach_SMP/blob/eb8b0bbf6299a0c19fca12ab71b810447f0d47f5/kern/startup.c#L282) function to add the cpu to the kernel.

Before call to `slave_main()`, we need to load the final GDT and IDT, to get the same value than BSP processor, and be able to correctly load the LDT entries.

To do this, we call to `gdt_init()` and `idt_init()` in `cpu_setup()`, just before we call `slave_main()`.

Once final GDT and IDT are loaded, and `slave_main()` finishes successfully, the AP processors are added to the kernel.


## Gratitude

- [Bosco García](https://github.com/jbgg): Development guidance, original [MinSMP](https://github.com/jbgg/MinSMP) developer, explanations and documentation about MultiProcessor architecture, helpful with *gnumach* development. 

- Guillermo Bernaldo de Quiros Maraver: Helpful in original development, first full compilation, and find original SMP problem (SMP without APIC support)

- [Samuel Thibault](https://github.com/sthibaul): Hurd core dev. Clarifications about SMP status in gnumach, helpful with gnumach questions.  

- [Distribu D](https://github.com/neutru): Helpful with debugging and memory addressing

  
## References

 - [Comments about the project bug-hurd maillist](https://lists.gnu.org/archive/html/bug-hurd/2018-08/msg00048.html)
 - [Initial thread in bug-hurd maillist](https://lists.gnu.org/archive/html/bug-hurd/2018-06/msg00048.html)
 - [SMP in GNU/Hurd FAQ](https://www.gnu.org/software/hurd/faq/smp.html)
 - [GNU Mach git repository](http://git.savannah.gnu.org/cgit/hurd/gnumach.git)
 - [Old Mach 4 source code](ftp://ftp.lip6.fr/pub/mach/mach4/mach/)
 - [**GNUMach_SMP repository**](https://github.com/AlmuHS/GNUMach_SMP)
 - [GNU Mach reference manual](https://www.gnu.org/software/hurd/gnumach-doc/)
 - [FOSDEM speaking: roadmap for the Hurd](http://ftp.belnet.be/mirror/FOSDEM/2019/AW1.121/roadmap_for_the_hurd.webm)
 - [**GENIAL ARTICLE about GNU/Hurd architecture**](https://blog.darknedgy.net/technology/2015/07/25/0/)
 - [**MultiProcessor Specification**](https://pdos.csail.mit.edu/6.828/2011/readings/ia32/MPspec.pdf)
 - [**ACPI Specification**](http://www.uefi.org/sites/default/files/resources/ACPI%206_2_A_Sept29.pdf)
 - [Mach boot trace](https://www.gnu.org/software/hurd/microkernel/mach/gnumach/boot_trace.html)
 - [Book: The Mach System](http://codex.cs.yale.edu/avi/os-book/OS9/appendices-dir/b.pdf)
 - [X15 operating system](https://www.sceen.net/x15)
 - [Symmetric Multiprocessing - OSDev Wiki](https://wiki.osdev.org/Symmetric_Multiprocessing)
 - [**Intel Developer Guide, Volume 3: System Programming Guide**](https://software.intel.com/sites/default/files/managed/a4/60/325384-sdm-vol-3abcd.pdf)
