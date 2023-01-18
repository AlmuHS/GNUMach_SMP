/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 */
/*
 *	File:	ipc/mach_debug.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Exported kernel calls.  See mach_debug/mach_debug.defs.
 */

#include <string.h>

#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/machine/vm_types.h>
#include <mach/vm_param.h>
#include <mach_debug/hash_info.h>
#include <kern/host.h>
#include <kern/mach_debug.server.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_marequest.h>
#include <ipc/ipc_table.h>
#include <ipc/ipc_right.h>



/*
 *	Routine:	mach_port_get_srights [kernel call]
 *	Purpose:
 *		Retrieve the number of extant send rights
 *		that a receive right has.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Retrieved number of send rights.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_get_srights(
	ipc_space_t		space,
	mach_port_name_t	name,
	mach_port_rights_t	*srightsp)
{
	ipc_port_t port;
	kern_return_t kr;
	mach_port_rights_t srights;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS)
		return kr;
	/* port is locked and active */

	srights = port->ip_srights;
	ip_unlock(port);

	*srightsp = srights;
	return KERN_SUCCESS;
}

/*
 *	Routine:	host_ipc_marequest_info
 *	Purpose:
 *		Return information about the marequest hash table.
 *	Conditions:
 *		Nothing locked.  Obeys CountInOut protocol.
 *	Returns:
 *		KERN_SUCCESS		Returned information.
 *		KERN_INVALID_HOST	The host is null.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
host_ipc_marequest_info(
	host_t 				host,
	unsigned int 			*maxp,
	hash_info_bucket_array_t 	*infop,
	unsigned int 			*countp)
{
	vm_offset_t addr;
	vm_size_t size = 0; /* '=0' to shut up lint */
	hash_info_bucket_t *info;
	unsigned int potential, actual;
	kern_return_t kr;

	if (host == HOST_NULL)
		return KERN_INVALID_HOST;

	/* start with in-line data */

	info = *infop;
	potential = *countp;

	for (;;) {
		actual = ipc_marequest_info(maxp, info, potential);
		if (actual <= potential)
			break;

		/* allocate more memory */

		if (info != *infop)
			kmem_free(ipc_kernel_map, addr, size);

		size = round_page(actual * sizeof *info);
		kr = kmem_alloc_pageable(ipc_kernel_map, &addr, size);
		if (kr != KERN_SUCCESS)
			return KERN_RESOURCE_SHORTAGE;

		info = (hash_info_bucket_t *) addr;
		potential = size/sizeof *info;
	}

	if (info == *infop) {
		/* data fit in-line; nothing to deallocate */

		*countp = actual;
	} else if (actual == 0) {
		kmem_free(ipc_kernel_map, addr, size);

		*countp = 0;
	} else {
		vm_map_copy_t copy;
		vm_size_t used;

		used = round_page(actual * sizeof *info);

		if (used != size)
			kmem_free(ipc_kernel_map, addr + used, size - used);

		kr = vm_map_copyin(ipc_kernel_map, addr, used,
				   TRUE, &copy);
		assert(kr == KERN_SUCCESS);

		*infop = (hash_info_bucket_t *) copy;
		*countp = actual;
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_dnrequest_info
 *	Purpose:
 *		Returns information about the dead-name requests
 *		registered with the named receive right.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Retrieved information.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_dnrequest_info(
	ipc_space_t	space,
	mach_port_name_t	name,
	unsigned int	*totalp,
	unsigned int	*usedp)
{
	unsigned int total, used;
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS)
		return kr;
	/* port is locked and active */

	if (port->ip_dnrequests == IPR_NULL) {
		total = 0;
		used = 0;
	} else {
		ipc_port_request_t dnrequests = port->ip_dnrequests;
		ipc_port_request_index_t index;

		total = dnrequests->ipr_size->its_size;

		for (index = 1, used = 0;
		     index < total; index++) {
			ipc_port_request_t ipr = &dnrequests[index];

			if (ipr->ipr_name != MACH_PORT_NULL)
				used++;
		}
	}
	ip_unlock(port);

	*totalp = total;
	*usedp = used;
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_kernel_object [kernel call]
 *	Purpose:
 *		Retrieve the type and address of the kernel object
 *		represented by a send or receive right.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Retrieved kernel object info.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote
 *					send or receive rights.
 */

kern_return_t
mach_port_kernel_object(
	ipc_space_t	space,
	mach_port_name_t	name,
	unsigned int	*typep,
	vm_offset_t	*addrp)
{
	ipc_entry_t entry;
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL)
		return KERN_INVALID_TASK;

	kr = ipc_right_lookup_read(space, name, &entry);
	if (kr != KERN_SUCCESS)
		return kr;
	/* space is read-locked and active */

	if ((entry->ie_bits & MACH_PORT_TYPE_SEND_RECEIVE) == 0) {
		is_read_unlock(space);
		return KERN_INVALID_RIGHT;
	}

	port = (ipc_port_t) entry->ie_object;
	assert(port != IP_NULL);

	ip_lock(port);
	is_read_unlock(space);

	if (!ip_active(port)) {
		ip_unlock(port);
		return KERN_INVALID_RIGHT;
	}

	*typep = ip_kotype(port);
	*addrp = port->ip_kobject;
	ip_unlock(port);
	return KERN_SUCCESS;
}
