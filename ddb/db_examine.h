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
	const char *modif);

extern void db_strcpy (char *dst, const char *src);

extern void db_examine (
	db_addr_t addr,
	const char *fmt,
	int count,
	task_t task);

void db_examine_forward(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif);

void db_examine_backward(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif);

extern void db_print_loc_and_inst (
	db_addr_t loc,
	task_t task);

int db_xcdump(
	db_addr_t addr,
	int	size,
	int	count,
	task_t task);

void db_print_cmd(void);

void db_search_cmd(void);

void db_search(
	db_addr_t	addr,
	int		size,
	db_expr_t	value,
	db_expr_t	mask,
	unsigned int	count,
	task_t		task);

/* instruction disassembler */
extern db_addr_t db_disasm(
	db_addr_t pc,
	boolean_t altform,
	task_t task);

#endif /* _DDB_DB_EXAMINE_H_ */
