/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989, 1988 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	File:	model_dep.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Basic initialization for I386 - ISA bus machines.
 */

#include <string.h>

#include <device/cons.h>

#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>
#include <mach/machine/multiboot.h>
#include <mach/xen.h>

#include <i386/vm_param.h>
#include <kern/assert.h>
#include <kern/cpu_number.h>
#include <kern/debug.h>
#include <kern/mach_clock.h>
#include <kern/printf.h>
#include <sys/time.h>
#include <sys/types.h>
#include <vm/vm_page.h>
#include <i386/fpu.h>
#include <i386/gdt.h>
#include <i386/ktss.h>
#include <i386/ldt.h>
#include <i386/machspl.h>
#include <i386/pic.h>
#include <i386/pit.h>
#include <i386/pmap.h>
#include <i386/proc_reg.h>
#include <i386/locore.h>
#include <i386/model_dep.h>
#include <i386at/autoconf.h>
#include <i386at/idt.h>
#include <i386at/int_init.h>
#include <i386at/kd.h>
#include <i386at/rtc.h>
#ifdef	MACH_XEN
#include <xen/console.h>
#include <xen/store.h>
#include <xen/evt.h>
#include <xen/xen.h>
#endif	/* MACH_XEN */

/* Location of the kernel's symbol table.
   Both of these are 0 if none is available.  */
#if MACH_KDB
static vm_offset_t kern_sym_start, kern_sym_end;
#else
#define kern_sym_start	0
#define kern_sym_end	0
#endif

/* These indicate the total extent of physical memory addresses we're using.
   They are page-aligned.  */
vm_offset_t phys_first_addr = 0;
vm_offset_t phys_last_addr;

/* A copy of the multiboot info structure passed by the boot loader.  */
#ifdef MACH_XEN
struct start_info boot_info;
#ifdef MACH_PSEUDO_PHYS
unsigned long *mfn_list;
#if VM_MIN_KERNEL_ADDRESS != LINEAR_MIN_KERNEL_ADDRESS
unsigned long *pfn_list = (void*) PFN_LIST;
#endif
#endif	/* MACH_PSEUDO_PHYS */
#if VM_MIN_KERNEL_ADDRESS != LINEAR_MIN_KERNEL_ADDRESS
unsigned long la_shift = VM_MIN_KERNEL_ADDRESS;
#endif
#else	/* MACH_XEN */
struct multiboot_info boot_info;
#endif	/* MACH_XEN */

/* Command line supplied to kernel.  */
char *kernel_cmdline = "";

/* This is used for memory initialization:
   it gets bumped up through physical memory
   that exists and is not occupied by boot gunk.
   It is not necessarily page-aligned.  */
static vm_offset_t avail_next
#ifndef MACH_HYP
	= 0x1000 /* XX end of BIOS data area */
#endif	/* MACH_HYP */
	;

/* Possibly overestimated amount of available memory
   still remaining to be handed to the VM system.  */
static vm_size_t avail_remaining;

extern char	version[];

extern void	setup_main();

void		halt_all_cpus (boolean_t reboot) __attribute__ ((noreturn));
void		halt_cpu (void) __attribute__ ((noreturn));

int		discover_x86_cpu_type (void);

void		inittodr();	/* forward */

int		rebootflag = 0;	/* exported to kdintr */

/* XX interrupt stack pointer and highwater mark, for locore.S.  */
vm_offset_t int_stack_top, int_stack_high;

#ifdef LINUX_DEV
extern void linux_init(void);
#endif

boolean_t init_alloc_aligned(vm_size_t size, vm_offset_t *addrp);

/*
 * Find devices.  The system is alive.
 */
