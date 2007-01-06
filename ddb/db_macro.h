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

#ifndef _DDB_DB_MACRO_H_
#define _DDB_DB_MACRO_H_

#include <sys/types.h>
#include <ddb/db_variables.h>

extern void db_def_macro_cmd (void);

extern void db_del_macro_cmd (void);

extern void db_show_macro (void);

extern int db_exec_macro (char *name);

extern int db_arg_variable (
	struct db_variable *vp,
	db_expr_t *valuep,
	int flag,
	db_var_aux_param_t ap);

#endif /* _DDB_DB_MACRO_H_ */
