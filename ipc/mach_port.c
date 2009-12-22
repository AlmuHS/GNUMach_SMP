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
 *	File:	ipc/mach_port.c
 *	Author:	Rich Draves
 *	Date: 	1989
 *
 *	Exported kernel calls.  See mach/mach_port.defs.
 */

#include <kern/debug.h>
#include <kern/printf.h>
#include <mach/port.h>
#include <mach/kern_return.h>
#include <mach/notify.h>
#include <mach/mach_param.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#ifdef MIGRATING_THREADS
#include <mach/rpc.h>
#include <kern/task.h>
#include <kern/act.h>
#endif /* MIGRATING_THREADS */
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_user.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_right.h>
#include <ipc/mach_port.h>



/*
 *	Routine:	mach_port_names_helper
 *	Purpose:
 *		A helper function for mach_port_names.
 */

void
mach_port_names_helper(
	ipc_port_timestamp_t	timestamp,
	ipc_entry_t		entry,
	mach_port_t		name,
	mach_port_t		*names,
	mach_port_type_t	*types,
	ipc_entry_num_t		*actualp)
{
	ipc_entry_bits_t bits = entry->ie_bits;
	ipc_port_request_index_t request = entry->ie_request;
	mach_port_type_t type;
	ipc_entry_num_t actual;

	if (bits & MACH_PORT_TYPE_SEND_RIGHTS) {
		ipc_port_t port;
		boolean_t died;

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		/*
		 *	The timestamp serializes mach_port_names
		 *	with ipc_port_destroy.  If the port died,
		 *	but after mach_port_names started, pretend
		 *	that it isn't dead.
		 */

		ip_lock(port);
		died = (!ip_active(port) &&
			IP_TIMESTAMP_ORDER(port->ip_timestamp, timestamp));
		ip_unlock(port);

		if (died) {
			/* pretend this is a dead-name entry */

			bits &= ~(IE_BITS_TYPE_MASK|IE_BITS_MAREQUEST);
			bits |= MACH_PORT_TYPE_DEAD_NAME;
			if (request != 0)
				bits++;
			request = 0;
		}
	}

	type = IE_BITS_TYPE(bits);
	if (request != 0)
		type |= MACH_PORT_TYPE_DNREQUEST;
	if (bits & IE_BITS_MAREQUEST)
		type |= MACH_PORT_TYPE_MAREQUEST;

	actual = *actualp;
	names[actual] = name;
	types[actual] = type;
	*actualp = actual+1;
}

