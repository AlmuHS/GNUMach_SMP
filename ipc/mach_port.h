/*
 * Mach Port Functions.
 * Copyright (C) 2008 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *	Author: Barry deFreese.
 */
/*
 *     Mach port functions.
 *
 */

#ifndef _IPC_MACH_PORT_H_
#define _IPC_MACH_PORT_H_

#include <sys/types.h>
#include <ipc/ipc_types.h>
#include <ipc/ipc_entry.h>

#if	MACH_KDB
void db_debug_port_references (boolean_t enable);
#endif	/* MACH_KDB */

/* RPCs */

extern kern_return_t
mach_port_allocate_name (
    ipc_space_t space,
    mach_port_right_t right,
    mach_port_name_t name);

extern kern_return_t
mach_port_allocate (
    ipc_space_t space,
    mach_port_right_t right,
    mach_port_name_t *namep);

extern kern_return_t
mach_port_destroy(
    ipc_space_t space,
    mach_port_name_t name);

extern kern_return_t
mach_port_deallocate(
    ipc_space_t space,
    mach_port_name_t name);

extern kern_return_t
mach_port_insert_right(
    ipc_space_t     space,
    mach_port_name_t     name,
    ipc_port_t      poly,
    mach_msg_type_name_t    polyPoly);

kern_return_t
mach_port_get_receive_status(
	ipc_space_t 		space,
	mach_port_name_t 	name,
	mach_port_status_t 	*statusp);

kern_return_t
mach_port_names(
	ipc_space_t		space,
	mach_port_name_t	**namesp,
	mach_msg_type_number_t	*namesCnt,
	mach_port_type_t	**typesp,
	mach_msg_type_number_t	*typesCnt);

kern_return_t
mach_port_type(
	ipc_space_t		space,
	mach_port_name_t	name,
	mach_port_type_t	*typep);

kern_return_t
mach_port_rename(
	ipc_space_t		space,
	mach_port_name_t	oname,
	mach_port_name_t	nname);

kern_return_t
mach_port_get_refs(
	ipc_space_t		space,
	mach_port_name_t	name,
	mach_port_right_t	right,
	mach_port_urefs_t	*urefsp);

kern_return_t
mach_port_mod_refs(
	ipc_space_t		space,
	mach_port_name_t	name,
	mach_port_right_t	right,
	mach_port_delta_t	delta);

kern_return_t
mach_port_set_qlimit(
	ipc_space_t 		space,
	mach_port_name_t 	name,
	mach_port_msgcount_t 	qlimit);

kern_return_t
mach_port_set_mscount(
	ipc_space_t		space,
	mach_port_name_t	name,
	mach_port_mscount_t	mscount);

kern_return_t
mach_port_set_seqno(
	ipc_space_t		space,
	mach_port_name_t	name,
	mach_port_seqno_t	seqno);

kern_return_t
mach_port_get_set_status(
	ipc_space_t			space,
	mach_port_name_t		name,
	mach_port_name_t		**members,
	mach_msg_type_number_t		*membersCnt);

kern_return_t
mach_port_move_member(
	ipc_space_t	space,
	mach_port_name_t	member,
	mach_port_name_t	after);

kern_return_t
mach_port_request_notification(
	ipc_space_t		space,
	mach_port_name_t		name,
	mach_msg_id_t		id,
	mach_port_mscount_t	sync,
	ipc_port_t		notify,
	ipc_port_t		*previousp);

kern_return_t
mach_port_extract_right(
	ipc_space_t		space,
	mach_port_name_t	name,
	mach_msg_type_name_t	msgt_name,
	ipc_port_t		*poly,
	mach_msg_type_name_t	*polyPoly);

kern_return_t
mach_port_set_protected_payload(
	ipc_space_t		space,
	mach_port_name_t	name,
	rpc_uintptr_t		payload);

kern_return_t
mach_port_clear_protected_payload(
	ipc_space_t		space,
	mach_port_name_t	name);

/* End of RPCs */

#endif /* _IPC_MACH_PORT_H_ */
