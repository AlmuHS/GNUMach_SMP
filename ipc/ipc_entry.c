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
 *	File:	ipc/ipc_entry.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Primitive functions to manipulate translation entries.
 */

#include <kern/printf.h>
#include <string.h>

#include <mach/kern_return.h>
#include <mach/port.h>
#include <kern/assert.h>
#include <kern/sched_prim.h>
#include <kern/slab.h>
#include <ipc/port.h>
#include <ipc/ipc_types.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_table.h>
#include <ipc/ipc_object.h>

struct kmem_cache ipc_entry_cache;

/*
 *	Routine:	ipc_entry_alloc
 *	Purpose:
 *		Allocate an entry out of the space.
 *	Conditions:
 *		The space must be write-locked.  May allocate memory.
 *	Returns:
 *		KERN_SUCCESS		An entry was allocated.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_NO_SPACE		No room for an entry in the space.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory for an entry.
 */

kern_return_t
ipc_entry_alloc(
	ipc_space_t	space,
	mach_port_name_t	*namep,
	ipc_entry_t	*entryp)
{
	kern_return_t kr;
	ipc_entry_t entry;
	rdxtree_key_t key;

	if (!space->is_active) {
		return KERN_INVALID_TASK;
	}

	kr = ipc_entry_get(space, namep, entryp);
	if (kr == KERN_SUCCESS)
		return kr;

	entry = ie_alloc();
	if (entry == IE_NULL) {
		return KERN_RESOURCE_SHORTAGE;
	}

	kr = rdxtree_insert_alloc(&space->is_map, entry, &key);
	if (kr) {
		ie_free(entry);
		return kr;
	}
	space->is_size += 1;

	entry->ie_bits = 0;
	entry->ie_object = IO_NULL;
	entry->ie_request = 0;
	entry->ie_name = (mach_port_name_t) key;

	*entryp = entry;
	*namep = (mach_port_name_t) key;
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_entry_alloc_name
 *	Purpose:
 *		Allocates/finds an entry with a specific name.
 *		If an existing entry is returned, its type will be nonzero.
 *	Conditions:
 *		The space must be write-locked.  May allocate memory.
 *	Returns:
 *		KERN_SUCCESS		Found existing entry with same name.
 *		KERN_SUCCESS		Allocated a new entry.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
ipc_entry_alloc_name(
	ipc_space_t	space,
	mach_port_name_t	name,
	ipc_entry_t	*entryp)
{
	kern_return_t kr;
	ipc_entry_t entry, e, *prevp;
	void **slot;
	assert(MACH_PORT_NAME_VALID(name));

	if (!space->is_active) {
		return KERN_INVALID_TASK;
	}

	slot = rdxtree_lookup_slot(&space->is_map, (rdxtree_key_t) name);
	if (slot != NULL)
		entry = *(ipc_entry_t *) slot;

	if (slot == NULL || entry == IE_NULL) {
		entry = ie_alloc();
		if (entry == IE_NULL) {
			return KERN_RESOURCE_SHORTAGE;
		}

		entry->ie_bits = 0;
		entry->ie_object = IO_NULL;
		entry->ie_request = 0;
		entry->ie_name = name;

		if (slot != NULL)
			rdxtree_replace_slot(slot, entry);
		else {
			kr = rdxtree_insert(&space->is_map,
					    (rdxtree_key_t) name, entry);
			if (kr != KERN_SUCCESS) {
				ie_free(entry);
				return kr;
			}
		}
		space->is_size += 1;

		*entryp = entry;
		return KERN_SUCCESS;
	}

	if (IE_BITS_TYPE(entry->ie_bits)) {
		/* Used entry.  */
		*entryp = entry;
		return KERN_SUCCESS;
	}

	/* Free entry.  Rip the entry out of the free list.  */
	for (prevp = &space->is_free_list, e = space->is_free_list;
	     e != entry;
	     ({ prevp = &e->ie_next_free; e = e->ie_next_free; }))
		continue;

	*prevp = entry->ie_next_free;
	space->is_free_list_size -= 1;

	entry->ie_bits = 0;
	assert(entry->ie_object == IO_NULL);
	assert(entry->ie_name == name);
	entry->ie_request = 0;

	space->is_size += 1;
	*entryp = entry;
	return KERN_SUCCESS;
}

#if	MACH_KDB
#include <ddb/db_output.h>
#include <kern/task.h>

#define	printf	kdbprintf

ipc_entry_t
db_ipc_object_by_name(
	const task_t	task,
	mach_port_name_t	name)
{
        ipc_space_t space = task->itk_space;
        ipc_entry_t entry;
 
        entry = ipc_entry_lookup(space, name);
        if(entry != IE_NULL) {
                iprintf("(task 0x%x, name 0x%x) ==> object 0x%x",
			entry->ie_object);
                return (ipc_entry_t) entry->ie_object;
        }
        return entry;
}
#endif	/* MACH_KDB */
