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

#ifndef _DDB_DB_ELF_H_
#define _DDB_DB_ELF_H_

#include <ddb/db_sym.h>
#include <machine/db_machdep.h>

extern boolean_t
elf_db_line_at_pc(
	db_symtab_t	*stab,
	db_sym_t	sym,
	char		**file,
	int		*line,
	db_addr_t	pc);

extern db_sym_t
elf_db_lookup(
	db_symtab_t	*stab,
	char *		symstr);

extern db_sym_t
elf_db_search_symbol(
	db_symtab_t *	symtab,
	db_addr_t	off,
	db_strategy_t	strategy,
	db_expr_t	*diffp);

extern void
elf_db_symbol_values(
	db_symtab_t	*stab,
	db_sym_t	sym,
	char		**namep,
	db_expr_t	*valuep);

#endif /* _DDB_DB_ELF_H_ */
