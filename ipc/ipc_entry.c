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
 *	Routine:	ipc_entry_lookup
 *	Purpose:
 *		Searches for an entry, given its name.
 *	Conditions:
 *		The space must be read or write locked throughout.
 *		The space must be active.
 */

ipc_entry_t
ipc_entry_lookup(
	ipc_space_t space,
	mach_port_t name)
{
	ipc_entry_t entry;

	assert(space->is_active);
	entry = rdxtree_lookup(&space->is_map, (rdxtree_key_t) name);
	if (entry != IE_NULL
	    && IE_BITS_TYPE(entry->ie_bits) == MACH_PORT_TYPE_NONE)
		entry = NULL;
	assert((entry == IE_NULL) || IE_BITS_TYPE(entry->ie_bits));
	return entry;
}

/*
 *	Routine:	ipc_entry_get
 *	Purpose:
 *		Tries to allocate an entry out of the space.
 *	Conditions:
 *		The space is write-locked and active throughout.
 *		An object may be locked.  Will not allocate memory.
 *	Returns:
 *		KERN_SUCCESS		A free entry was found.
 *		KERN_NO_SPACE		No entry allocated.
 */

kern_return_t
ipc_entry_get(
	ipc_space_t space,
	mach_port_t *namep,
	ipc_entry_t *entryp)
{
	mach_port_t new_name;
	ipc_entry_t free_entry;

	assert(space->is_active);

	/* Get entry from the free list.  */
	free_entry = space->is_free_list;
	if (free_entry == IE_NULL)
		return KERN_NO_SPACE;

	space->is_free_list = free_entry->ie_next_free;
	space->is_free_list_size -= 1;

	/*
	 *	Initialize the new entry.  We need only
	 *	increment the generation number and clear ie_request.
	 */

    {
	mach_port_gen_t gen;

	assert((free_entry->ie_bits &~ IE_BITS_GEN_MASK) == 0);
	gen = free_entry->ie_bits + IE_BITS_GEN_ONE;
	free_entry->ie_bits = gen;
	free_entry->ie_request = 0;
	new_name = MACH_PORT_MAKE(free_entry->ie_name, gen);
    }

	/*
	 *	The new name can't be MACH_PORT_NULL because index
	 *	is non-zero.  It can't be MACH_PORT_DEAD because
	 *	the table isn't allowed to grow big enough.
	 *	(See comment in ipc/ipc_table.h.)
	 */

	assert(MACH_PORT_VALID(new_name));
	assert(free_entry->ie_object == IO_NULL);

	space->is_size += 1;
	*namep = new_name;
	*entryp = free_entry;
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_entry_alloc
 *	Purpose:
 *		Allocate an entry out of the space.
 *	Conditions:
 *		The space is not locked before, but it is write-locked after
 *		if the call is successful.  May allocate memory.
 *	Returns:
 *		KERN_SUCCESS		An entry was allocated.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_NO_SPACE		No room for an entry in the space.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory for an entry.
 */

kern_return_t
ipc_entry_alloc(
	ipc_space_t	space,
	mach_port_t	*namep,
	ipc_entry_t	*entryp)
{
	kern_return_t kr;
	ipc_entry_t entry;
	rdxtree_key_t key;

	is_write_lock(space);

	if (!space->is_active) {
		is_write_unlock(space);
		return KERN_INVALID_TASK;
	}

	kr = ipc_entry_get(space, namep, entryp);
	if (kr == KERN_SUCCESS)
		/* Success.  Space is write-locked.  */
		return kr;

	entry = ie_alloc();
	if (entry == IE_NULL) {
		is_write_unlock(space);
		return KERN_RESOURCE_SHORTAGE;
	}

	kr = rdxtree_insert_alloc(&space->is_map, entry, &key);
	if (kr) {
		is_write_unlock(space);
		ie_free(entry);
		return kr;
	}
	space->is_size += 1;

	entry->ie_bits = 0;
	entry->ie_object = IO_NULL;
	entry->ie_request = 0;
	entry->ie_name = (mach_port_t) key;

	*entryp = entry;
	*namep = (mach_port_t) key;
	/* Success.  Space is write-locked.  */
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_entry_alloc_name
 *	Purpose:
 *		Allocates/finds an entry with a specific name.
 *		If an existing entry is returned, its type will be nonzero.
 *	Conditions:
 *		The space is not locked before, but it is write-locked after
 *		if the call is successful.  May allocate memory.
 *	Returns:
 *		KERN_SUCCESS		Found existing entry with same name.
 *		KERN_SUCCESS		Allocated a new entry.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
ipc_entry_alloc_name(
	ipc_space_t	space,
	mach_port_t	name,
	ipc_entry_t	*entryp)
{
	kern_return_t kr;
	ipc_entry_t entry, e, *prevp;
	void **slot;
	assert(MACH_PORT_VALID(name));

	is_write_lock(space);

	if (!space->is_active) {
		is_write_unlock(space);
		return KERN_INVALID_TASK;
	}

	slot = rdxtree_lookup_slot(&space->is_map, (rdxtree_key_t) name);
	if (slot != NULL)
		entry = *(ipc_entry_t *) slot;

	if (slot == NULL || entry == IE_NULL) {
		entry = ie_alloc();
		if (entry == IE_NULL) {
			is_write_unlock(space);
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
				is_write_unlock(space);
				ie_free(entry);
				return kr;
			}
		}
		space->is_size += 1;

		*entryp = entry;
		/* Success.  Space is write-locked.  */
		return KERN_SUCCESS;
	}

	if (IE_BITS_TYPE(entry->ie_bits)) {
		/* Used entry.  */
		*entryp = entry;
		/* Success.  Space is write-locked.  */
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
	/* Success.  Space is write-locked.  */
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_entry_dealloc
 *	Purpose:
 *		Deallocates an entry from a space.
 *	Conditions:
 *		The space must be write-locked throughout.
 *		The space must be active.
 */

void
ipc_entry_dealloc(
	ipc_space_t	space,
	mach_port_t	name,
	ipc_entry_t	entry)
{
	assert(space->is_active);
	assert(entry->ie_object == IO_NULL);
	assert(entry->ie_request == 0);

	if (space->is_free_list_size < IS_FREE_LIST_SIZE_LIMIT) {
		space->is_free_list_size += 1;
		entry->ie_bits &= IE_BITS_GEN_MASK;
		entry->ie_next_free = space->is_free_list;
		space->is_free_list = entry;
	} else {
		rdxtree_remove(&space->is_map, (rdxtree_key_t) name);
		ie_free(entry);
	}
	space->is_size -= 1;
}


#if	MACH_KDB
#include <ddb/db_output.h>
#include <kern/task.h>

#define	printf	kdbprintf

ipc_entry_t
db_ipc_object_by_name(
	const task_t	task,
	mach_port_t	name)
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