/*
 *	Routine:	mach_port_names [kernel call]
 *	Purpose:
 *		Retrieves a list of the rights present in the space,
 *		along with type information.  (Same as returned
 *		by mach_port_type.)  The names are returned in
 *		no particular order, but they (and the type info)
 *		are an accurate snapshot of the space.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Arrays of names and types returned.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
mach_port_names(
	ipc_space_t		space,
	mach_port_t		**namesp,
	mach_msg_type_number_t	*namesCnt,
	mach_port_type_t	**typesp,
	mach_msg_type_number_t	*typesCnt)
{
	ipc_tree_entry_t tentry;
	ipc_entry_t table;
	ipc_entry_num_t tsize;
	mach_port_index_t index;
	ipc_entry_num_t actual;	/* this many names */
	ipc_port_timestamp_t timestamp;	/* logical time of this operation */
	mach_port_t *names;
	mach_port_type_t *types;
	kern_return_t kr;

	vm_size_t size;		/* size of allocated memory */
	vm_offset_t addr1;	/* allocated memory, for names */
	vm_offset_t addr2;	/* allocated memory, for types */
	vm_map_copy_t memory1;	/* copied-in memory, for names */
	vm_map_copy_t memory2;	/* copied-in memory, for types */

	/* safe simplifying assumption */
	assert_static(sizeof(mach_port_t) == sizeof(mach_port_type_t));

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	size = 0;

	for (;;) {
		ipc_entry_num_t bound;
		vm_size_t size_needed;

		is_read_lock(space);
		if (!space->is_active) {
			is_read_unlock(space);
			if (size != 0) {
				kmem_free(ipc_kernel_map, addr1, size);
				kmem_free(ipc_kernel_map, addr2, size);
			}
			return KERN_INVALID_TASK;
		}

		/* upper bound on number of names in the space */

		bound = space->is_table_size + space->is_tree_total;
		size_needed = round_page(bound * sizeof(mach_port_t));

		if (size_needed <= size)
			break;

		is_read_unlock(space);

		if (size != 0) {
			kmem_free(ipc_kernel_map, addr1, size);
			kmem_free(ipc_kernel_map, addr2, size);
		}
		size = size_needed;

		kr = vm_allocate(ipc_kernel_map, &addr1, size, TRUE);
		if (kr != KERN_SUCCESS)
			return KERN_RESOURCE_SHORTAGE;

		kr = vm_allocate(ipc_kernel_map, &addr2, size, TRUE);
		if (kr != KERN_SUCCESS) {
			kmem_free(ipc_kernel_map, addr1, size);
			return KERN_RESOURCE_SHORTAGE;
		}

		/* can't fault while we hold locks */

		kr = vm_map_pageable(ipc_kernel_map, addr1, addr1 + size,
				     VM_PROT_READ|VM_PROT_WRITE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_pageable(ipc_kernel_map, addr2, addr2 + size,
				     VM_PROT_READ|VM_PROT_WRITE);
		assert(kr == KERN_SUCCESS);
	}
	/* space is read-locked and active */

	names = (mach_port_t *) addr1;
	types = (mach_port_type_t *) addr2;
	actual = 0;

	timestamp = ipc_port_timestamp();

	table = space->is_table;
	tsize = space->is_table_size;

	for (index = 0; index < tsize; index++) {
		ipc_entry_t entry = &table[index];
		ipc_entry_bits_t bits = entry->ie_bits;

		if (IE_BITS_TYPE(bits) != MACH_PORT_TYPE_NONE) {
			mach_port_t name = MACH_PORT_MAKEB(index, bits);

			mach_port_names_helper(timestamp, entry, name,
					       names, types, &actual);
		}
	}

	for (tentry = ipc_splay_traverse_start(&space->is_tree);
	     tentry != ITE_NULL;
	     tentry = ipc_splay_traverse_next(&space->is_tree, FALSE)) {
		ipc_entry_t entry = &tentry->ite_entry;
		mach_port_t name = tentry->ite_name;

		assert(IE_BITS_TYPE(tentry->ite_bits) != MACH_PORT_TYPE_NONE);

		mach_port_names_helper(timestamp, entry, name,
				       names, types, &actual);
	}
	ipc_splay_traverse_finish(&space->is_tree);
	is_read_unlock(space);

	if (actual == 0) {
		memory1 = VM_MAP_COPY_NULL;
		memory2 = VM_MAP_COPY_NULL;

		if (size != 0) {
			kmem_free(ipc_kernel_map, addr1, size);
			kmem_free(ipc_kernel_map, addr2, size);
		}
	} else {
		vm_size_t size_used;

		size_used = round_page(actual * sizeof(mach_port_t));

		/*
		 *	Make used memory pageable and get it into
		 *	copied-in form.  Free any unused memory.
		 */

		kr = vm_map_pageable(ipc_kernel_map,
				     addr1, addr1 + size_used,
				     VM_PROT_NONE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_pageable(ipc_kernel_map,
				     addr2, addr2 + size_used,
				     VM_PROT_NONE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_copyin(ipc_kernel_map, addr1, size_used,
				   TRUE, &memory1);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_copyin(ipc_kernel_map, addr2, size_used,
				   TRUE, &memory2);
		assert(kr == KERN_SUCCESS);

		if (size_used != size) {
			kmem_free(ipc_kernel_map,
				  addr1 + size_used, size - size_used);
			kmem_free(ipc_kernel_map,
				  addr2 + size_used, size - size_used);
		}
	}

	*namesp = (mach_port_t *) memory1;
	*namesCnt = actual;
	*typesp = (mach_port_type_t *) memory2;
	*typesCnt = actual;
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_type [kernel call]
 *	Purpose:
 *		Retrieves the type of a right in the space.
 *		The type is a bitwise combination of one or more
 *		of the following type bits:
 *			MACH_PORT_TYPE_SEND
 *			MACH_PORT_TYPE_RECEIVE
 *			MACH_PORT_TYPE_SEND_ONCE
 *			MACH_PORT_TYPE_PORT_SET
 *			MACH_PORT_TYPE_DEAD_NAME
 *		In addition, the following pseudo-type bits may be present:
 *			MACH_PORT_TYPE_DNREQUEST
 *				A dead-name notification is requested.
 *			MACH_PORT_TYPE_MAREQUEST
 *				The send/receive right is blocked;
 *				a msg-accepted notification is outstanding.
 *			MACH_PORT_TYPE_COMPAT
 *				This is a compatibility-mode right;
 *				when the port dies, it will disappear
 *				instead of turning into a dead-name.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Type is returned.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 */

kern_return_t
mach_port_type(
	ipc_space_t		space,
	mach_port_t		name,
	mach_port_type_t	*typep)
{
	mach_port_urefs_t urefs;
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS)
		return kr;
	/* space is write-locked and active */

	kr = ipc_right_info(space, name, entry, typep, &urefs);
	if (kr == KERN_SUCCESS)
		is_write_unlock(space);
	/* space is unlocked */
	return kr;
}

/*
 *	Routine:	mach_port_rename [kernel call]
 *	Purpose:
 *		Changes the name denoting a right,
 *		from oname to nname.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The right is renamed.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The oname doesn't denote a right.
 *		KERN_INVALID_VALUE	The nname isn't a legal name.
 *		KERN_NAME_EXISTS	The nname already denotes a right.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
mach_port_rename(
	ipc_space_t	space,
	mach_port_t	oname,
	mach_port_t	nname)
{
	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	if (!MACH_PORT_VALID(nname))
		return KERN_INVALID_VALUE;

	return ipc_object_rename(space, oname, nname);
}

/*
 *	Routine:	mach_port_allocate_name [kernel call]
 *	Purpose:
 *		Allocates a right in a space, using a specific name
 *		for the new right.  Possible rights:
 *			MACH_PORT_RIGHT_RECEIVE
 *			MACH_PORT_RIGHT_PORT_SET
 *			MACH_PORT_RIGHT_DEAD_NAME
 *
 *		A new port (allocated with MACH_PORT_RIGHT_RECEIVE)
 *		has no extant send or send-once rights and no queued
 *		messages.  Its queue limit is MACH_PORT_QLIMIT_DEFAULT
 *		and its make-send count is 0.  It is not a member of
 *		a port set.  It has no registered no-senders or
 *		port-destroyed notification requests.
 *
 *		A new port set has no members.
 *
 *		A new dead name has one user reference.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The right is allocated.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	The name isn't a legal name.
 *		KERN_INVALID_VALUE	"right" isn't a legal kind of right.
 *		KERN_NAME_EXISTS	The name already denotes a right.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
mach_port_allocate_name(space, right, name)
	ipc_space_t space;
	mach_port_right_t right;
	mach_port_t name;
{
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	if (!MACH_PORT_VALID(name))
		return KERN_INVALID_VALUE;

	switch (right) {
	    case MACH_PORT_RIGHT_RECEIVE: {
		ipc_port_t port;

		kr = ipc_port_alloc_name(space, name, &port);
		if (kr == KERN_SUCCESS)
			ip_unlock(port);
		break;
	    }

	    case MACH_PORT_RIGHT_PORT_SET: {
		ipc_pset_t pset;

		kr = ipc_pset_alloc_name(space, name, &pset);
		if (kr == KERN_SUCCESS)
			ips_unlock(pset);
		break;
	    }

	    case MACH_PORT_RIGHT_DEAD_NAME:
		kr = ipc_object_alloc_dead_name(space, name);
		break;

	    default:
		kr = KERN_INVALID_VALUE;
		break;
	}

	return kr;
}

/*
 *	Routine:	mach_port_allocate [kernel call]
 *	Purpose:
 *		Allocates a right in a space.  Like mach_port_allocate_name,
 *		except that the implementation picks a name for the right.
 *		The name may be any legal name in the space that doesn't
 *		currently denote a right.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The right is allocated.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	"right" isn't a legal kind of right.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 *		KERN_NO_SPACE		No room in space for another right.
 */

kern_return_t
mach_port_allocate(space, right, namep)
	ipc_space_t space;
	mach_port_right_t right;
	mach_port_t *namep;
{
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	switch (right) {
	    case MACH_PORT_RIGHT_RECEIVE: {
		ipc_port_t port;

		kr = ipc_port_alloc(space, namep, &port);
		if (kr == KERN_SUCCESS)
			ip_unlock(port);
		break;
	    }

	    case MACH_PORT_RIGHT_PORT_SET: {
		ipc_pset_t pset;

		kr = ipc_pset_alloc(space, namep, &pset);
		if (kr == KERN_SUCCESS)
			ips_unlock(pset);
		break;
	    }

	    case MACH_PORT_RIGHT_DEAD_NAME:
		kr = ipc_object_alloc_dead(space, namep);
		break;

	    default:
		kr = KERN_INVALID_VALUE;
		break;
	}

	return (kr);
}

/*
 *	Routine:	mach_port_destroy [kernel call]
 *	Purpose:
 *		Cleans up and destroys all rights denoted by a name
 *		in a space.  The destruction of a receive right
 *		destroys the port, unless a port-destroyed request
 *		has been made for it; the destruction of a port-set right
 *		destroys the port set.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The name is destroyed.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 */

kern_return_t
mach_port_destroy(
	ipc_space_t	space,
	mach_port_t	name)
{
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS)
		return kr;
	/* space is write-locked and active */

	kr = ipc_right_destroy(space, name, entry); /* unlocks space */
	return kr;
}

/*
 *	Routine:	mach_port_deallocate [kernel call]
 *	Purpose:
 *		Deallocates a user reference from a send right,
 *		send-once right, or a dead-name right.  May
 *		deallocate the right, if this is the last uref,
 *		and destroy the name, if it doesn't denote
 *		other rights.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The uref is deallocated.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	The right isn't correct.
 */

static volatile int mach_port_deallocate_debug = 0;
kern_return_t
mach_port_deallocate(
	ipc_space_t	space,
	mach_port_t	name)
{
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS) {
		if (name != MACH_PORT_NULL && name != MACH_PORT_DEAD) {
			printf("task %p deallocating an invalid port %u, most probably a bug.\n", current_task(), name);
			if (mach_port_deallocate_debug)
				SoftDebugger("mach_port_deallocate");
		}
		return kr;
	}
	/* space is write-locked */

	kr = ipc_right_dealloc(space, name, entry); /* unlocks space */
	return kr;
}

