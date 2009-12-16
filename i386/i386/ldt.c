/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 * "Local" descriptor table.  At the moment, all tasks use the
 * same LDT.
 */
#include <mach/machine/vm_types.h>
#include <mach/xen.h>

#include <intel/pmap.h>

#include "vm_param.h"
#include "seg.h"
#include "gdt.h"
#include "ldt.h"

extern int syscall();

#ifdef	MACH_XEN
/* It is actually defined in xen_boothdr.S */
extern
#endif	/* MACH_XEN */
struct real_descriptor ldt[LDTSZ];

void
ldt_init()
{
#ifdef	MACH_XEN
	pmap_set_page_readwrite(ldt);
#else	/* MACH_XEN */
	/* Initialize the master LDT descriptor in the GDT.  */
	fill_gdt_descriptor(KERNEL_LDT,
			    kvtolin(&ldt), sizeof(ldt)-1,
			    ACC_PL_K|ACC_LDT, 0);
#endif	/* MACH_XEN */

	/* Initialize the LDT descriptors.  */
	fill_ldt_gate(USER_SCALL,
		      (vm_offset_t)&syscall, KERNEL_CS,
		      ACC_PL_U|ACC_CALL_GATE, 0);
	fill_ldt_descriptor(USER_CS,
			    VM_MIN_ADDRESS,
			    VM_MAX_ADDRESS-VM_MIN_ADDRESS-4096,
			    /* XXX LINEAR_... */
			    ACC_PL_U|ACC_CODE_R, SZ_32);
	fill_ldt_descriptor(USER_DS,
			    VM_MIN_ADDRESS,
			    VM_MAX_ADDRESS-VM_MIN_ADDRESS-4096,
			    ACC_PL_U|ACC_DATA_W, SZ_32);

	/* Activate the LDT.  */
#ifdef	MACH_HYP
	hyp_set_ldt(&ldt, LDTSZ);
#else	/* MACH_HYP */
	lldt(KERNEL_LDT);
#endif	/* MACH_HYP */
}
