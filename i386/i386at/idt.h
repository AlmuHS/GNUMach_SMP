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

#ifndef _I386AT_IDT_
#define _I386AT_IDT_

/* There are 256 interrupt vectors on x86,
 * the first 32 are taken by cpu faults */
#define IDTSZ (0x100)

/* PIC sits at 0x20-0x2f */
#define PIC_INT_BASE 0x20

/* IOAPIC sits at 0x30-0x47 */
#define IOAPIC_INT_BASE 0x30

/* IOAPIC spurious interrupt vector set to 0xff */
#define IOAPIC_SPURIOUS_BASE 0xff

/* Remote -> local AST requests */
#define CALL_AST_CHECK 0xfa

/* Currently for TLB shootdowns */
#define CALL_PMAP_UPDATE 0xfb

#include <i386/idt-gen.h>

#ifndef __ASSEMBLER__
extern void idt_init (void);
extern void ap_idt_init (int cpu);
#endif /* __ASSEMBLER__ */

#endif /* _I386AT_IDT_ */
