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

#ifndef _KERN_MACH_HOST_H
#define _KERN_MACH_HOST_H

#include <kern/processor.h>
#include <kern/host.h>
#include <mach/host_info.h>

/* RPCs */

kern_return_t host_processors(
	const host_t		host,
	processor_array_t	*processor_list,
	natural_t		*countp);

kern_return_t	host_info(
	const host_t	host,
	int		flavor,
	host_info_t	info,
	natural_t	*count);

kern_return_t host_kernel_version(
	const host_t		host,
	kernel_version_t	out_version);

kern_return_t
host_processor_sets(
	const host_t			host,
	processor_set_name_array_t	*pset_list,
	natural_t			*count);

kern_return_t
host_processor_set_priv(
	const host_t	host,
	processor_set_t	pset_name,
	processor_set_t	*pset);

kern_return_t
processor_set_default(
	const host_t	host,
	processor_set_t	*pset);

kern_return_t
host_reboot(const host_t host, int options);

kern_return_t
host_get_boot_info(
        host_t              priv_host,
        kernel_boot_info_t  boot_info);

kern_return_t task_get_assignment(
	task_t		task,
	processor_set_t	*pset);

kern_return_t
thread_wire(
	host_t		host,
	thread_t	thread,
	boolean_t	wired);

kern_return_t thread_get_assignment(
	thread_t	thread,
	processor_set_t	*pset);

/* End of RPCs */

#endif /* _KERN_MACH_HOST_H */