/*
 *	Routine:	mach_port_get_refs [kernel call]
 *	Purpose:
 *		Retrieves the number of user references held by a right.
 *		Receive rights, port-set rights, and send-once rights
 *		always have one user reference.  Returns zero if the
 *		name denotes a right, but not the queried right.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Number of urefs returned.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	"right" isn't a legal value.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 */

kern_return_t
mach_port_get_refs(
	ipc_space_t		space,
	mach_port_t		name,
	mach_port_right_t	right,
	mach_port_urefs_t	*urefsp)
{
	mach_port_type_t type;
	mach_port_urefs_t urefs;
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	if (right >= MACH_PORT_RIGHT_NUMBER)
		return KERN_INVALID_VALUE;

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS)
		return kr;
	/* space is write-locked and active */

	kr = ipc_right_info(space, name, entry, &type, &urefs);	/* unlocks */
	if (kr != KERN_SUCCESS)
		return kr;	/* space is unlocked */
	is_write_unlock(space);

	if (type & MACH_PORT_TYPE(right))
		switch (right) {
		    case MACH_PORT_RIGHT_SEND_ONCE:
			assert(urefs == 1);
			/* fall-through */

		    case MACH_PORT_RIGHT_PORT_SET:
		    case MACH_PORT_RIGHT_RECEIVE:
			*urefsp = 1;
			break;

		    case MACH_PORT_RIGHT_DEAD_NAME:
		    case MACH_PORT_RIGHT_SEND:
			assert(urefs > 0);
			*urefsp = urefs;
			break;

		    default:
			panic("mach_port_get_refs: strange rights");
		}
	else
		*urefsp = 0;

	return kr;
}

