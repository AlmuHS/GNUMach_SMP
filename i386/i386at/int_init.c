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

#include <i386at/idt.h>
#include <i386at/int_init.h>
#include <i386/gdt.h>
#include <i386/mp_desc.h>
#include <kern/printf.h>
#ifdef APIC
#include <i386/apic.h>
#endif

/* defined in locore.S */
extern vm_offset_t int_entry_table[];

static void
int_fill(struct real_gate *myidt)
{
	int i;
#ifndef APIC
	int base = PIC_INT_BASE;
	int nirq = 16;
#else
	int base = IOAPIC_INT_BASE;
	int nirq = NINTR;
#endif

	for (i = 0; i < nirq; i++) {
		fill_idt_gate(myidt, base + i,
			      int_entry_table[i], KERNEL_CS,
			      ACC_PL_K|ACC_INTR_GATE, 0);
	}
#if NCPUS > 1
	fill_idt_gate(myidt, CALL_AST_CHECK,
			      int_entry_table[i], KERNEL_CS,
			      ACC_PL_K|ACC_INTR_GATE, 0);
	i++;
	fill_idt_gate(myidt, CALL_PMAP_UPDATE,
			      int_entry_table[i], KERNEL_CS,
			      ACC_PL_K|ACC_INTR_GATE, 0);
	i++;
#endif
#ifdef APIC
	fill_idt_gate(myidt, IOAPIC_SPURIOUS_BASE,
			      int_entry_table[i], KERNEL_CS,
			      ACC_PL_K|ACC_INTR_GATE, 0);
	i++;
#endif
}

void
int_init(void)
{
	int_fill(idt);
}

#if NCPUS > 1
void ap_int_init(int cpu)
{
	int_fill(mp_desc_table[cpu]->idt);
}
#endif
