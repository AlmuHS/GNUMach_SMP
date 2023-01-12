/*
 * Copyright (C) 2023 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * GNU Mach is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _KERN_MACH_DEBUG_H
#define _KERN_MACH_DEBUG_H

#include <mach/mach_types.h>	/* task_t, pointer_t */
#include <kern/task.h>

/* RPCs */

#if defined(MACH_KDB) && defined(MACH_DEBUG)
kern_return_t host_load_symbol_table(
		host_t		host,
		task_t		task,
		char		*name,
		pointer_t	symtab,
		unsigned int	symbtab_count);
#endif /* defined(MACH_KDB) && defined(MACH_DEBUG) */

kern_return_t
mach_port_get_srights(
	ipc_space_t		space,
	mach_port_name_t	name,
	mach_port_rights_t	*srightsp);

kern_return_t
mach_port_dnrequest_info(
	ipc_space_t	space,
	mach_port_name_t	name,
	unsigned int	*totalp,
	unsigned int	*usedp);

kern_return_t
mach_port_kernel_object(
	ipc_space_t	space,
	mach_port_name_t	name,
	unsigned int	*typep,
	vm_offset_t	*addrp);

kern_return_t
host_ipc_marequest_info(
	host_t 				host,
	unsigned int 			*maxp,
	hash_info_bucket_array_t 	*infop,
	unsigned int 			*countp);

#if MACH_DEBUG
kern_return_t host_slab_info(host_t host, cache_info_array_t *infop,
                             unsigned int *infoCntp);
#endif /* MACH_DEBUG */

kern_return_t processor_set_stack_usage(
	processor_set_t	pset,
	unsigned int	*totalp,
	vm_size_t	*spacep,
	vm_size_t	*residentp,
	vm_size_t	*maxusagep,
	vm_offset_t	*maxstackp);

kern_return_t host_stack_usage(
	host_t		host,
	vm_size_t	*reservedp,
	unsigned int	*totalp,
	vm_size_t	*spacep,
	vm_size_t	*residentp,
	vm_size_t	*maxusagep,
	vm_offset_t	*maxstackp);

kern_return_t
mach_vm_region_info(
	vm_map_t 		map,
	vm_offset_t 		address,
	vm_region_info_t 	*regionp,
	ipc_port_t 		*portp);

kern_return_t
mach_vm_object_info(
	vm_object_t 		object,
	vm_object_info_t 	*infop,
	ipc_port_t 		*shadowp,
	ipc_port_t 		*copyp);

kern_return_t
mach_vm_object_pages(
	vm_object_t 		object,
	vm_page_info_array_t 	*pagesp,
	natural_t 		*countp);

kern_return_t
host_virtual_physical_table_info(const host_t host,
		hash_info_bucket_array_t *infop, natural_t *countp);

/* End of RPCs */

#endif /* _KERN_MACH_DEBUG_H */
