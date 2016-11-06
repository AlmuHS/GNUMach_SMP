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
#include <kern/macros.h>
#include <kern/printf.h>
#include <kern/startup.h>
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
#include <i386at/biosmem.h>
#include <i386at/elf.h>
#include <i386at/idt.h>
#include <i386at/int_init.h>
#include <i386at/kd.h>
#include <i386at/rtc.h>
#include <i386at/model_dep.h>
#ifdef	MACH_XEN
#include <xen/console.h>
#include <xen/store.h>
#include <xen/evt.h>
#include <xen/xen.h>
#endif	/* MACH_XEN */

#if	ENABLE_IMMEDIATE_CONSOLE
#include "immc.h"
#endif	/* ENABLE_IMMEDIATE_CONSOLE */

/* Location of the kernel's symbol table.
   Both of these are 0 if none is available.  */
#if MACH_KDB
#include <ddb/db_sym.h>
#include <i386/db_interface.h>

/* a.out symbol table */
static vm_offset_t kern_sym_start, kern_sym_end;

/* ELF section header */
static unsigned elf_shdr_num;
static vm_size_t elf_shdr_size;
static vm_offset_t elf_shdr_addr;
static unsigned elf_shdr_shndx;

#else /* MACH_KDB */
#define kern_sym_start	0
#define kern_sym_end	0
#endif /* MACH_KDB */

#define RESERVED_BIOS 0x10000

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

extern char	version[];

/* If set, reboot the system on ctrl-alt-delete.  */
boolean_t	rebootflag = FALSE;	/* exported to kdintr */

/* Interrupt stack.  */
static char int_stack[KERNEL_STACK_SIZE] __aligned(KERNEL_STACK_SIZE);
vm_offset_t int_stack_top, int_stack_base;

#ifdef LINUX_DEV
extern void linux_init(void);
#endif

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
	 * Make more free memory.
	 *
	 * This is particularly important for the Linux drivers which
	 * require available DMA memory.
	 */
	biosmem_free_usable();

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

#if VM_MIN_KERNEL_ADDRESS == 0
	/*
	 * Unmap page 0 to trap NULL references.
	 *
	 * Note that this breaks accessing some BIOS areas stored there.
	 */
	pmap_unmap_page_zero();
#endif
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

