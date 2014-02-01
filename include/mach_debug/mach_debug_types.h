/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 *	Mach kernel debugging interface type declarations
 */

#ifndef	_MACH_DEBUG_MACH_DEBUG_TYPES_H_
#define _MACH_DEBUG_MACH_DEBUG_TYPES_H_

#include <mach_debug/ipc_info.h>
#include <mach_debug/vm_info.h>
#include <mach_debug/slab_info.h>
#include <mach_debug/hash_info.h>

typedef	char	symtab_name_t[32];

/*
 *	A fixed-length string data type intended for names given to
 *	kernel objects.
 *
 *	Note that it is not guaranteed that the in-kernel data
 *	structure will hold KERNEL_DEBUG_NAME_MAX bytes.  The given
 *	name will be truncated to fit into the target data structure.
 */
#define KERNEL_DEBUG_NAME_MAX (64)
typedef char	kernel_debug_name_t[KERNEL_DEBUG_NAME_MAX];

#endif	/* _MACH_DEBUG_MACH_DEBUG_TYPES_H_ */