void machine_init(void)
{
	/*
	 * Initialize the console.
	 */
	cninit();

	/*
	 * Set up to use floating point.
	 */
	init_fpu();

#ifdef MACH_HYP
	hyp_init();
#else	/* MACH_HYP */
#ifdef LINUX_DEV
	/*
	 * Initialize Linux drivers.
	 */
	linux_init();
#endif

	/*
	 * Find the devices
	 */
	probeio();
#endif	/* MACH_HYP */

	/*
	 * Get the time
	 */
	inittodr();

#ifndef MACH_HYP
	/*
	 * Tell the BIOS not to clear and test memory.
	 */
	*(unsigned short *)phystokv(0x472) = 0x1234;
#endif	/* MACH_HYP */

	/*
	 * Unmap page 0 to trap NULL references.
	 */
	pmap_unmap_page_zero();
}

/* Conserve power on processor CPU.  */
void machine_idle (int cpu)
{
#ifdef	MACH_HYP
  hyp_idle();
#else	/* MACH_HYP */
  assert (cpu == cpu_number ());
  asm volatile ("hlt" : : : "memory");
#endif	/* MACH_HYP */
}

void machine_relax ()
{
	asm volatile ("rep; nop" : : : "memory");
}

/*
 * Halt a cpu.
 */
void halt_cpu(void)
{
#ifdef	MACH_HYP
	hyp_halt();
#else	/* MACH_HYP */
	asm volatile("cli");
	while (TRUE)
	  machine_idle (cpu_number ());
#endif	/* MACH_HYP */
}

/*
 * Halt the system or reboot.
 */
void halt_all_cpus(reboot)
	boolean_t	reboot;
{
	if (reboot) {
#ifdef	MACH_HYP
	    hyp_reboot();
#endif	/* MACH_HYP */
	    kdreboot();
	}
	else {
	    rebootflag = 1;
#ifdef	MACH_HYP
	    hyp_halt();
#endif	/* MACH_HYP */
	    printf("In tight loop: hit ctl-alt-del to reboot\n");
	    (void) spl0();
	}
	while (TRUE)
	  machine_idle (cpu_number ());
}

void exit(int rc)
{
	halt_all_cpus(0);
}

void db_reset_cpu(void)
{
	halt_all_cpus(1);
}


/*
 * Compute physical memory size and other parameters.
 */
