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

#ifndef _DDB_DB_MP_H_
#define _DDB_DB_MP_H_

void	remote_db(void);
int	lock_db(void);
void	unlock_db(int);
void	db_on(int i);

#if CONSOLE_ON_MASTER
void db_console(void);
#endif /* CONSOLE_ON_MASTER */

boolean_t db_enter(void);
void remote_db_enter(void);
void db_leave(void);

#endif /* _DDB_DB_MP_H_ */
