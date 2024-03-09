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

#include <inttypes.h>
#include <string.h>

#include <device/cons.h>

#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>
#include <mach/machine/multiboot.h>
#include <mach/xen.h>

#include <kern/assert.h>
#include <kern/cpu_number.h>
#include <kern/debug.h>
#include <kern/mach_clock.h>
#include <kern/macros.h>
#include <kern/printf.h>
#include <kern/startup.h>
#include <kern/smp.h>
#include <sys/types.h>
#include <vm/vm_page.h>
#include <i386/fpu.h>
#include <i386/gdt.h>
#include <i386/ktss.h>
#include <i386/ldt.h>
#include <i386/spl.h>
#include <i386/mp_desc.h>
#include <i386/pit.h>
#include <i386/pmap.h>
#include <i386/proc_reg.h>
#include <i386/vm_param.h>
#include <i386/locore.h>
#include <i386/model_dep.h>
#include <i386/smp.h>
#include <i386/seg.h>
#include <i386at/acpi_parse_apic.h>
#include <i386at/autoconf.h>
#include <i386at/biosmem.h>
#include <i386at/elf.h>
#include <i386at/idt.h>
#include <i386at/int_init.h>
#include <i386at/kd.h>
#include <i386at/rtc.h>
#include <i386at/model_dep.h>
#include <machine/irq.h>

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

/* ELF section header */
static unsigned elf_shdr_num;
static vm_size_t elf_shdr_size;
static vm_offset_t elf_shdr_addr;
static unsigned elf_shdr_shndx;

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
struct multiboot_raw_info boot_info;
#endif	/* MACH_XEN */

/* Command line supplied to kernel.  */
char *kernel_cmdline = "";

extern char	version[];

/* Realmode temporary GDT */
extern struct pseudo_descriptor gdt_descr_tmp;

/* Realmode relocated jmp */
extern uint32_t apboot_jmp_offset;

/* If set, reboot the system on ctrl-alt-delete.  */
boolean_t	rebootflag = FALSE;	/* exported to kdintr */

#ifdef LINUX_DEV
extern void linux_init(void);
#endif

/*
 * Find devices.  The system is alive.
 */
void machine_init(void)
{
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
#if defined(APIC)
	int err;

	err = acpi_apic_init();
	if (err) {
		printf("acpi_apic_init failed with %d\n", err);
		for (;;);
	}
#endif
#if (NCPUS > 1)
	smp_init();
#endif
#if defined(APIC)
	ioapic_configure();
#endif
	clkstart();

	/*
	 * Initialize the console.
	 */
	cninit();

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

#if NCPUS > 1
	/*
	 * Patch the realmode gdt with the correct offset and the first jmp to
	 * protected mode with the correct target.
	 */
	gdt_descr_tmp.linear_base += apboot_addr;
	apboot_jmp_offset += apboot_addr;

	/*
	 * Initialize the HPET
	 */
	hpet_init();
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
	    printf("Shutdown completed successfully, now in tight loop.\n");
	    printf("You can safely power off the system or hit ctl-alt-del to reboot\n");
	    (void) spl0();
	}
	while (TRUE)
	  machine_idle (cpu_number ());
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

	if (mbi->flags & MULTIBOOT_LOADER_MODULES && mbi->mods_count) {
		i = mbi->mods_count * sizeof(struct multiboot_raw_module);
		biosmem_register_boot_data(mbi->mods_addr, mbi->mods_addr + i, TRUE);

		tmp = phystokv(mbi->mods_addr);

		for (i = 0; i < mbi->mods_count; i++) {
			mod = (struct multiboot_raw_module *)tmp + i;
			if (mod->mod_end != mod->mod_start)
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
		if (tmp != 0)
			biosmem_register_boot_data(mbi->shdr_addr, mbi->shdr_addr + tmp, FALSE);

		tmp = phystokv(mbi->shdr_addr);

		for (i = 0; i < mbi->shdr_num; i++) {
			shdr = (struct elf_shdr *)(tmp + (i * mbi->shdr_size));

			if ((shdr->type != ELF_SHT_SYMTAB)
			    && (shdr->type != ELF_SHT_STRTAB))
				continue;

			if (shdr->size != 0)
				biosmem_register_boot_data(shdr->addr, shdr->addr + shdr->size, FALSE);
		}
	}
}

#endif /* MACH_HYP */

/*
 * Basic PC VM initialization.
 * Turns on paging and changes the kernel segments to use high linear addresses.
 */
static void
i386at_init(void)
{
	/*
	 * Initialize the PIC prior to any possible call to an spl.
	 */
#ifndef	MACH_HYP
# ifdef APIC
	picdisable();
# else
	picinit();
# endif
#else	/* MACH_HYP */
	hyp_intrinit();
#endif	/* MACH_HYP */
	spl_init = 1;

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
	vm_offset_t addr;

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

	if (boot_info.flags & MULTIBOOT_MODS && boot_info.mods_count) {
		struct multiboot_raw_module *m;
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

	pmap_make_temporary_mapping();

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

#ifndef MACH_XEN
	init_percpu(0);
#endif
#if NCPUS > 1
	/* Initialize SMP structures in the master processor */
	mp_desc_init(0);
#endif // NCPUS

	pmap_remove_temporary_mapping();

#ifdef	MACH_XEN
	hyp_p2m_init();
#endif	/* MACH_XEN */

	interrupt_stack_alloc();
}

/*
 *	C boot entrypoint - called by boot_entry in boothdr.S.
 *	Running in flat mode, but without paging yet.
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
	if ((boot_info.flags & MULTIBOOT_ELF_SHDR)
	    && boot_info.shdr_num)
	{
		elf_shdr_num = boot_info.shdr_num;
		elf_shdr_size = boot_info.shdr_size;
		elf_shdr_addr = (vm_offset_t)phystokv(boot_info.shdr_addr);
		elf_shdr_shndx = boot_info.shdr_strndx;

		printf("ELF section header table at %08" PRIxPTR "\n", elf_shdr_addr);
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
	if (elf_shdr_num)
	{
		elf_db_sym_init(elf_shdr_num,elf_shdr_size,
				elf_shdr_addr, elf_shdr_shndx,
				"mach", NULL);
	}
#endif	/* MACH_KDB */

	machine_slot[0].is_cpu = TRUE;
	machine_slot[0].cpu_subtype = CPU_SUBTYPE_AT386;

#if defined(__x86_64__) && !defined(USER32)
	machine_slot[0].cpu_type = CPU_TYPE_X86_64;
#else
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
#endif

	/*
	 * Start the system.
	 */
	setup_main();

}

#include <mach/vm_prot.h>
#include <vm/pmap.h>
#include <mach/time_value.h>

vm_offset_t
timemmap(dev_t dev, vm_offset_t off, vm_prot_t prot)
{
	extern time_value_t *mtime;

	if (prot & VM_PROT_WRITE) return (-1);

	return (i386_btop(pmap_extract(pmap_kernel(), (vm_offset_t) mtime)));
}

void
startrtclock(void)
{
#ifdef APIC
	unmask_irq(timer_pin);
	calibrate_lapic_timer();
	if (cpu_number() != 0) {
		lapic_enable_timer();
	}
#else
	clkstart();
#ifndef MACH_HYP
	unmask_irq(0);
#endif
#endif
}

void
inittodr(void)
{
	time_value64_t	new_time;
	uint64_t	newsecs;

	(void) readtodc(&newsecs);
	new_time.seconds = newsecs;
	new_time.nanoseconds = 0;

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