void
mem_size_init(void)
{
	/* Physical memory on all PCs starts at physical address 0.
	   XX make it a constant.  */
	phys_first_addr = 0;

#ifdef MACH_HYP
	if (boot_info.nr_pages >= 0x100000) {
		printf("Truncating memory size to 4GiB\n");
		phys_last_addr = 0xffffffffU;
	} else
		phys_last_addr = boot_info.nr_pages * 0x1000;
#else	/* MACH_HYP */
	/* TODO: support mmap */
	vm_size_t phys_last_kb = 0x400 + boot_info.mem_upper;
	/* Avoid 4GiB overflow.  */
	if (phys_last_kb < 0x400 || phys_last_kb >= 0x400000) {
		printf("Truncating memory size to 4GiB\n");
		phys_last_addr = 0xffffffffU;
	} else
		phys_last_addr = phys_last_kb * 0x400;
#endif	/* MACH_HYP */

	printf("AT386 boot: physical memory from 0x%x to 0x%x\n",
	       phys_first_addr, phys_last_addr);

	/* Reserve 1/6 of the memory address space for virtual mappings.
	 * Yes, this loses memory.  Blame i386.  */
	if (phys_last_addr > ((VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / 6) * 5) {
		phys_last_addr = ((VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / 6) * 5;
		printf("Truncating memory size to %dMiB\n", (phys_last_addr - phys_first_addr) / (1024 * 1024));
		/* TODO Xen: free lost memory */
	}

	phys_first_addr = round_page(phys_first_addr);
	phys_last_addr = trunc_page(phys_last_addr);

#ifdef MACH_HYP
	/* Memory is just contiguous */
	avail_remaining = phys_last_addr;
#else	/* MACH_HYP */
	avail_remaining
	  = phys_last_addr - (0x100000 - (boot_info.mem_lower * 0x400)
			      - 0x1000);
#endif	/* MACH_HYP */
}

/*
 * Basic PC VM initialization.
 * Turns on paging and changes the kernel segments to use high linear addresses.
 */
void
i386at_init(void)
{
	/* XXX move to intel/pmap.h */
	extern pt_entry_t *kernel_page_dir;
	int nb_direct, i;
	vm_offset_t addr;

	/*
	 * Initialize the PIC prior to any possible call to an spl.
	 */
#ifndef	MACH_HYP
	picinit();
#else	/* MACH_HYP */
	hyp_intrinit();
#endif	/* MACH_HYP */

	/*
	 * Find memory size parameters.
	 */
	mem_size_init();

#ifdef MACH_XEN
	kernel_cmdline = (char*) boot_info.cmd_line;
#else	/* MACH_XEN */
	/* Copy content pointed by boot_info before losing access to it when it
	 * is too far in physical memory.  */
	if (boot_info.flags & MULTIBOOT_CMDLINE) {
		int len = strlen ((char*)phystokv(boot_info.cmdline)) + 1;
		assert(init_alloc_aligned(round_page(len), &addr));
		kernel_cmdline = (char*) phystokv(addr);
		memcpy(kernel_cmdline, (char*)phystokv(boot_info.cmdline), len);
		boot_info.cmdline = addr;
	}

	if (boot_info.flags & MULTIBOOT_MODS) {
		struct multiboot_module *m;
		int i;

		assert(init_alloc_aligned(round_page(boot_info.mods_count * sizeof(*m)), &addr));
		m = (void*) phystokv(addr);
		memcpy(m, (void*) phystokv(boot_info.mods_addr), boot_info.mods_count * sizeof(*m));
		boot_info.mods_addr = addr;

		for (i = 0; i < boot_info.mods_count; i++) {
			vm_size_t size = m[i].mod_end - m[i].mod_start;
			assert(init_alloc_aligned(round_page(size), &addr));
			memcpy((void*) phystokv(addr), (void*) phystokv(m[i].mod_start), size);
			m[i].mod_start = addr;
			m[i].mod_end = addr + size;

			size = strlen((char*) phystokv(m[i].string)) + 1;
			assert(init_alloc_aligned(round_page(size), &addr));
			memcpy((void*) phystokv(addr), (void*) phystokv(m[i].string), size);
			m[i].string = addr;
		}
	}
#endif	/* MACH_XEN */

	/*
	 *	Initialize kernel physical map, mapping the
	 *	region from loadpt to avail_start.
	 *	Kernel virtual address starts at VM_KERNEL_MIN_ADDRESS.
	 *	XXX make the BIOS page (page 0) read-only.
	 */
	pmap_bootstrap();

	/*
	 * Turn paging on.
	 * We'll have to temporarily install a direct mapping
	 * between physical memory and low linear memory,
	 * until we start using our new kernel segment descriptors.
	 * Also, set the WP bit so that on 486 or better processors
	 * page-level write protection works in kernel mode.
	 */
	init_alloc_aligned(0, &addr);
	nb_direct = (addr + NPTES * PAGE_SIZE - 1) / (NPTES * PAGE_SIZE);
	for (i = 0; i < nb_direct; i++)
		kernel_page_dir[lin2pdenum(VM_MIN_KERNEL_ADDRESS) + i] =
			kernel_page_dir[lin2pdenum(LINEAR_MIN_KERNEL_ADDRESS) + i];

#ifdef	MACH_XEN
	{
		int i;
		for (i = 0; i < PDPNUM; i++)
			pmap_set_page_readonly_init((void*) kernel_page_dir + i * INTEL_PGBYTES);
#if PAE
		pmap_set_page_readonly_init(kernel_pmap->pdpbase);
#endif	/* PAE */
	}
#endif	/* MACH_XEN */
#if PAE
	set_cr3((unsigned)_kvtophys(kernel_pmap->pdpbase));
#ifndef	MACH_HYP
	if (!CPU_HAS_FEATURE(CPU_FEATURE_PAE))
		panic("CPU doesn't have support for PAE.");
	set_cr4(get_cr4() | CR4_PAE);
#endif	/* MACH_HYP */
#else
	set_cr3((unsigned)_kvtophys(kernel_page_dir));
#endif	/* PAE */
#ifndef	MACH_HYP
	if (CPU_HAS_FEATURE(CPU_FEATURE_PGE))
		set_cr4(get_cr4() | CR4_PGE);
	/* already set by Hypervisor */
	set_cr0(get_cr0() | CR0_PG | CR0_WP);
#endif	/* MACH_HYP */
	flush_instr_queue();
#ifdef	MACH_XEN
	pmap_clear_bootstrap_pagetable((void *)boot_info.pt_base);
#endif	/* MACH_XEN */

	/* Interrupt stacks are allocated in physical memory,
	   while kernel stacks are allocated in kernel virtual memory,
	   so phys_last_addr serves as a convenient dividing point.  */
	int_stack_high = phystokv(phys_last_addr);

	/*
	 * Initialize and activate the real i386 protected-mode structures.
	 */
	gdt_init();
	idt_init();
#ifndef	MACH_HYP
	int_init();
#endif	/* MACH_HYP */
	ldt_init();
	ktss_init();

	/* Get rid of the temporary direct mapping and flush it out of the TLB.  */
	for (i = 0 ; i < nb_direct; i++) {
#ifdef	MACH_XEN
#ifdef	MACH_PSEUDO_PHYS
		if (!hyp_mmu_update_pte(kv_to_ma(&kernel_page_dir[lin2pdenum(VM_MIN_KERNEL_ADDRESS) + i]), 0))
#else	/* MACH_PSEUDO_PHYS */
		if (hyp_do_update_va_mapping(VM_MIN_KERNEL_ADDRESS + i * INTEL_PGBYTES, 0, UVMF_INVLPG | UVMF_ALL))
#endif	/* MACH_PSEUDO_PHYS */
			printf("couldn't unmap frame %d\n", i);
#else	/* MACH_XEN */
		kernel_page_dir[lin2pdenum(VM_MIN_KERNEL_ADDRESS) + i] = 0;
#endif	/* MACH_XEN */
	}

	/* Not used after boot, better give it back.  */
#ifdef	MACH_XEN
	hyp_free_page(0, (void*) VM_MIN_KERNEL_ADDRESS);
#endif	/* MACH_XEN */

	flush_tlb();

#ifdef	MACH_XEN
	hyp_p2m_init();
#endif	/* MACH_XEN */

	/* XXX We'll just use the initialization stack we're already running on
	   as the interrupt stack for now.  Later this will have to change,
	   because the init stack will get freed after bootup.  */
	asm("movl %%esp,%0" : "=m" (int_stack_top));
}

/*
 *	C boot entrypoint - called by boot_entry in boothdr.S.
 *	Running in 32-bit flat mode, but without paging yet.
 */
void c_boot_entry(vm_offset_t bi)
{
	/* Stash the boot_image_info pointer.  */
	boot_info = *(typeof(boot_info)*)phystokv(bi);
	int cpu_type;

	/* Before we do _anything_ else, print the hello message.
	   If there are no initialized console devices yet,
	   it will be stored and printed at the first opportunity.  */
	printf(version);
	printf("\n");

#ifdef MACH_XEN
	printf("Running on %s.\n", boot_info.magic);
	if (boot_info.flags & SIF_PRIVILEGED)
		panic("Mach can't run as dom0.");
#ifdef MACH_PSEUDO_PHYS
	mfn_list = (void*)boot_info.mfn_list;
#endif
#else	/* MACH_XEN */

#if	MACH_KDB
	/*
	 * Locate the kernel's symbol table, if the boot loader provided it.
	 * We need to do this before i386at_init()
	 * so that the symbol table's memory won't be stomped on.
	 */
	if ((boot_info.flags & MULTIBOOT_AOUT_SYMS)
	    && boot_info.syms.a.addr)
	{
		vm_size_t symtab_size, strtab_size;

		kern_sym_start = (vm_offset_t)phystokv(boot_info.syms.a.addr);
		symtab_size = (vm_offset_t)phystokv(boot_info.syms.a.tabsize);
		strtab_size = (vm_offset_t)phystokv(boot_info.syms.a.strsize);
		kern_sym_end = kern_sym_start + 4 + symtab_size + strtab_size;

		printf("kernel symbol table at %08x-%08x (%d,%d)\n",
		       kern_sym_start, kern_sym_end,
		       symtab_size, strtab_size);
	}
#endif	/* MACH_KDB */
#endif	/* MACH_XEN */

	cpu_type = discover_x86_cpu_type ();

	/*
	 * Do basic VM initialization
	 */
	i386at_init();

#if	MACH_KDB
	/*
	 * Initialize the kernel debugger's kernel symbol table.
	 */
	if (kern_sym_start)
	{
		aout_db_sym_init(kern_sym_start, kern_sym_end, "mach", 0);
	}
#endif	/* MACH_KDB */

	machine_slot[0].is_cpu = TRUE;
	machine_slot[0].running = TRUE;
	machine_slot[0].cpu_subtype = CPU_SUBTYPE_AT386;

	switch (cpu_type)
	  {
	  default:
	    printf("warning: unknown cpu type %d, assuming i386\n", cpu_type);
	  case 3:
	    machine_slot[0].cpu_type = CPU_TYPE_I386;
	    break;
	  case 4:
	    machine_slot[0].cpu_type = CPU_TYPE_I486;
	    break;
	  case 5:
	    machine_slot[0].cpu_type = CPU_TYPE_PENTIUM;
	    break;
	  case 6:
	  case 15:
	    machine_slot[0].cpu_type = CPU_TYPE_PENTIUMPRO;
	    break;
	  }

	/*
	 * Start the system.
	 */
	setup_main();

}

#include <mach/vm_prot.h>
#include <vm/pmap.h>
#include <mach/time_value.h>

int
timemmap(dev,off,prot)
	vm_prot_t prot;
{
	extern time_value_t *mtime;

#ifdef	lint
	dev++; off++;
#endif	/* lint */

	if (prot & VM_PROT_WRITE) return (-1);

	return (i386_btop(pmap_extract(pmap_kernel(), (vm_offset_t) mtime)));
}

void
startrtclock(void)
{
	clkstart();
}

void
inittodr(void)
{
	time_value_t	new_time;

	new_time.seconds = 0;
	new_time.microseconds = 0;

	(void) readtodc((u_int *)&new_time.seconds);

	{
	    spl_t	s = splhigh();
	    time = new_time;
	    splx(s);
	}
}

void
resettodr(void)
{
	writetodc();
}

unsigned int pmap_free_pages(void)
{
	return atop(avail_remaining);
}

/* Always returns page-aligned regions.  */
boolean_t
init_alloc_aligned(vm_size_t size, vm_offset_t *addrp)
{
	vm_offset_t addr;

#ifdef MACH_HYP
	/* There is none */
	if (!avail_next)
		avail_next = _kvtophys(boot_info.pt_base) + (boot_info.nr_pt_frames + 3) * 0x1000;
#else	/* MACH_HYP */
	extern char start[], end[];
	int i;
	static int wrapped = 0;

	/* Memory regions to skip.  */
	vm_offset_t cmdline_start_pa = boot_info.flags & MULTIBOOT_CMDLINE
		? boot_info.cmdline : 0;
	vm_offset_t cmdline_end_pa = cmdline_start_pa
		? cmdline_start_pa+strlen((char*)phystokv(cmdline_start_pa))+1
		: 0;
	vm_offset_t mods_start_pa = boot_info.flags & MULTIBOOT_MODS
		? boot_info.mods_addr : 0;
	vm_offset_t mods_end_pa = mods_start_pa
		? mods_start_pa
		  + boot_info.mods_count * sizeof(struct multiboot_module)
		: 0;

	retry:
#endif	/* MACH_HYP */

	/* Page-align the start address.  */
	avail_next = round_page(avail_next);

#ifndef MACH_HYP
	/* Start with memory above 16MB, reserving the low memory for later. */
	/* Don't care on Xen */
	if (!wrapped && phys_last_addr > 16 * 1024*1024)
	  {
	    if (avail_next < 16 * 1024*1024)
	      avail_next = 16 * 1024*1024;
	    else if (avail_next == phys_last_addr)
	      {
		/* We have used all the memory above 16MB, so now start on
		   the low memory.  This will wind up at the end of the list
		   of free pages, so it should not have been allocated to any
		   other use in early initialization before the Linux driver
		   glue initialization needs to allocate low memory.  */
		avail_next = 0x1000;
		wrapped = 1;
	      }
	  }
#endif	/* MACH_HYP */

	/* Check if we have reached the end of memory.  */
        if (avail_next == 
		(
#ifndef MACH_HYP
		wrapped ? 16 * 1024*1024 : 
#endif	/* MACH_HYP */
		phys_last_addr))
		return FALSE;

	/* Tentatively assign the current location to the caller.  */
	addr = avail_next;

	/* Bump the pointer past the newly allocated region
	   and see where that puts us.  */
	avail_next += size;

#ifndef MACH_HYP
	/* Skip past the I/O and ROM area.  */
	if ((avail_next > (boot_info.mem_lower * 0x400)) && (addr < 0x100000))
	{
		avail_next = 0x100000;
		goto retry;
	}

	/* Skip our own kernel code, data, and bss.  */
	if ((avail_next > (vm_offset_t)start) && (addr < (vm_offset_t)end))
	{
		avail_next = (vm_offset_t)end;
		goto retry;
	}

	/* Skip any areas occupied by valuable boot_info data.  */
	if ((avail_next > cmdline_start_pa) && (addr < cmdline_end_pa))
	{
		avail_next = cmdline_end_pa;
		goto retry;
	}
	if ((avail_next > mods_start_pa) && (addr < mods_end_pa))
	{
		avail_next = mods_end_pa;
		goto retry;
	}
	if ((avail_next > kern_sym_start) && (addr < kern_sym_end))
	{
		avail_next = kern_sym_end;
		goto retry;
	}
	if (boot_info.flags & MULTIBOOT_MODS)
	{
		struct multiboot_module *m = (struct multiboot_module *)
			phystokv(boot_info.mods_addr);
		for (i = 0; i < boot_info.mods_count; i++)
		{
			if ((avail_next > m[i].mod_start)
			    && (addr < m[i].mod_end))
			{
				avail_next = m[i].mod_end;
				goto retry;
			}
			/* XXX string */
		}
	}
#endif	/* MACH_HYP */

	avail_remaining -= size;

	*addrp = addr;
	return TRUE;
}

boolean_t pmap_next_page(addrp)
	vm_offset_t *addrp;
{
	return init_alloc_aligned(PAGE_SIZE, addrp);
}

/* Grab a physical page:
   the standard memory allocation mechanism
   during system initialization.  */
vm_offset_t
pmap_grab_page(void)
{
	vm_offset_t addr;
	if (!pmap_next_page(&addr))
		panic("Not enough memory to initialize Mach");
	return addr;
}

boolean_t pmap_valid_page(x)
	vm_offset_t x;
{
	/* XXX is this OK?  What does it matter for?  */
	return (((phys_first_addr <= x) && (x < phys_last_addr))
#ifndef MACH_HYP
		&& !(
		((boot_info.mem_lower * 1024) <= x) && 
		(x < 1024*1024))
#endif	/* MACH_HYP */
		);
}
