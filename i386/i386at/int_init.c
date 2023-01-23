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

/* defined in locore.S */
extern vm_offset_t int_entry_table[];

void int_init(void)
{
	int i;
#ifndef APIC
	for (i = 0; i < 16; i++) {
		fill_idt_gate(PIC_INT_BASE + i,
			      int_entry_table[i], KERNEL_CS,
			      ACC_PL_K|ACC_INTR_GATE, 0);
	}
#else
	for (i = 0; i < 24; i++) {
		fill_idt_gate(IOAPIC_INT_BASE + i,
			      int_entry_table[i], KERNEL_CS,
			      ACC_PL_K|ACC_INTR_GATE, 0);
	}
	fill_idt_gate(IOAPIC_SPURIOUS_BASE,
			      int_entry_table[24], KERNEL_CS,
			      ACC_PL_K|ACC_INTR_GATE, 0);
#endif
}

