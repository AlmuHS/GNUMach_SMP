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
 */
/*
 *	File:	ipc/ipc_space.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Functions to manipulate IPC capability spaces.
 */

#include <string.h>

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/port.h>
#include <kern/assert.h>
#include <kern/sched_prim.h>
#include <kern/slab.h>
#include <ipc/port.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_table.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_right.h>



struct kmem_cache ipc_space_cache;
ipc_space_t ipc_space_kernel;
ipc_space_t ipc_space_reply;

/*
 *	Routine:	ipc_space_reference
 *	Routine:	ipc_space_release
 *	Purpose:
 *		Function versions of the IPC space macros.
 *		The "is_" cover macros can be defined to use the
 *		macros or the functions, as desired.
 */

void
ipc_space_reference(
	ipc_space_t	space)
{
	ipc_space_reference_macro(space);
}

void
ipc_space_release(
	ipc_space_t	space)
{
	ipc_space_release_macro(space);
}

/* A place-holder object for the zeroth entry.  */
struct ipc_entry zero_entry;

/*
 *	Routine:	ipc_space_create
 *	Purpose:
 *		Creates a new IPC space.
 *
 *		The new space has two references, one for the caller
 *		and one because it is active.
 *	Conditions:
 *		Nothing locked.  Allocates memory.
 *	Returns:
 *		KERN_SUCCESS		Created a space.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
ipc_space_create(
	ipc_space_t		*spacep)
{
	ipc_space_t space;

	space = is_alloc();
	if (space == IS_NULL)
		return KERN_RESOURCE_SHORTAGE;

	is_ref_lock_init(space);
	space->is_references = 2;

	is_lock_init(space);
	space->is_active = TRUE;

	rdxtree_init(&space->is_map);
	rdxtree_init(&space->is_reverse_map);
	/* The zeroth entry is reserved.  */
	rdxtree_insert(&space->is_map, 0, &zero_entry);
	space->is_size = 1;
	space->is_free_list = NULL;
	space->is_free_list_size = 0;

	*spacep = space;
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_space_create_special
 *	Purpose:
 *		Create a special space.  A special space
 *		doesn't hold rights in the normal way.
 *		Instead it is place-holder for holding
 *		disembodied (naked) receive rights.
 *		See ipc_port_alloc_special/ipc_port_dealloc_special.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Created a space.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
ipc_space_create_special(
	ipc_space_t	*spacep)
{
	ipc_space_t space;

	space = is_alloc();
	if (space == IS_NULL)
		return KERN_RESOURCE_SHORTAGE;

	is_ref_lock_init(space);
	space->is_references = 1;

	is_lock_init(space);
	space->is_active = FALSE;

	*spacep = space;
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_space_destroy
 *	Purpose:
 *		Marks the space as dead and cleans up the entries.
 *		Does nothing if the space is already dead.
 *	Conditions:
 *		Nothing locked.
 */

void
ipc_space_destroy(
	ipc_space_t	space)
{
	boolean_t active;

	assert(space != IS_NULL);

	is_write_lock(space);
	active = space->is_active;
	space->is_active = FALSE;
	is_write_unlock(space);

	if (!active)
		return;

	ipc_entry_t entry;
	struct rdxtree_iter iter;
	rdxtree_for_each(&space->is_map, &iter, entry) {
		if (entry->ie_name == MACH_PORT_NULL)
			continue;

		mach_port_type_t type = IE_BITS_TYPE(entry->ie_bits);

		if (type != MACH_PORT_TYPE_NONE) {
			mach_port_name_t name =
				MACH_PORT_MAKEB(entry->ie_name, entry->ie_bits);

			ipc_right_clean(space, name, entry);
		}

		ie_free(entry);
	}
	rdxtree_remove_all(&space->is_map);
	rdxtree_remove_all(&space->is_reverse_map);

	/*
	 *	Because the space is now dead,
	 *	we must release the "active" reference for it.
	 *	Our caller still has his reference.
	 */

	is_release(space);
}
