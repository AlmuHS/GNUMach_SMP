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

#ifdef DEBUG

#include <mach/machine/asm.h>
#include <i386/xen.h>

#include "debug.h"

	.text
ENTRY(_debug_trace)
	pushf
	cli
	pushq	%rax
	pushq	%rbx
	.byte	0x36	/* SS: bug in gas? */
	movl	%ss:EXT(debug_trace_pos),%eax
	movq	S_ARG0,%rbx
	movq	%rbx,%ss:EXT(debug_trace_buf)(,%eax,16)
	movl	S_ARG1,%ebx
	movl	%ebx,%ss:EXT(debug_trace_buf)+8(,%eax,16)
	incl	%eax
	andl	$DEBUG_TRACE_LEN-1,%eax
	.byte	0x36	/* SS: bug in gas? */
	movl	%eax,%ss:EXT(debug_trace_pos)
	popq	%rbx
	popq	%rax
	popf
	ret

#endif /* DEBUG */

/* XXX gas bug?  need at least one symbol... */
foo:

