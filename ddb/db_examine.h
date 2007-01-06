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

#ifndef _DDB_DB_EXAMINE_H_
#define _DDB_DB_EXAMINE_H_

#include <sys/types.h>
#include <ddb/db_variables.h>
#include <ddb/db_expr.h>

extern void db_examine_cmd (
	db_expr_t addr, 
	int have_addr, 
	db_expr_t count,
	char *modif);

extern void db_strcpy (char *dst, char *src);

extern void db_examine (
	db_addr_t addr,
	char *fmt,
	int count,
	task_t task);

extern void db_print_loc_and_inst (
	db_addr_t loc,
	task_t task);

#endif /* _DDB_DB_EXAMINE_H_ */
