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
#ifndef _MACH_MULTIBOOT_H_
#define _MACH_MULTIBOOT_H_

#include <mach/machine/vm_types.h>
#include <mach/machine/multiboot.h>

struct multiboot_region
{
	vm_offset_t		start;
	vm_offset_t		end;
};

struct multiboot_rlist
{
	int			count;
	vm_offset_t		regions;
};

struct multiboot_module
{
	/* Location and size of the module.  */
	struct multiboot_region	region;

	/* Command-line associated with this boot module:
	   a null-terminated ASCII string.
	   Both start and end are 0 if there is no command line.
	   The end pointer points at least one byte past the terminating null.  */
	struct multiboot_region	cmdline;

	/* Reserved; boot loader must initialize to zero.  */
	natural_t		pad[4];
};

struct multiboot_info
{
	/* List of available physical memory regions.
	   Can (and probably does) include the memory containing
	   the kernel, boot modules, this structure, etc.  */
	struct multiboot_rlist	avail;

	/* Physical memory region occupied by things the boot loader set up
	   and the OS shouldn't clobber at least until it's all done initializing itself.
	   This includes the kernel image, boot modules, these structures,
	   initial processor tables, etc.  */
	struct multiboot_rlist	occupied;

	/* Command-line for the OS kernel: a null-terminated ASCII string.
	   Both start and end are 0 if there is no command line.
	   The end pointer points at least one byte past the terminating null.  */
	struct multiboot_region	cmdline;

	/* Secondary boot modules loaded with this kernel image.  */
	int			nmods;
	vm_offset_t		mods;

	/* Reserved; boot loader must initialize to zero.  */
	natural_t		pad[4];
};

#endif _MACH_MULTIBOOT_H_
