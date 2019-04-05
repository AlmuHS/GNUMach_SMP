# Hurd_SMP project

## Objective
  The objective of this project is to fix and complete SMP support (multiprocessing) in GNU/Hurd. This support must be implemented in GNU/Hurd's microkernel (aka GNU Mach)

### Original status:
 GNU/Hurd includes a tiny SMP support, as this [FAQ](https://www.gnu.org/software/hurd/faq/smp.html) explain.  
 The GNU Mach source code includes many special cases for multiprocessor, controlled by `#if NCPUS > 1` macro. 
 
 But this support is very limited:

   - GNU Mach don't detect CPUs in runtime: The number of CPUs must be hardcoded in compilation time.  
    The number of cpus is set in `mach_ncpus` configuration variable, set to 1 by default, in `configfrag.ac` file.
    This variable will generate `NCPUS` macro, used by gnumach to control the special cases for multiprocessor.  
    If `NCPUS > 1`, gnumach will enable multiprocessor support, with the number of cpus set by the user in `mach_ncpus` variable.
    In other case, this support will be unabled. 

   - The special cases to multicore in gnumach source code have never been tested, so these can contain many errors.
  Furthermore, these special case are incomplete: many functions, as `cpu_number()` or `intel_startCPU()` aren't written. 

   -  GNU Mach doesn't initialize the processor with the properly options to multiprocessing. By this reason, the current support is only multithread, not real multiprocessor

  Added to this, there are other problem: Many drivers included in Hurd aren't thread-safe, and these could crash in a SMP environment. So, It's necessary to isolate this drivers, to avoid concurrency problems


### Solution

To solve this, we need to implement some routines to detect the number of processors, assign an identifier to each processor, and configure the lapic and IPI support. 
These routines must been executed during Mach boot.
   
   > "Really, all the support you want to get from the hardware is just getting the number of processors, initializing them, and   support for interprocessor interrupts (IPI) for signaling." - Samuel Thibault
    [link](https://lists.gnu.org/archive/html/bug-hurd/2018-08/msg00071.html)
    
   > "The process scheduler probably already has the support. What is missing
is the hardware driver for SMP: enumeration and initialization." - Samuel Thibault
   [link](https://lists.gnu.org/archive/html/bug-hurd/2018-08/msg00083.html)


   The current necessary functions are `cpu_number()` (in kern/cpu_number.h) and `intel_startCPU()`.
   Another not-implemented function, but don't critical, is `cpu_control()` [*Reference*](https://www.gnu.org/software/hurd/gnumach-doc/Processor-Control.html#Processor-Control)
   
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

	+ If you prefer to use a virtual machine as Qemu, you can use this script: [*qemu_hurd script*](https://gist.github.com/AlmuHS/73bae6dadf19b0482a34eaab567bfdfa). 
		-  Also, you can install It in VirtualBox!! ;)
		

- **Compile the sources**: From Debian GNU/Hurd, follow this steps:

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

## Task done

- Recovered and updated old APIC headers from Mach 4
- Modified `configfrag.ac`
	+ Now, if `mach_ncpus > 1`, `NCPUS` will be set to 255
- Integrated cpu detection and enumeration from acpi tables
- Solved memory mapping for `*lapic`. Now It's possible to read the Local APIC of the current processsor.
- Solved ioapic enumeration: changed linked list to array
- Initialized *master_cpu* variable to 0
- Initialized *ktss* for master_cpu
- Enabled cpus using StartUp IPI, and switched them to protected mode
	+ Loaded GDT and IDT



## Current status

- In the [Min_SMP](https://github.com/AlmuHS/Min_SMP/) test environment, the cpus are detected and started correctly
	+ I need to implement APIC configuration
- In *gnumach*, the number of cpus and its lapic structures are detected and enumerated correctly
- ioapic enumeration feels to work correctly
	+ Mach use PIC 8259 controller, so ioapic is not necessary. Migrate Mach to ioapic is a future TODO
- *gnumach* boots successfully with a only cpu, in SMP mode
- *gnumach* enable all cpus and boots successfully
	+ I need to "link" the cpus to the rest of the system

## Implementation 

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
	+ 	This call require that pagging is configured, so the call is added in [`kern/startup.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/0d31cc80e8f1e4f041568508b6b165b0174b4334/kern/startup.c#L133), after pagging configuration
- 	The cpus enabling is implemented in [`mp_desc.c`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/mp_desc.c)
	+ 	The routine to switch the cpus to protected mode is [`cpuboot.S`](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/cpuboot.S	)

## Gratitude

- [Bosco García](https://github.com/jbgg): Development guidance, original [MinSMP](https://github.com/jbgg/MinSMP) developer, explainations and documentation about MultiProcessor architecture, helpful with *gnumach* development. 

- Guillermo Bernaldo de Quiros Maraver: Helpful in original development, first full compilation, and find original SMP problem (SMP non-ACPI support)

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