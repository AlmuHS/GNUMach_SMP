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
#include <mach/machine/eflags.h>
#include <mach/machine/vm_types.h>
#include <mach/xen.h>

#include <intel/pmap.h>
#include <kern/debug.h>

#include "vm_param.h"
#include "seg.h"
#include "gdt.h"
#include "ldt.h"
#include "locore.h"
#include "mp_desc.h"
#include "msr.h"

#ifdef	MACH_PV_DESCRIPTORS
/* It is actually defined in xen_boothdr.S */
extern
#endif	/* MACH_PV_DESCRIPTORS */
struct real_descriptor ldt[LDTSZ];

#if defined(__x86_64__) && ! defined(USER32)
#define USER_SEGMENT_SIZEBITS SZ_64
#else
#define USER_SEGMENT_SIZEBITS SZ_32
#endif

void
ldt_fill(struct real_descriptor *myldt, struct real_descriptor *mygdt)
{
#ifdef	MACH_PV_DESCRIPTORS
#ifdef	MACH_PV_PAGETABLES
	pmap_set_page_readwrite(myldt);
#endif	/* MACH_PV_PAGETABLES */
#else	/* MACH_PV_DESCRIPTORS */
	/* Initialize the master LDT descriptor in the GDT.  */
	_fill_gdt_sys_descriptor(mygdt, KERNEL_LDT,
			        kvtolin(myldt), (LDTSZ * sizeof(struct real_descriptor))-1,
			        ACC_PL_K|ACC_LDT, 0);
#endif	/* MACH_PV_DESCRIPTORS */

	/* Initialize the syscall entry point */
#if defined(__x86_64__) && ! defined(USER32)
        if (!CPU_HAS_FEATURE(CPU_FEATURE_SEP))
            panic("syscall support is missing on 64 bit");
        /* Enable 64-bit syscalls */
        wrmsr(MSR_REG_EFER, rdmsr(MSR_REG_EFER) | MSR_EFER_SCE);
        wrmsr(MSR_REG_LSTAR, (vm_offset_t)syscall64);
        wrmsr(MSR_REG_STAR, ((((long)USER_CS - 16) << 16) | (long)KERNEL_CS) << 32);
        wrmsr(MSR_REG_FMASK, EFL_IF | EFL_IOPL_USER);
#else /* defined(__x86_64__) && ! defined(USER32) */
	fill_ldt_gate(myldt, USER_SCALL,
		      (vm_offset_t)&syscall, KERNEL_CS,
		      ACC_PL_U|ACC_CALL_GATE, 0);
#endif /* defined(__x86_64__) && ! defined(USER32) */

	/* Initialize the 32bit LDT descriptors.  */
	fill_ldt_descriptor(myldt, USER_CS,
			    VM_MIN_USER_ADDRESS,
			    VM_MAX_USER_ADDRESS-VM_MIN_USER_ADDRESS-4096,
			    /* XXX LINEAR_... */
			    ACC_PL_U|ACC_CODE_R, USER_SEGMENT_SIZEBITS);
	fill_ldt_descriptor(myldt, USER_DS,
			    VM_MIN_USER_ADDRESS,
			    VM_MAX_USER_ADDRESS-VM_MIN_USER_ADDRESS-4096,
			    ACC_PL_U|ACC_DATA_W, USER_SEGMENT_SIZEBITS);

	/* Activate the LDT.  */
#ifdef	MACH_PV_DESCRIPTORS
	hyp_set_ldt(myldt, LDTSZ);
#else	/* MACH_PV_DESCRIPTORS */
	lldt(KERNEL_LDT);
#endif	/* MACH_PV_DESCRIPTORS */
}

void
ldt_init(void)
{
	ldt_fill(ldt, gdt);
}

#if NCPUS > 1
void
ap_ldt_init(int cpu)
{
	ldt_fill(mp_desc_table[cpu]->ldt, mp_gdt[cpu]);
}
#endif
