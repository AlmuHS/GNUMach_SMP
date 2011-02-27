/*
 * Copyright (C) 2007 Free Software Foundation, Inc.
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
 * Author: Barry deFreese.
 */

#ifndef _I386_DB_INTERFACE_H_
#define _I386_DB_INTERFACE_H_

#include <sys/types.h>
#include <kern/task.h>
#include <machine/thread.h>

extern boolean_t kdb_trap (
	int 			type,
	int			code,
	struct i386_saved_state *regs);

extern void db_read_bytes (
	vm_offset_t	addr,
	int		size,
	char		*data,
	task_t		task);

extern void db_write_bytes (
	vm_offset_t	addr,
	int		size,
	char		*data,
	task_t		task);

extern boolean_t db_check_access (
	vm_offset_t	addr,
	int		size,
	task_t		task);

extern boolean_t db_phys_eq (
	task_t		task1,
	vm_offset_t	addr1,
	task_t		task2,
	vm_offset_t	addr2);

extern void db_task_name (task_t task);

#define I386_DB_TYPE_X 0
#define I386_DB_TYPE_W 1
#define I386_DB_TYPE_RW 3

#define I386_DB_LEN_1 0
#define I386_DB_LEN_2 1
#define I386_DB_LEN_4 3

#define I386_DB_LOCAL 1
#define I386_DB_GLOBAL 2

extern unsigned long dr0 (vm_offset_t linear_addr, int type, int len, int persistence);
extern unsigned long dr1 (vm_offset_t linear_addr, int type, int len, int persistence);
extern unsigned long dr2 (vm_offset_t linear_addr, int type, int len, int persistence);
extern unsigned long dr3 (vm_offset_t linear_addr, int type, int len, int persistence);

#endif /* _I386_DB_INTERFACE_H_ */
