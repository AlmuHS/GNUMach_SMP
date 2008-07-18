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

extern kern_return_t
mach_port_allocate_name (
    ipc_space_t space,
    mach_port_right_t right,
    mach_port_t name);

extern kern_return_t
mach_port_allocate (
    ipc_space_t space,
    mach_port_right_t right,
    mach_port_t *namep);

extern kern_return_t
mach_port_deallocate(
    ipc_space_t space,
    mach_port_t name);

extern kern_return_t
mach_port_insert_right(
    ipc_space_t     space,
    mach_port_t     name,
    ipc_port_t      poly,
    mach_msg_type_name_t    polyPoly);

#endif /* _IPC_MACH_PORT_H_ */
