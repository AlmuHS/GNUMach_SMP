/*
 * Copyright (c) 1995-1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
#ifndef _MACH_I386_MULTIBOOT_H_
#define _MACH_I386_MULTIBOOT_H_

#include <mach/machine/vm_types.h>

/* For a.out kernel boot images, the following header must appear
   somewhere in the first 8192 bytes of the kernel image file.  */
struct multiboot_header
{
	/* Must be MULTIBOOT_MAGIC */
	unsigned		magic;

	/* Feature flags - see below.  */
	unsigned		flags;

	/*
	 * Checksum
	 *
	 * The above fields plus this one must equal 0 mod 2^32.
	 */
	unsigned		checksum;

	/* These are only valid if MULTIBOOT_AOUT_KLUDGE is set.  */
	vm_offset_t		header_addr;
	vm_offset_t		load_addr;
	vm_offset_t		load_end_addr;
	vm_offset_t		bss_end_addr;
	vm_offset_t		entry;
};

/* The entire multiboot_header must be contained
   within the first MULTIBOOT_SEARCH bytes of the kernel image.  */
#define MULTIBOOT_SEARCH	8192

/* Magic value identifying the multiboot_header.  */
#define MULTIBOOT_MAGIC		0x1badb002

/* Features flags for 'flags'.
   If a boot loader sees a flag in MULTIBOOT_MUSTKNOW set
   and it doesn't understand it, it must fail.  */
#define MULTIBOOT_MUSTKNOW	0x0000ffff

/* Align all boot modules on page (4KB) boundaries.  */
#define MULTIBOOT_PAGE_ALIGN	0x00000001

/* Must be provided memory information in multiboot_info structure */
#define MULTIBOOT_MEMORY_INFO	0x00000002

/* Use the load address fields above instead of the ones in the a.out header
   to figure out what to load where, and what to do afterwards.
   This should only be needed for a.out kernel images
   (ELF and other formats can generally provide the needed information).  */
#define MULTIBOOT_AOUT_KLUDGE	0x00010000

/* The boot loader passes this value in register EAX to signal the kernel
   that the multiboot method is being used */
#define MULTIBOOT_VALID         0x2badb002

/* The boot loader passes this data structure to the kernel in
   register EBX on entry.  */
struct multiboot_info
{
	/* These flags indicate which parts of the multiboot_info are valid;
	   see below for the actual flag bit definitions.  */
	unsigned		flags;

	/* Lower/Upper memory installed in the machine.
	   Valid only if MULTIBOOT_MEMORY is set in flags word above.  */
	vm_size_t		mem_lower;
	vm_size_t		mem_upper;

	/* BIOS disk device the kernel was loaded from.
	   Valid only if MULTIBOOT_BOOT_DEVICE is set in flags word above.  */
	unsigned char		boot_device[4];

	/* Command-line for the OS kernel: a null-terminated ASCII string.
	   Valid only if MULTIBOOT_CMDLINE is set in flags word above.  */
	vm_offset_t		cmdline;

	/* List of boot modules loaded with the kernel.
	   Valid only if MULTIBOOT_MODS is set in flags word above.  */
	unsigned		mods_count;
	vm_offset_t		mods_addr;

	/* Symbol information for a.out or ELF executables. */
	union
	{
	  struct
	  {
	    /* a.out symbol information valid only if MULTIBOOT_AOUT_SYMS
	       is set in flags word above.  */
	    vm_size_t		tabsize;
	    vm_size_t		strsize;
	    vm_offset_t		addr;
	    unsigned		reserved;
	  } a;

	  struct
	  {
	    /* ELF section header information valid only if
	       MULTIBOOT_ELF_SHDR is set in flags word above.  */
	    unsigned		num;
	    vm_size_t		size;
	    vm_offset_t		addr;
	    unsigned		shndx;
	  } e;
	} syms;

	/* Memory map buffer.
	   Valid only if MULTIBOOT_MEM_MAP is set in flags word above.  */
	vm_size_t		mmap_count;
	vm_offset_t		mmap_addr;
};

#define MULTIBOOT_MEMORY	0x00000001
#define MULTIBOOT_BOOT_DEVICE	0x00000002
#define MULTIBOOT_CMDLINE	0x00000004
#define MULTIBOOT_MODS		0x00000008
#define MULTIBOOT_AOUT_SYMS	0x00000010
#define MULTIBOOT_ELF_SHDR	0x00000020
#define MULTIBOOT_MEM_MAP	0x00000040


/* The mods_addr field above contains the physical address of the first
   of 'mods_count' multiboot_module structures.  */
struct multiboot_module
{
	/* Physical start and end addresses of the module data itself.  */
	vm_offset_t		mod_start;
	vm_offset_t		mod_end;

	/* Arbitrary ASCII string associated with the module.  */
	vm_offset_t		string;

	/* Boot loader must set to 0; OS must ignore.  */
	unsigned		reserved;
};


/* The mmap_addr field above contains the physical address of the first
   of the AddrRangeDesc structure.  "size" represents the size of the
   rest of the structure and optional padding.  The offset to the beginning
   of the next structure is therefore "size + 4".  */
struct AddrRangeDesc
{
  unsigned long size;
  unsigned long BaseAddrLow;
  unsigned long BaseAddrHigh;
  unsigned long LengthLow;
  unsigned long LengthHigh;
  unsigned long Type;

  /* unspecified optional padding... */
};

struct multiboot_mmap
{
  unsigned long size;
  unsigned long long BaseAddr;
  unsigned long long Length;
  unsigned long Type;

  /* unspecified optional padding... */
};

/* usable memory "Type", all others are reserved.  */
#define MB_ARD_MEMORY       1

/*
 * Copyright (c) 2010, 2012 Richard Braun.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Versions used by the biosmem module.
 */

#include <kern/macros.h>

/*
 * Magic number provided by the OS to the boot loader.
 */
#define MULTIBOOT_OS_MAGIC 0x1badb002

/*
 * Multiboot flags requesting services from the boot loader.
 */
#define MULTIBOOT_OS_MEMORY_INFO 0x2

#define MULTIBOOT_OS_FLAGS MULTIBOOT_OS_MEMORY_INFO

/*
 * Magic number to identify a multiboot compliant boot loader.
 */
#define MULTIBOOT_LOADER_MAGIC 0x2badb002

/*
 * Multiboot flags set by the boot loader.
 */
#define MULTIBOOT_LOADER_MEMORY     0x01
#define MULTIBOOT_LOADER_CMDLINE    0x04
#define MULTIBOOT_LOADER_MODULES    0x08
#define MULTIBOOT_LOADER_SHDR       0x20
#define MULTIBOOT_LOADER_MMAP       0x40

/*
 * A multiboot module.
 */
struct multiboot_raw_module {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} __packed;

/*
 * Memory map entry.
 */
struct multiboot_raw_mmap_entry {
    uint32_t size;
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
} __packed;

/*
 * Multiboot information structure as passed by the boot loader.
 */
struct multiboot_raw_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t unused0;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t shdr_num;
    uint32_t shdr_size;
    uint32_t shdr_addr;
    uint32_t shdr_strndx;
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t unused1[9];
} __packed;

/*
 * Versions of the multiboot structures suitable for use with 64-bit pointers.
 */

struct multiboot_os_module {
    void *mod_start;
    void *mod_end;
    char *string;
};

struct multiboot_os_info {
    uint32_t flags;
    char *cmdline;
    struct multiboot_module *mods_addr;
    uint32_t mods_count;
};

#endif /* _MACH_I386_MULTIBOOT_H_ */