/*
 *	Routine:	mach_port_mod_refs
 *	Purpose:
 *		Modifies the number of user references held by a right.
 *		The resulting number of user references must be non-negative.
 *		If it is zero, the right is deallocated.  If the name
 *		doesn't denote other rights, it is destroyed.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Modified number of urefs.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	"right" isn't a legal value.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote specified right.
 *		KERN_INVALID_VALUE	Impossible modification to urefs.
 *		KERN_UREFS_OVERFLOW	Urefs would overflow.
 */

kern_return_t
mach_port_mod_refs(
	ipc_space_t		space,
	mach_port_t		name,
	mach_port_right_t	right,
	mach_port_delta_t	delta)
{
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	if (right >= MACH_PORT_RIGHT_NUMBER)
		return KERN_INVALID_VALUE;

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS)
		return kr;
	/* space is write-locked and active */

	kr = ipc_right_delta(space, name, entry, right, delta);	/* unlocks */
	return kr;
}

/*
 *	Routine:	old_mach_port_get_receive_status [kernel call]
 *	Purpose:
 *		Compatibility for code written before sequence numbers.
 *		Retrieves mucho info about a receive right.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Retrieved status.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_get_receive_status(ipc_space_t, mach_port_t, mach_port_status_t *);
kern_return_t
old_mach_port_get_receive_status(space, name, statusp)
	ipc_space_t space;
	mach_port_t name;
	old_mach_port_status_t *statusp;
{
	mach_port_status_t status;
	kern_return_t kr;

	kr = mach_port_get_receive_status(space, name, &status);
	if (kr != KERN_SUCCESS)
		return kr;

	statusp->mps_pset = status.mps_pset;
	statusp->mps_mscount = status.mps_mscount;
	statusp->mps_qlimit = status.mps_qlimit;
	statusp->mps_msgcount = status.mps_msgcount;
	statusp->mps_sorights = status.mps_sorights;
	statusp->mps_srights = status.mps_srights;
	statusp->mps_pdrequest = status.mps_pdrequest;
	statusp->mps_nsrequest = status.mps_nsrequest;

	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_set_qlimit [kernel call]
 *	Purpose:
 *		Changes a receive right's queue limit.
 *		The new queue limit must be between 0 and
 *		MACH_PORT_QLIMIT_MAX, inclusive.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Set queue limit.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 *		KERN_INVALID_VALUE	Illegal queue limit.
 */