void machine_relax (void)
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
void halt_all_cpus(boolean_t reboot)
{
	if (reboot) {
#ifdef	MACH_HYP
	    hyp_reboot();
#endif	/* MACH_HYP */
	    kdreboot();
	}
	else {
	    rebootflag = TRUE;
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

void db_halt_cpu(void)
{
	halt_all_cpus(0);
}

void db_reset_cpu(void)
{
	halt_all_cpus(1);
}

#ifndef	MACH_HYP

static void
register_boot_data(const struct multiboot_raw_info *mbi)
{
	struct multiboot_raw_module *mod;
	struct elf_shdr *shdr;
	unsigned long tmp;
	unsigned int i;

	extern char _start[], _end[];

	biosmem_register_boot_data(_kvtophys(&_start), _kvtophys(&_end), FALSE);

	/* cmdline and modules are moved to a safe place by i386at_init.  */

	if ((mbi->flags & MULTIBOOT_LOADER_CMDLINE) && (mbi->cmdline != 0)) {
		biosmem_register_boot_data(mbi->cmdline,
					   mbi->cmdline
					   + strlen((void *)phystokv(mbi->cmdline)) + 1, TRUE);
	}

	if (mbi->flags & MULTIBOOT_LOADER_MODULES) {
		i = mbi->mods_count * sizeof(struct multiboot_raw_module);
		biosmem_register_boot_data(mbi->mods_addr, mbi->mods_addr + i, TRUE);

		tmp = phystokv(mbi->mods_addr);

		for (i = 0; i < mbi->mods_count; i++) {
			mod = (struct multiboot_raw_module *)tmp + i;
			biosmem_register_boot_data(mod->mod_start, mod->mod_end, TRUE);

			if (mod->string != 0) {
				biosmem_register_boot_data(mod->string,
							   mod->string
							   + strlen((void *)phystokv(mod->string)) + 1,
							   TRUE);
			}
		}
	}

	if (mbi->flags & MULTIBOOT_LOADER_SHDR) {
		tmp = mbi->shdr_num * mbi->shdr_size;
		biosmem_register_boot_data(mbi->shdr_addr, mbi->shdr_addr + tmp, FALSE);

		tmp = phystokv(mbi->shdr_addr);

		for (i = 0; i < mbi->shdr_num; i++) {
			shdr = (struct elf_shdr *)(tmp + (i * mbi->shdr_size));

			if ((shdr->type != ELF_SHT_SYMTAB)
			    && (shdr->type != ELF_SHT_STRTAB))
				continue;

			biosmem_register_boot_data(shdr->addr, shdr->addr + shdr->size, FALSE);
		}
	}
}

#endif /* MACH_HYP */

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
	vm_offset_t addr, delta;

	/*
	 * Initialize the PIC prior to any possible call to an spl.
	 */
#ifndef	MACH_HYP
	picinit();
#else	/* MACH_HYP */
	hyp_intrinit();
#endif	/* MACH_HYP */

	/*
	 * Read memory map and load it into the physical page allocator.
	 */
#ifdef MACH_HYP
	biosmem_xen_bootstrap();
#else /* MACH_HYP */
	register_boot_data((struct multiboot_raw_info *) &boot_info);
	biosmem_bootstrap((struct multiboot_raw_info *) &boot_info);
#endif /* MACH_HYP */

#ifdef MACH_XEN
	kernel_cmdline = (char*) boot_info.cmd_line;
#else	/* MACH_XEN */
	/* Copy content pointed by boot_info before losing access to it when it
	 * is too far in physical memory.
	 * Also avoids leaving them in precious areas such as DMA memory.  */
	if (boot_info.flags & MULTIBOOT_CMDLINE) {
		int len = strlen ((char*)phystokv(boot_info.cmdline)) + 1;
		if (! init_alloc_aligned(round_page(len), &addr))
		  panic("could not allocate memory for multiboot command line");
		kernel_cmdline = (char*) phystokv(addr);
		memcpy(kernel_cmdline, (void *)phystokv(boot_info.cmdline), len);
		boot_info.cmdline = addr;
	}

	if (boot_info.flags & MULTIBOOT_MODS) {
		struct multiboot_module *m;
		int i;

		if (! init_alloc_aligned(
			round_page(boot_info.mods_count * sizeof(*m)), &addr))
		  panic("could not allocate memory for multiboot modules");
		m = (void*) phystokv(addr);
		memcpy(m, (void*) phystokv(boot_info.mods_addr), boot_info.mods_count * sizeof(*m));
		boot_info.mods_addr = addr;

		for (i = 0; i < boot_info.mods_count; i++) {
			vm_size_t size = m[i].mod_end - m[i].mod_start;
			if (! init_alloc_aligned(round_page(size), &addr))
			  panic("could not allocate memory for multiboot "
				"module %d", i);
			memcpy((void*) phystokv(addr), (void*) phystokv(m[i].mod_start), size);
			m[i].mod_start = addr;
			m[i].mod_end = addr + size;

			size = strlen((char*) phystokv(m[i].string)) + 1;
			if (! init_alloc_aligned(round_page(size), &addr))
			  panic("could not allocate memory for multiboot "
				"module command line %d", i);
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
	 *	Load physical segments into the VM system.
	 *	The early allocation functions become unusable after
	 *	this point.
	 */
	biosmem_setup();

	/*
	 * We'll have to temporarily install a direct mapping
	 * between physical memory and low linear memory,
	 * until we start using our new kernel segment descriptors.
	 */
#if INIT_VM_MIN_KERNEL_ADDRESS != LINEAR_MIN_KERNEL_ADDRESS
	delta = INIT_VM_MIN_KERNEL_ADDRESS - LINEAR_MIN_KERNEL_ADDRESS;
	if ((vm_offset_t)(-delta) < delta)
		delta = (vm_offset_t)(-delta);
	nb_direct = delta >> PDESHIFT;
	for (i = 0; i < nb_direct; i++)
		kernel_page_dir[lin2pdenum_cont(INIT_VM_MIN_KERNEL_ADDRESS) + i] =
			kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS) + i];
#endif
	/* We need BIOS memory mapped at 0xc0000 & co for Linux drivers */
#ifdef LINUX_DEV
#if VM_MIN_KERNEL_ADDRESS != 0
	kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)] =
		kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS)];
#endif
#endif

#ifdef	MACH_PV_PAGETABLES
	for (i = 0; i < PDPNUM; i++)
		pmap_set_page_readonly_init((void*) kernel_page_dir + i * INTEL_PGBYTES);
#if PAE
	pmap_set_page_readonly_init(kernel_pmap->pdpbase);
#endif	/* PAE */
#endif	/* MACH_PV_PAGETABLES */
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
	/* Turn paging on.
	 * Also set the WP bit so that on 486 or better processors
	 * page-level write protection works in kernel mode.
	 */
	set_cr0(get_cr0() | CR0_PG | CR0_WP);
	set_cr0(get_cr0() & ~(CR0_CD | CR0_NW));
	if (CPU_HAS_FEATURE(CPU_FEATURE_PGE))
		set_cr4(get_cr4() | CR4_PGE);
