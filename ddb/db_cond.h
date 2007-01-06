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

#ifndef _DDB_DB_COND_H_
#define _DDB_DB_COND_H_

#include <sys/types.h>
#include <machine/db_machdep.h>

extern void db_cond_free (db_thread_breakpoint_t bkpt);

extern boolean_t db_cond_check (db_thread_breakpoint_t bkpt);

extern void db_cond_print (db_thread_breakpoint_t bkpt);

extern void db_cond_cmd (void);

#endif /* _DDB_DB_COND_H_ */
