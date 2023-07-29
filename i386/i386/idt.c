/* 
 * Copyright (c) 1994 The University of Utah and
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

#include <i386/vm_param.h>
#include <i386/seg.h>
#include <i386at/idt.h>
#include <i386/gdt.h>
#include <i386/mp_desc.h>

struct real_gate idt[IDTSZ];

struct idt_init_entry
{
	unsigned long entrypoint;
	unsigned short vector;
	unsigned short type;
#ifdef __x86_64__
	unsigned short ist;
	unsigned short pad_0;
#endif
};
extern struct idt_init_entry idt_inittab[];

static void
idt_fill(struct real_gate *myidt)
{
#ifdef	MACH_PV_DESCRIPTORS
	if (hyp_set_trap_table(kvtolin(idt_inittab)))
		panic("couldn't set trap table\n");
#else	/* MACH_PV_DESCRIPTORS */
	struct idt_init_entry *iie = idt_inittab;

	/* Initialize the exception vectors from the idt_inittab.  */
	while (iie->entrypoint)
	{
		fill_idt_gate(myidt, iie->vector, iie->entrypoint, KERNEL_CS, iie->type,
#ifdef __x86_64__
			      iie->ist
#else
			      0
#endif
		    );
		iie++;
	}

	/* Load the IDT pointer into the processor.  */
	{
		struct pseudo_descriptor pdesc;

		pdesc.limit = (IDTSZ * sizeof(struct real_gate))-1;
		pdesc.linear_base = kvtolin(myidt);
		lidt(&pdesc);
	}
#endif	/* MACH_PV_DESCRIPTORS */
}

void idt_init(void)
{
	idt_fill(idt);
}

#if NCPUS > 1
void ap_idt_init(int cpu)
{
	idt_fill(mp_desc_table[cpu]->idt);
}
#endif
