/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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
 *	File:	ipc/ipc_entry.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Definitions for translation entries, which represent
 *	tasks' capabilities for ports and port sets.
 */

#ifndef	_IPC_IPC_ENTRY_H_
#define _IPC_IPC_ENTRY_H_

#include <mach/mach_types.h>
#include <mach/port.h>
#include <mach/kern_return.h>
#include <kern/slab.h>
#include <ipc/port.h>
#include <ipc/ipc_table.h>
#include <ipc/ipc_types.h>

/*
 *	Spaces hold capabilities for ipc_object_t's (ports and port sets).
 *	Each ipc_entry_t records a capability.
 */

typedef unsigned int ipc_entry_bits_t;
typedef ipc_table_elems_t ipc_entry_num_t;	/* number of entries */

typedef struct ipc_entry {
	mach_port_name_t ie_name;
	ipc_entry_bits_t ie_bits;
	struct ipc_object *ie_object;
	union {
		struct ipc_entry *next_free;
		/*XXX ipc_port_request_index_t request;*/
		unsigned int request;
	} index;
} *ipc_entry_t;

#define	IE_NULL		((ipc_entry_t) 0)

#define	ie_request	index.request
#define	ie_next_free	index.next_free

#define	IE_BITS_UREFS_MASK	0x0000ffff	/* 16 bits of user-reference */
#define	IE_BITS_UREFS(bits)	((bits) & IE_BITS_UREFS_MASK)

#define	IE_BITS_TYPE_MASK	0x001f0000	/* 5 bits of capability type */
#define	IE_BITS_TYPE(bits)	((bits) & IE_BITS_TYPE_MASK)

#define	IE_BITS_MAREQUEST	0x00200000	/* 1 bit for msg-accepted */

#define	IE_BITS_RIGHT_MASK	0x003fffff	/* relevant to the right */

#if PORT_GENERATIONS
#error "not supported"
#define	IE_BITS_GEN_MASK	0xff000000U	/* 8 bits for generation */
#define	IE_BITS_GEN(bits)	((bits) & IE_BITS_GEN_MASK)
#define	IE_BITS_GEN_ONE		0x01000000	/* low bit of generation */
#else
#define	IE_BITS_GEN_MASK	0
#define	IE_BITS_GEN(bits)	0
#define	IE_BITS_GEN_ONE		0
#endif


extern struct kmem_cache ipc_entry_cache;
#define ie_alloc()	((ipc_entry_t) kmem_cache_alloc(&ipc_entry_cache))
#define	ie_free(e)	kmem_cache_free(&ipc_entry_cache, (vm_offset_t) (e))

extern kern_return_t
ipc_entry_alloc(ipc_space_t space, mach_port_name_t *namep, ipc_entry_t *entryp);

extern kern_return_t
ipc_entry_alloc_name(ipc_space_t space, mach_port_name_t name, ipc_entry_t *entryp);

ipc_entry_t
db_ipc_object_by_name(
       task_t        	  task,
       mach_port_name_t   name);

#endif	/* _IPC_IPC_ENTRY_H_ */