kern_return_t
mach_port_set_qlimit(space, name, qlimit)
	ipc_space_t space;
	mach_port_t name;
	mach_port_msgcount_t qlimit;
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	if (qlimit > MACH_PORT_QLIMIT_MAX)
		return KERN_INVALID_VALUE;

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS)
		return kr;
	/* port is locked and active */

	ipc_port_set_qlimit(port, qlimit);

	ip_unlock(port);
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_set_mscount [kernel call]
 *	Purpose:
 *		Changes a receive right's make-send count.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Set make-send count.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_set_mscount(
	ipc_space_t		space,
	mach_port_t		name,
	mach_port_mscount_t	mscount)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS)
		return kr;
	/* port is locked and active */

	ipc_port_set_mscount(port, mscount);

	ip_unlock(port);
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_set_seqno [kernel call]
 *	Purpose:
 *		Changes a receive right's sequence number.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Set sequence number.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_set_seqno(
	ipc_space_t		space,
	mach_port_t		name,
	mach_port_seqno_t	seqno)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS)
		return kr;
	/* port is locked and active */

	ipc_port_set_seqno(port, seqno);

	ip_unlock(port);
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_gst_helper
 *	Purpose:
 *		A helper function for mach_port_get_set_status.
 */

void
mach_port_gst_helper(
	ipc_pset_t		pset,
	ipc_port_t		port,
	ipc_entry_num_t		maxnames,
	mach_port_t		*names,
	ipc_entry_num_t		*actualp)
{
	ipc_pset_t ip_pset;
	mach_port_t name;

	assert(port != IP_NULL);

	ip_lock(port);
	assert(ip_active(port));

	name = port->ip_receiver_name;
	assert(name != MACH_PORT_NULL);
	ip_pset = port->ip_pset;

	ip_unlock(port);

	if (pset == ip_pset) {
		ipc_entry_num_t actual = *actualp;

		if (actual < maxnames)
			names[actual] = name;

		*actualp = actual+1;
	}
}

