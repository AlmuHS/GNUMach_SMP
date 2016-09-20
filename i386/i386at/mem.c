/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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

#include <device/io_req.h>
#include <i386/model_dep.h>

/* This provides access to any memory that is not main RAM */

/*ARGSUSED*/
int
memmmap(dev, off, prot)
dev_t		dev;
vm_offset_t	off;
vm_prot_t	prot;
{
	struct vm_page *p;

	if (off == 0)
		return 0;

	/*
	 * The legacy device mappings are included in the page tables and
	 * need their own test.
	 */
	if (off >= 0xa0000 && off < 0x100000)
		goto out;

	p = vm_page_lookup_pa(off);

	if (p != NULL) {
		return -1;
	}

out:
	return i386_btop(off);
}
