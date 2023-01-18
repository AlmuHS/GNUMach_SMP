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

#endif /* _IPC_MACH_PORT_H_ */