/*
 *	Routine:	mach_port_get_set_status [kernel call]
 *	Purpose:
 *		Retrieves a list of members in a port set.
 *		Returns the space's name for each receive right member.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Retrieved list of members.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote a port set.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
mach_port_get_set_status(
	ipc_space_t			space,
	mach_port_t			name,
	mach_port_t			**members,
	mach_msg_type_number_t		*membersCnt)
{
	ipc_entry_num_t actual;		/* this many members */
	ipc_entry_num_t maxnames;	/* space for this many members */
	kern_return_t kr;

	vm_size_t size;		/* size of allocated memory */
	vm_offset_t addr;	/* allocated memory */
	vm_map_copy_t memory;	/* copied-in memory */

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	size = PAGE_SIZE;	/* initial guess */

	for (;;) {
		ipc_tree_entry_t tentry;
		ipc_entry_t entry, table;
		ipc_entry_num_t tsize;
		mach_port_index_t index;
		mach_port_t *names;
		ipc_pset_t pset;

		kr = vm_allocate(ipc_kernel_map, &addr, size, TRUE);
		if (kr != KERN_SUCCESS)
			return KERN_RESOURCE_SHORTAGE;

		/* can't fault while we hold locks */

		kr = vm_map_pageable(ipc_kernel_map, addr, addr + size,
				     VM_PROT_READ|VM_PROT_WRITE);
		assert(kr == KERN_SUCCESS);

		kr = ipc_right_lookup_read(space, name, &entry);
		if (kr != KERN_SUCCESS) {
			kmem_free(ipc_kernel_map, addr, size);
			return kr;
		}
		/* space is read-locked and active */

		if (IE_BITS_TYPE(entry->ie_bits) != MACH_PORT_TYPE_PORT_SET) {
			is_read_unlock(space);
			kmem_free(ipc_kernel_map, addr, size);
			return KERN_INVALID_RIGHT;
		}

		pset = (ipc_pset_t) entry->ie_object;
		assert(pset != IPS_NULL);
		/* the port set must be active */

		names = (mach_port_t *) addr;
		maxnames = size / sizeof(mach_port_t);
		actual = 0;

		table = space->is_table;
		tsize = space->is_table_size;

		for (index = 0; index < tsize; index++) {
			ipc_entry_t ientry = &table[index];
			ipc_entry_bits_t bits = ientry->ie_bits;

			if (bits & MACH_PORT_TYPE_RECEIVE) {
				ipc_port_t port =
					(ipc_port_t) ientry->ie_object;

				mach_port_gst_helper(pset, port, maxnames,
						     names, &actual);
			}
		}

		for (tentry = ipc_splay_traverse_start(&space->is_tree);
		     tentry != ITE_NULL;
		     tentry = ipc_splay_traverse_next(&space->is_tree,FALSE)) {
			ipc_entry_bits_t bits = tentry->ite_bits;

			assert(IE_BITS_TYPE(bits) != MACH_PORT_TYPE_NONE);

			if (bits & MACH_PORT_TYPE_RECEIVE) {
				ipc_port_t port =
					(ipc_port_t) tentry->ite_object;

				mach_port_gst_helper(pset, port, maxnames,
						     names, &actual);
			}
		}
		ipc_splay_traverse_finish(&space->is_tree);
		is_read_unlock(space);

		if (actual <= maxnames)
			break;

		/* didn't have enough memory; allocate more */

		kmem_free(ipc_kernel_map, addr, size);
		size = round_page(actual * sizeof(mach_port_t)) + PAGE_SIZE;
	}

	if (actual == 0) {
		memory = VM_MAP_COPY_NULL;

		kmem_free(ipc_kernel_map, addr, size);
	} else {
		vm_size_t size_used;

		size_used = round_page(actual * sizeof(mach_port_t));

		/*
		 *	Make used memory pageable and get it into
		 *	copied-in form.  Free any unused memory.
		 */

		kr = vm_map_pageable(ipc_kernel_map,
				     addr, addr + size_used,
				     VM_PROT_NONE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_copyin(ipc_kernel_map, addr, size_used,
				   TRUE, &memory);
		assert(kr == KERN_SUCCESS);

		if (size_used != size)
			kmem_free(ipc_kernel_map,
				  addr + size_used, size - size_used);
	}

	*members = (mach_port_t *) memory;
	*membersCnt = actual;
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_move_member [kernel call]
 *	Purpose:
 *		If after is MACH_PORT_NULL, removes member
 *		from the port set it is in.  Otherwise, adds
 *		member to after, removing it from any set
 *		it might already be in.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Moved the port.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	Member didn't denote a right.
 *		KERN_INVALID_RIGHT	Member didn't denote a receive right.
 *		KERN_INVALID_NAME	After didn't denote a right.
 *		KERN_INVALID_RIGHT	After didn't denote a port set right.
 *		KERN_NOT_IN_SET
 *			After is MACH_PORT_NULL and Member isn't in a port set.
 */

kern_return_t
mach_port_move_member(
	ipc_space_t	space,
	mach_port_t	member,
	mach_port_t	after)
{
	ipc_entry_t entry;
	ipc_port_t port;
	ipc_pset_t nset;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_right_lookup_read(space, member, &entry);
	if (kr != KERN_SUCCESS)
		return kr;
	/* space is read-locked and active */

	if ((entry->ie_bits & MACH_PORT_TYPE_RECEIVE) == 0) {
		is_read_unlock(space);
		return KERN_INVALID_RIGHT;
	}

	port = (ipc_port_t) entry->ie_object;
	assert(port != IP_NULL);

	if (after == MACH_PORT_NULL)
		nset = IPS_NULL;
	else {
		entry = ipc_entry_lookup(space, after);
		if (entry == IE_NULL) {
			is_read_unlock(space);
			return KERN_INVALID_NAME;
		}

		if ((entry->ie_bits & MACH_PORT_TYPE_PORT_SET) == 0) {
			is_read_unlock(space);
			return KERN_INVALID_RIGHT;
		}

		nset = (ipc_pset_t) entry->ie_object;
		assert(nset != IPS_NULL);
	}

	kr = ipc_pset_move(space, port, nset);
	/* space is unlocked */
	return kr;
}

/*
 *	Routine:	mach_port_request_notification [kernel call]
 *	Purpose:
 *		Requests a notification.  The caller supplies
 *		a send-once right for the notification to use,
 *		and the call returns the previously registered
 *		send-once right, if any.  Possible types:
 *
 *		MACH_NOTIFY_PORT_DESTROYED
 *			Requests a port-destroyed notification
 *			for a receive right.  Sync should be zero.
 *		MACH_NOTIFY_NO_SENDERS
 *			Requests a no-senders notification for a
 *			receive right.  If there are currently no
 *			senders, sync is less than or equal to the
 *			current make-send count, and a send-once right
 *			is supplied, then an immediate no-senders
 *			notification is generated.
 *		MACH_NOTIFY_DEAD_NAME
 *			Requests a dead-name notification for a send
 *			or receive right.  If the name is already a
 *			dead name, sync is non-zero, and a send-once
 *			right is supplied, then an immediate dead-name
 *			notification is generated.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Requested a notification.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	Bad id value.
 *		KERN_INVALID_NAME	Name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote appropriate right.
 *		KERN_INVALID_CAPABILITY	The notify port is dead.
 *	MACH_NOTIFY_PORT_DESTROYED:
 *		KERN_INVALID_VALUE	Sync isn't zero.
 *	MACH_NOTIFY_DEAD_NAME:
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 *		KERN_INVALID_ARGUMENT	Name denotes dead name, but
 *			sync is zero or notify is IP_NULL.
 *		KERN_UREFS_OVERFLOW	Name denotes dead name, but
 *			generating immediate notif. would overflow urefs.
 */

kern_return_t
mach_port_request_notification(
	ipc_space_t		space,
	mach_port_t		name,
	mach_msg_id_t		id,
	mach_port_mscount_t	sync,
	ipc_port_t		notify,
	ipc_port_t		*previousp)
{
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	if (notify == IP_DEAD)
		return KERN_INVALID_CAPABILITY;

	switch (id) {
	    case MACH_NOTIFY_PORT_DESTROYED: {
		ipc_port_t port, previous;

		if (sync != 0)
			return KERN_INVALID_VALUE;

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS)
			return kr;
		/* port is locked and active */

		ipc_port_pdrequest(port, notify, &previous);
		/* port is unlocked */

		*previousp = previous;
		break;
	    }

	    case MACH_NOTIFY_NO_SENDERS: {
		ipc_port_t port;

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS)
			return kr;
		/* port is locked and active */

		ipc_port_nsrequest(port, sync, notify, previousp);
		/* port is unlocked */
		break;
	    }

	    case MACH_NOTIFY_DEAD_NAME:
		kr = ipc_right_dnrequest(space, name, sync != 0,
					 notify, previousp);
		if (kr != KERN_SUCCESS)
			return kr;
		break;

	    default:
		return KERN_INVALID_VALUE;
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_insert_right [kernel call]
 *	Purpose:
 *		Inserts a right into a space, as if the space
 *		voluntarily received the right in a message,
 *		except that the right gets the specified name.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Inserted the right.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	The name isn't a legal name.
 *		KERN_NAME_EXISTS	The name already denotes a right.
 *		KERN_INVALID_VALUE	Message doesn't carry a port right.
 *		KERN_INVALID_CAPABILITY	Port is null or dead.
 *		KERN_UREFS_OVERFLOW	Urefs limit would be exceeded.
 *		KERN_RIGHT_EXISTS	Space has rights under another name.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
mach_port_insert_right(
	ipc_space_t		space,
	mach_port_t		name,
	ipc_port_t		poly,
	mach_msg_type_name_t	polyPoly)
{
	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	if (!MACH_PORT_VALID(name) ||
	    !MACH_MSG_TYPE_PORT_ANY_RIGHT(polyPoly))
		return KERN_INVALID_VALUE;

	if (!IO_VALID((ipc_object_t)poly))
		return KERN_INVALID_CAPABILITY;

	return ipc_object_copyout_name(space, (ipc_object_t)poly,
				       polyPoly, FALSE, name);
}

/*
 *	Routine:	mach_port_extract_right [kernel call]
 *	Purpose:
 *		Extracts a right from a space, as if the space
 *		voluntarily sent the right to the caller.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Extracted the right.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	Requested type isn't a port right.
 *		KERN_INVALID_NAME	Name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote appropriate right.
 */

kern_return_t
mach_port_extract_right(
	ipc_space_t		space,
	mach_port_t		name,
	mach_msg_type_name_t	msgt_name,
	ipc_port_t		*poly,
	mach_msg_type_name_t	*polyPoly)
{
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	if (!MACH_MSG_TYPE_PORT_ANY(msgt_name))
		return KERN_INVALID_VALUE;

	kr = ipc_object_copyin(space, name, msgt_name, (ipc_object_t *) poly);

	if (kr == KERN_SUCCESS)
		*polyPoly = ipc_object_copyin_type(msgt_name);
	return kr;
}

/*
 *	Routine:	mach_port_get_receive_status [kernel call]
 *	Purpose:
 *		Retrieves mucho info about a receive right.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Retrieved status.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_get_receive_status(space, name, statusp)
	ipc_space_t space;
	mach_port_t name;
	mach_port_status_t *statusp;
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS)
		return kr;
	/* port is locked and active */

	if (port->ip_pset != IPS_NULL) {
		ipc_pset_t pset = port->ip_pset;

		ips_lock(pset);
		if (!ips_active(pset)) {
			ipc_pset_remove(pset, port);
			ips_check_unlock(pset);
			goto no_port_set;
		} else {
			statusp->mps_pset = pset->ips_local_name;
			imq_lock(&pset->ips_messages);
			statusp->mps_seqno = port->ip_seqno;
			imq_unlock(&pset->ips_messages);
			ips_unlock(pset);
			assert(MACH_PORT_VALID(statusp->mps_pset));
		}
	} else {
	    no_port_set:
		statusp->mps_pset = MACH_PORT_NULL;
		imq_lock(&port->ip_messages);
		statusp->mps_seqno = port->ip_seqno;
		imq_unlock(&port->ip_messages);
	}

	statusp->mps_mscount = port->ip_mscount;
	statusp->mps_qlimit = port->ip_qlimit;
	statusp->mps_msgcount = port->ip_msgcount;
	statusp->mps_sorights = port->ip_sorights;
	statusp->mps_srights = port->ip_srights > 0;
	statusp->mps_pdrequest = port->ip_pdrequest != IP_NULL;
	statusp->mps_nsrequest = port->ip_nsrequest != IP_NULL;
	ip_unlock(port);

	return KERN_SUCCESS;
}

