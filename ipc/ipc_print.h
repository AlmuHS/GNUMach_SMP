/*
 * Copyright (c) 2013 Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _IPC_PRINT_H_
#define	_IPC_PRINT_H_

#if MACH_KDB

#include <mach/mach_types.h>
#include <mach/message.h>
#include <ipc/ipc_types.h>
#include <ipc/ipc_pset.h>

extern void ipc_port_print(const ipc_port_t);

extern void ipc_pset_print(const ipc_pset_t);

extern void ipc_kmsg_print(const ipc_kmsg_t);

extern void ipc_msg_print(mach_msg_header_t*);

#endif  /* MACH_KDB */

#endif	/* IPC_PRINT_H */
