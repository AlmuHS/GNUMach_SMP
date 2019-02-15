# Hurd_SMP project

## Objective
  The objective of this project is to fix and complete SMP support (multiprocessing) in GNU/Hurd. This support must be implemented in GNU/Hurd's microkernel (aka GNU Mach)

### Current status:
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

### Task list

  1. Implement a routine to detect and identify the processors: This routine must check the number of processors, initialize the lapic of BSP (the master processor), and assign a kernel ID for each processor. This kernel ID don't have to be equal to the APIC ID. 
  
     The relation Kernel/APIC can be setted with an array, where the kernel ID is the index, and the APIC data the contains.  
     The list of processors can be get from memory, reading from ACPI table or MP table. MP table is deprecated in most modern CPUs, so It's prefer to use ACPI table to this
     
     The task to do in this routine are:
     
       - Detect the number of processors
         - Creates a array indexed by kernel ID, which set a relation with APIC ID.
       - Initialize the lapic of BSP
       - Initialize IOAPIC
     

      This routine could be called from `i386at_init()` (i386/i386at/model_dep.c). This function will call to the functions which initialize the lapic and the ioapic.  
      **NOTE**: This routine must be execute in first time, previously to `intel_startCPU()` or other routines.


        - **How to find APIC table**
     
           To find APIC table, we can read RSDT table [RSDT reference](http://www.uefi.org/sites/default/files/resources/ACPI%206_2_A_Sept29.pdf#G10.1358180).
           To get the address of RSDT, we need to read RDSP table. The RSDP table can be getted as this [RDSP reference](http://www.uefi.org/sites/default/files/resources/ACPI%206_2_A_Sept29.pdf#G10.1357698)  
           Once got RSDT table, we need to read *Entry* field, and search the pointer to the APIC table in the array referenced in this field.  
     
           We can find an example about reading ACPI table in X15 OS: [Reference](https://github.com/richardbraun/x15/blob/0c0e2a02a42a8161e1b8dc1e1943fe5057ecb3a3/arch/x86/machine/acpi.c#L576)  

        - **Old LAPIC table**: in old versions of gnumach, was a little structure for lapic and  APIC registers.
            We can find this code here: [`apic.h`](http://git.savannah.gnu.org/cgit/hurd/gnumach.git/commit/i386/imps/apic.h?id=0266d331d780ff0e595eda337a3501ffbfea9330)
            This code was removed in this commit, but could be interesting recover and update It.
     
        - Other tasks
        
            Also, we need to initialize the `machine_slot` of each processor (currently only initializes cpu0). 
      
            The `machine_slot` has this structure. [Reference](https://github.com/AlmuHS/GNUMach_SMP/blob/0d490ef21c156907f3f26a6cdc00842f462a877a/include/mach/machine.h#L68):
      
                      
                      struct machine_slot {
                        /*boolean_t*/integer_t	is_cpu;		/* is there a cpu in this slot? */
                        	cpu_type_t	cpu_type;	/* type of cpu */
                        	cpu_subtype_t	cpu_subtype;	/* subtype of cpu */
                        /*boolean_t*/integer_t	running;	/* is cpu running */
                        	integer_t	cpu_ticks[CPU_STATE_MAX];
                        	integer_t	clock_freq;	/* clock interrupt frequency */
                      };
                       
            We can find an example of initialization in this link: [Example](https://github.com/AlmuHS/GNUMach_SMP/blob/0d490ef21c156907f3f26a6cdc00842f462a877a/i386/i386at/model_dep.c#L612)
      
            This modification also involve the redefinition of `NCPUS`, which must be set to the maximum **possible** number of processors.
            We can do this modifying `configfrag.ac`, with this:
         
          
                 
                 # Multiprocessor support is still broken.
                 AH_TEMPLATE([MULTIPROCESSOR], [set things up for a uniprocessor])
                 mach_ncpus=2
                 AC_DEFINE_UNQUOTED([NCPUS], [$mach_ncpus], [number of CPUs])
                 [if [ $mach_ncpus -gt 1 ]; then]
                   AC_DEFINE([MULTIPROCESSOR], [1], [set things up for a multiprocessor])
                   AC_DEFINE_UNQUOTED([NCPUS], [256], [number of CPUs])
                 [fi]
                 
            Another interesting struct is `machine_info`. This struct save the number of cpus, and other data about the machine
            
                    struct machine_info {                                                               │
                        integer_t       major_version;  /* kernel major version id */               │objdump: supported targets: elf32-i386 elf32-iamcu elf32-little elf32-big plugin sr
                        integer_t       minor_version;  /* kernel minor version id */               │ec symbolsrec verilog tekhex binary ihex
                        integer_t       max_cpus;       /* max number of cpus compiled */           │objdump: supported architectures: i386 i386:x86-64 i386:x64-32 i8086 i386:intel i38
                        integer_t       avail_cpus;     /* number actually available */             │6:x86-64:intel i386:x64-32:intel i386:nacl i386:x86-64:nacl i386:x64-32:nacl iamcu
                        vm_size_t       memory_size;    /* size of memory in bytes */               │iamcu:intel plugin
                    };
      
      - Interesting files and functions
      
          - `machine.c` [Reference](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/kern/machine.c)
          - `c_boot_entry()` [reference](https://github.com/AlmuHS/GNUMach_SMP/blob/0d490ef21c156907f3f26a6cdc00842f462a877a/i386/i386at/model_dep.c#L529)  
          -  Example, in X15 OS: [reference](https://github.com/richardbraun/x15/blob/d6d90a3276a09da65690b019e985392bf77b53b0/arch/x86/machine/cpu.c#L1114)  
          - SMP implementation in OSKit: [reference](http://cvs.savannah.nongnu.org/viewvc/oskit/oskit/smp/x86/)
   
     1.1. Implements `cpu_number()` function: this function must return the kernel ID of the processor which is executing the function.  
       To get this, we have to read the local apic memory space, which will shows the lapic of the current CPU. Reading the lapic, we can get its APIC ID.
       Once got the APIC ID of the current CPU, the function will search in the Kernel/APIC array until find the same APIC ID, and return the index (Kernel ID) of this position.


  2. Implements a routine to initialize the processors: This routine will initialize the lapic of each processor and other structures needed to run the kernel.  
  
     We can find an example of lapic initialization here [reference](https://github.com/mit-pdos/xv6-public/blob/b818915f793cd20c5d1e24f668534a9d690f3cc8/lapic.c#L55)  
     Also, we can to get more information in Chapter 8.4 and 8.11 of Intel Developer Guide, Volume 3. [link](https://software.intel.com/sites/default/files/managed/a4/60/325384-sdm-vol-3abcd.pdf)
     
     
  3. Implements `intel_startCPU()`: This function will initialize the descriptor tables of the processor specified by parameter, and launch the startup IPI to this processor. 
  This function will be executed during the boot of the kernel (process 0).  
      
      The task to do in this function are:
      
        - Initialize the processor descriptor tables  
        - Raise Startup IPI to this processor  
        
      We have a current implementation of `intel_startCPU()` in this [link](https://github.com/AlmuHS/GNUMach_SMP/blob/smp/i386/i386/mp_desc.c). 
      This implementation is based in XNU's `intel_startCPU()` [function](https://github.com/nneonneo/osx-10.9-opensource/blob/f5a0b24e4d98574462c8f5e5dfcf0d37ef7e0764/xnu-2422.1.72/osfmk/i386/mp.c#L423)
  
      We can find explainations about how to raise an IPI in this pages: [*Reference 1*](https://www.cs.usfca.edu/~cruse/cs630f08/lesson22.ppt), 
      [*Reference 2*](https://www.cheesecake.org/sac/smp.html), 
      [*Reference 3*](http://www.dis.uniroma1.it/pub/quaglia/AOSV-traps-interrupts.pdf)
  
      We can get information about how to raise an IPI in Intel Developer Guide, Volume 3, Chapter 10.6
  
     After raise startup IPI, the processor will starts in real mode. To switch to protected mode, we can follow these steps:
     [switch to protected mode](https://stackoverflow.com/questions/36968829/how-to-switch-from-real-mode-to-protected-mode-after-bootloader)
     We can find a little assembly snippet to enter in protected mode [here](https://wiki.osdev.org/Protected_Mode)
      
  
  
  4. Implements another routine to start the processors. This routine calls to `processor_start()` for each processor, who will start the processor using this sequence of calls:  
   [`processor_start(processor_t  processor)`](https://github.com/AlmuHS/GNUMach_SMP/blob/5d527f532dfba9f2da54555d5fbe585dd458579b/kern/processor.c#L447) -> [`cpu_start(processor->slot_num)`](https://github.com/AlmuHS/GNUMach_SMP/blob/5d527f532dfba9f2da54555d5fbe585dd458579b/i386/i386/mp_desc.c#L335) -> [`intel_startCPU(cpu)`](https://github.com/AlmuHS/GNUMach_SMP/blob/5d527f532dfba9f2da54555d5fbe585dd458579b/i386/i386/mp_desc.c#L180)
   
   These articles shows some annotations about how to do the AP Startup:
   [Reference1](https://wiki.osdev.org/Symmetric_Multiprocessing#AP_startup), 
   [Reference2](https://stackoverflow.com/a/16368043/7077301)

 
 
  (...)
  
  After implement IPI support, It's recommended reimplement `machine_idle()`, `machine_relax ()`, `halt_cpu()` and `halt_all_cpus()` using IPI.
   [reference](https://github.com/AlmuHS/GNUMach_SMP/blob/0d490ef21c156907f3f26a6cdc00842f462a877a/i386/i386at/model_dep.c#L201)
  
  Added to this, in `ast_check.c`, we have to implement both functions, using IPI [Reference](https://github.com/AlmuHS/GNUMach_SMP/blob/master/i386/i386/ast_check.c)  
  This functions must force the processors to check if there are any AST signal, and call to attend this          
           
     ```
    [22:28] <AlmuHS> what is the use of AST in gnumach?
    [22:28] <AlmuHS> this file what do? https://github.com/AlmuHS/GNUMach_SMP/blob/master/i386/i386/ast_check.c
    [22:28] <youpi> I don't know
    [22:28] <youpi> but look at what calls that
    [22:29] <youpi> see e.g. the call in thread.c
    [22:29] <AlmuHS> This function is called during the sequence of cpu_up(), in machine.c
    [22:29] <AlmuHS> but only if NCPUS > 1
    [22:29] <youpi> it seems like it's to trigger an AST check on another processor
    [22:30] <youpi> i.e. a processor tells another to run ast_check
    [22:30] <youpi> (see the comment in thread.c)
    [22:30] <AlmuHS> https://github.com/AlmuHS/GNUMach_SMP/blob/master/kern/machine.c
    [22:30] <youpi> well, the initialization part is not necessarily what's important to think about at first
    [22:31] <youpi> i.e. until you know what you'll have to do during execution, you don't know what you'll need to intialize at initialization
    [22:31] <youpi> you might even not need to initialize anything
    [22:32] <AlmuHS> then, this is the reason because all functions in ast_check.c are empty
    [22:33] <youpi> cause_ast_check being empty is really probably a TODO
    [22:33] <AlmuHS> but I'm not clear what I need to write in this functions
    [22:34] <youpi> what the comment said: make another processor run ast_check()
    [22:34] <youpi> which probably means raising an inter-processor interrupt
    [22:34] <youpi> (aka IPI)
    [22:34] <youpi> to get ast_check() called by the other processor
    [22:36] <AlmuHS> then, this funcions must raise an IPI in the processor?
    [22:36] <youpi> that's the idea
    [22:36] <youpi> the IPI probably needs some setup
    ```
  
  We can to use [XV6 source code](https://pdos.csail.mit.edu/6.828/2018/xv6.html). as model to implements the function and routines. Some interesting files are [`lapic.c`](https://github.com/mit-pdos/xv6-public/blob/master/lapic.c), [`proc.c`](https://github.com/mit-pdos/xv6-public/blob/master/proc.c) and [`main.c`](https://github.com/mit-pdos/xv6-public/blob/master/main.c)

  
## References

 - [Comments about the project bug-hurd maillist](https://lists.gnu.org/archive/html/bug-hurd/2018-08/msg00048.html)
 - [Initial thread in bug-hurd maillist](https://lists.gnu.org/archive/html/bug-hurd/2018-06/msg00048.html)
 - [SMP in GNU/Hurd FAQ](https://www.gnu.org/software/hurd/faq/smp.html)
 - [GNU Mach git repository](http://git.savannah.gnu.org/cgit/hurd/gnumach.git)
 - [Old Mach 4 source code](ftp://ftp.lip6.fr/pub/mach/mach4/mach/)
 - [GNU Mach reference manual](https://www.gnu.org/software/hurd/gnumach-doc/)
 - [**GNUMach_SMP repository**](https://github.com/AlmuHS/GNUMach_SMP)
 - [**Debian GNU/Hurd SMP crash**](https://www.youtube.com/watch?v=78JYeXMNeVM)
 - [sthibault FOSDEM slides](https://people.debian.org/~sthibault/hurd-i386/)
 - [FOSDEM speaking: roadmap for the Hurd](http://ftp.belnet.be/mirror/FOSDEM/2019/AW1.121/roadmap_for_the_hurd.webm)
 - [**GENIAL ARTICLE about GNU/Hurd architecture**](https://blog.darknedgy.net/technology/2015/07/25/0/)
 - [**MultiProcessor Specification**](https://pdos.csail.mit.edu/6.828/2011/readings/ia32/MPspec.pdf)
 - [**ACPI Specification**](http://www.uefi.org/sites/default/files/resources/ACPI%206_2_A_Sept29.pdf)
 - [Mach boot trace](https://www.gnu.org/software/hurd/microkernel/mach/gnumach/boot_trace.html)
 - [SPL man page](https://man.openbsd.org/spl)
 - [Book: The Mach System](http://codex.cs.yale.edu/avi/os-book/OS9/appendices-dir/b.pdf)
 - [Book: Mach3 Mysteries](http://www.nv50.0fees.net/Doc/Mach3Mysteries.pdf)
 - [X15 operating system](https://www.sceen.net/x15)
 - [Symmetric Multiprocessing - OSDev Wiki](https://wiki.osdev.org/Symmetric_Multiprocessing)
 - [**Intel® 64 and IA-32 Architectures Software Developer’s Manuals**](https://software.intel.com/sites/default/files/managed/39/c5/325462-sdm-vol-1-2abcd-3abcd.pdf)
 - [**Intel Developer Guide, Volume 3: System Programming Guide**](https://software.intel.com/sites/default/files/managed/a4/60/325384-sdm-vol-3abcd.pdf)