#ifdef MIGRATING_THREADS
kern_return_t
mach_port_set_rpcinfo(space, name, rpc_info, rpc_info_count)
	ipc_space_t space;
	mach_port_t name;
	void *rpc_info;
	unsigned int rpc_info_count;
{
	ipc_target_t target;
	ipc_object_t object;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_object_translate(space, name,
				  MACH_PORT_RIGHT_PORT_SET, &object);
	if (kr == KERN_SUCCESS)
		target = &((ipc_pset_t)object)->ips_target;
	else {
		kr = ipc_object_translate(space, name,
					  MACH_PORT_RIGHT_RECEIVE, &object);
		if (kr != KERN_SUCCESS)
			return kr;
		target = &((ipc_port_t)object)->ip_target;
	}

	/* port/pset is locked and active */

	kr = port_machine_set_rpcinfo(target, rpc_info, rpc_info_count);

	io_unlock(object);

	return kr;
}

#if 1
int sacts, maxsacts;
#endif

sact_count()
{
	printf("%d server activations in use, %d max\n", sacts, maxsacts);
}

kern_return_t
mach_port_create_act(task, name, user_stack, user_rbuf, user_rbuf_size, out_act)
	task_t task;
	mach_port_t name;
	vm_offset_t user_stack;
	vm_offset_t user_rbuf;
	vm_size_t user_rbuf_size;
	Act **out_act;
{
	ipc_target_t target;
	ipc_space_t space;
	ipc_object_t object;
	kern_return_t kr;
	Act *act;

	if (task == 0)
		return KERN_INVALID_TASK;

	/* First create the new activation.  */
	kr = act_create(task, user_stack, user_rbuf, user_rbuf_size, &act);
	if (kr != KERN_SUCCESS)
		return kr;

	space = task->itk_space;

	kr = ipc_object_translate(space, name,
				  MACH_PORT_RIGHT_PORT_SET, &object);
	if (kr == KERN_SUCCESS)
		target = &((ipc_pset_t)object)->ips_target;
	else {
		kr = ipc_object_translate(space, name,
					  MACH_PORT_RIGHT_RECEIVE, &object);
		if (kr != KERN_SUCCESS) {
			act_terminate(act);
			act_deallocate(act);
			return kr;
		}
		target = &((ipc_port_t)object)->ip_target;
	}

	/* port/pset is locked and active */
#if 0
	printf("act port/pset %08x ipc_target %08x stack %08x act %08x\n",
	       object, target, user_stack, act);
#endif

	/* Assign the activation to the port's actpool.  */
	kr = act_set_target(act, target);
	if (kr != KERN_SUCCESS) {
		io_unlock(object);
		act_terminate(act);
		act_deallocate(act);
		return kr;
	}
#if 0
	printf(" actpool %08x act %08x\n", target->ip_actpool, act);
#endif

	io_unlock(object);

	/* Pass our reference to the activation back to the user.  */
	*out_act = act;

#if 1
	sacts++;
	if (sacts > maxsacts)
		maxsacts = sacts;
	act->mact.pcb->ss.mpsfu_high = 0x69;
#endif
	return KERN_SUCCESS;
}

#ifdef RPCKERNELSIG
kern_return_t
mach_port_set_syscall_right(task, name)
	task_t task;
	mach_port_t name;
{
	ipc_entry_t entry;
	ipc_port_t port;
	kern_return_t kr;

	if (task == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_right_lookup_write(task, name, &entry);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	if (!(entry->ie_bits & MACH_PORT_TYPE(MACH_PORT_RIGHT_SEND))) {
		is_write_unlock(space);
		return KERN_INVALID_RIGHT;
	}

	task->syscall_ipc_entry = *entry;

	is_write_unlock(space);

	return KERN_SUCCESS;
}
#endif
#endif /* MIGRATING_THREADS */
