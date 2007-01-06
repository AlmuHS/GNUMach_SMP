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
 *	Author: Barry deFreese.
 */

#ifndef _DDB_DB_TRAP_H_
#define _DDB_DB_TRAP_H_

#include <sys/types.h>
#include <machine/db_machdep.h>

extern void db_task_trap (
	int 		type,
	int 		code,
	boolean_t 	user_space);

extern void db_trap (int type, int code);

#endif /* _DDB_DB_TRAP_H_ */
