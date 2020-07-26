/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity 
 * pertaining to distribution of the software without specific, written
 * prior permission.
 * 
 * CARNEGIE MELLON, IBM, AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON, IBM, AND CSL DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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
 * This file describes the standard LDT provided by default
 * to all user-level Mach tasks.
 */
#ifndef _I386_LDT_
#define _I386_LDT_

#include "seg.h"

/*
 * User descriptors for Mach - 32-bit flat address space
 */
#define	USER_SCALL	0x07		/* system call gate */
#define	USER_CS		0x17		/* user code segment */
#define	USER_DS		0x1f		/* user data segment */

#define	LDTSZ		4


#ifndef __ASSEMBLER__

extern struct real_descriptor ldt[LDTSZ];

/* Fill a segment descriptor in the LDT.  */
#define fill_ldt_descriptor(selector, base, limit, access, sizebits) \
	fill_descriptor(&ldt[selector/8], base, limit, access, sizebits)

#define fill_ldt_gate(selector, offset, dest_selector, access, word_count) \
	fill_gate((struct real_gate*)&ldt[selector/8], \
		  offset, dest_selector, access, word_count)

void ldt_init(void);

#endif /* !__ASSEMBLER__ */

#endif /* _I386_LDT_ */
