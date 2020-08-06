/*
 * Linux initialization.
 *
 * Copyright (C) 1996 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *      Author: Shantanu Goel, University of Utah CSL
 */

/*
 *  linux/init/main.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <sys/types.h>

#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>

#include <vm/vm_page.h>
#include <kern/kalloc.h>

#include <machine/spl.h>
#include <machine/pmap.h>
#include <machine/vm_param.h>
#include <machine/model_dep.h>

#define MACH_INCLUDE
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/dev/glue/glue.h>

#include <asm/system.h>
#include <asm/io.h>

/*
 * Timing loop count.
 */
unsigned long loops_per_sec = 1;

#if defined(__SMP__) && defined(__i386__)
unsigned long smp_loops_per_tick = 1000000;
#endif

/*
 * End of physical memory.
 */
unsigned long high_memory;

/*
 * Flag to indicate auto-configuration is in progress.
 */
int linux_auto_config = 1;

/*
 * Hard drive parameters obtained from the BIOS.
 */
struct drive_info_struct
{
  char dummy[32];
} drive_info;

/*
 * Forward declarations.
 */
static void calibrate_delay (void);

/*
 * Amount of contiguous memory to allocate for initialization.
 */
#define CONTIG_ALLOC (512 * 1024)

/*
 * Initialize Linux drivers.
 */
void
linux_init (void)
{
  int addr;
  unsigned long memory_start, memory_end;
  vm_page_t pages;

  /*
   * Initialize memory size.
   */
  high_memory = vm_page_seg_end(VM_PAGE_SEL_DIRECTMAP);
  init_IRQ ();
  linux_sched_init ();

  /*
   * Set loop count.
   */
  calibrate_delay ();

  /*
   * Initialize drive info.
   */
  addr = *((unsigned *) phystokv (0x104));
  memcpy (&drive_info,
	  (void *) ((addr & 0xffff) + ((addr >> 12) & 0xffff0)), 16);
  addr = *((unsigned *) phystokv (0x118));
  memcpy ((char *) &drive_info + 16,
	  (void *) ((addr & 0xffff) + ((addr >> 12) & 0xffff0)), 16);

  /*
   * Initialize Linux memory allocator.
   */
  linux_kmem_init ();

  /*
   * Allocate contiguous memory below 16 MB.
   */
  memory_start = alloc_contig_mem (CONTIG_ALLOC, 16 * 1024 * 1024, 0, &pages);
  if (memory_start == 0)
    panic ("linux_init: alloc_contig_mem failed");
  memory_end = memory_start + CONTIG_ALLOC;

  /*
   * Initialize PCI bus.
   */
  memory_start = pci_init (memory_start, memory_end);

  if (memory_start > memory_end)
    panic ("linux_init: ran out memory");

  /*
   * Initialize devices.
   */
#ifdef CONFIG_INET
  linux_net_emulation_init ();
#endif

  device_setup ();

#ifdef CONFIG_PCMCIA
  /* 
   * Initialize pcmcia.
   */
  pcmcia_init ();
#endif

  restore_IRQ ();

  linux_auto_config = 0;
}

#ifndef NBPW
#define NBPW 32
#endif

/*
 * Allocate contiguous memory with the given constraints.
 */
unsigned long
alloc_contig_mem (unsigned size, unsigned limit,
		  unsigned mask, vm_page_t * pages)
{
  vm_page_t p;

  p = vm_page_grab_contig(size, VM_PAGE_SEL_DMA);

  if (p == NULL)
    return 0;

  if (pages)
    *pages = p;

  return phystokv(vm_page_to_pa(p));
}

/*
 * Free memory allocated by alloc_contig_mem.
 */
void
free_contig_mem (vm_page_t pages, unsigned size)
{
  vm_page_free_contig(pages, size);
}

/* This is the number of bits of precision for the loops_per_second.  Each
 * bit takes on average 1.5/HZ seconds.  This (like the original) is a little
 * better than 1%
 */
#define LPS_PREC 8

static void
calibrate_delay (void)
{
  int ticks;
  int loopbit;
  int lps_precision = LPS_PREC;

  loops_per_sec = (1 << 12);

#ifndef MACH
  printk ("Calibrating delay loop.. ");
#endif
  while (loops_per_sec <<= 1)
    {
      /* wait for "start of" clock tick */
      ticks = jiffies;
      while (ticks == jiffies)
	/* nothing */ ;
      /* Go .. */
      ticks = jiffies;
      __delay (loops_per_sec);
      ticks = jiffies - ticks;
      if (ticks)
	break;
    }

  /* Do a binary approximation to get loops_per_second set to equal one clock
   * (up to lps_precision bits)
   */
  loops_per_sec >>= 1;
  loopbit = loops_per_sec;
  while (lps_precision-- && (loopbit >>= 1))
    {
      loops_per_sec |= loopbit;
      ticks = jiffies;
      while (ticks == jiffies);
      ticks = jiffies;
      __delay (loops_per_sec);
      if (jiffies != ticks)	/* longer than 1 tick */
	loops_per_sec &= ~loopbit;
    }

  /* finally, adjust loops per second in terms of seconds instead of clocks */
  loops_per_sec *= HZ;
  /* Round the value and print it */
#ifndef MACH
  printk ("ok - %lu.%02lu BogoMIPS\n",
	  (loops_per_sec + 2500) / 500000,
	  ((loops_per_sec + 2500) / 5000) % 100);
#endif

#if defined(__SMP__) && defined(__i386__)
  smp_loops_per_tick = loops_per_sec / 400;
#endif
}