#endif	/* MACH_HYP */
	flush_instr_queue();
#ifdef	MACH_PV_PAGETABLES
	pmap_clear_bootstrap_pagetable((void *)boot_info.pt_base);
#endif	/* MACH_PV_PAGETABLES */

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

#if INIT_VM_MIN_KERNEL_ADDRESS != LINEAR_MIN_KERNEL_ADDRESS
	/* Get rid of the temporary direct mapping and flush it out of the TLB.  */
	for (i = 0 ; i < nb_direct; i++) {
#ifdef	MACH_XEN
#ifdef	MACH_PSEUDO_PHYS
		if (!hyp_mmu_update_pte(kv_to_ma(&kernel_page_dir[lin2pdenum_cont(VM_MIN_KERNEL_ADDRESS) + i]), 0))
#else	/* MACH_PSEUDO_PHYS */
		if (hyp_do_update_va_mapping(VM_MIN_KERNEL_ADDRESS + i * INTEL_PGBYTES, 0, UVMF_INVLPG | UVMF_ALL))
#endif	/* MACH_PSEUDO_PHYS */
			printf("couldn't unmap frame %d\n", i);
#else	/* MACH_XEN */
		kernel_page_dir[lin2pdenum_cont(INIT_VM_MIN_KERNEL_ADDRESS) + i] = 0;
#endif	/* MACH_XEN */
	}
#endif
	/* Keep BIOS memory mapped */
#ifdef LINUX_DEV
#if VM_MIN_KERNEL_ADDRESS != 0
	kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)] =
		kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS)];
#endif
#endif

	/* Not used after boot, better give it back.  */
#ifdef	MACH_XEN
	hyp_free_page(0, (void*) VM_MIN_KERNEL_ADDRESS);
#endif	/* MACH_XEN */

	flush_tlb();

#ifdef	MACH_XEN
	hyp_p2m_init();
#endif	/* MACH_XEN */

	int_stack_base = (vm_offset_t)&int_stack;
	int_stack_top = int_stack_base + KERNEL_STACK_SIZE - 4;
}

/*
 *	C boot entrypoint - called by boot_entry in boothdr.S.
 *	Running in 32-bit flat mode, but without paging yet.
 */
void c_boot_entry(vm_offset_t bi)
{
#if	ENABLE_IMMEDIATE_CONSOLE
	romputc = immc_romputc;
#endif	/* ENABLE_IMMEDIATE_CONSOLE */

	/* Stash the boot_image_info pointer.  */
	boot_info = *(typeof(boot_info)*)phystokv(bi);
	int cpu_type;

	/* Before we do _anything_ else, print the hello message.
	   If there are no initialized console devices yet,
	   it will be stored and printed at the first opportunity.  */
	printf("%s", version);
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

		printf("kernel symbol table at %08lx-%08lx (%d,%d)\n",
		       kern_sym_start, kern_sym_end,
		       symtab_size, strtab_size);
	}

	if ((boot_info.flags & MULTIBOOT_ELF_SHDR)
	    && boot_info.syms.e.num)
	{
		elf_shdr_num = boot_info.syms.e.num;
		elf_shdr_size = boot_info.syms.e.size;
		elf_shdr_addr = (vm_offset_t)phystokv(boot_info.syms.e.addr);
		elf_shdr_shndx = boot_info.syms.e.shndx;

		printf("ELF section header table at %08lx\n", elf_shdr_addr);
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
		aout_db_sym_init((char *)kern_sym_start, (char *)kern_sym_end, "mach", (char *)0);
	}

	if (elf_shdr_num)
	{
		elf_db_sym_init(elf_shdr_num,elf_shdr_size,
				elf_shdr_addr, elf_shdr_shndx,
				"mach", NULL);
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
timemmap(dev, off, prot)
	dev_t dev;
	vm_offset_t off;
	vm_prot_t prot;
{
	extern time_value_t *mtime;

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

boolean_t
init_alloc_aligned(vm_size_t size, vm_offset_t *addrp)
{
	*addrp = biosmem_bootalloc(vm_page_atop(vm_page_round(size)));

	if (*addrp == 0)
		return FALSE;

	return TRUE;
}

/* Grab a physical page:
   the standard memory allocation mechanism
   during system initialization.  */
vm_offset_t
pmap_grab_page(void)
{
	vm_offset_t addr;
	if (!init_alloc_aligned(PAGE_SIZE, &addr))
		panic("Not enough memory to initialize Mach");
	return addr;
}
