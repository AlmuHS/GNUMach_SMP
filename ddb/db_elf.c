/*
 * Copyright (C) 2014 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#if MACH_KDB

/*
 * Symbol table routines for ELF format files.
 */

#include <string.h>
#include <mach/std_types.h>
#include <mach/exec/elf.h>
#include <machine/db_machdep.h>		/* data types */
#include <machine/vm_param.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>
#include <ddb/db_elf.h>

#ifndef	DB_NO_ELF

struct db_symtab_elf {
  int		type;
  Elf_Sym	*start;
  Elf_Sym	*end;
  char		*strings;
  char		*map_pointer;	/* symbols are for this map only,
				   if not null */
  char		name[SYMTAB_NAME_LEN];
  /* symtab name */
};

boolean_t
elf_db_sym_init (unsigned shdr_num,
		 vm_size_t shdr_size,
		 vm_offset_t shdr_addr,
		 unsigned shdr_shndx,
		 char *name,
		 char *task_addr)
{
  Elf_Shdr *shdr, *symtab, *strtab;
  const char *shstrtab;
  unsigned i;

  if (shdr_num == 0)
    return FALSE;

  if (shdr_size != sizeof *shdr)
    return FALSE;

  shdr = (Elf_Shdr *) shdr_addr;

  if (shdr[shdr_shndx].sh_type != SHT_STRTAB)
    return FALSE;

  shstrtab = (const char *) phystokv (shdr[shdr_shndx].sh_addr);

  symtab = strtab = NULL;
  for (i = 0; i < shdr_num; i++)
    switch (shdr[i].sh_type) {
    case SHT_SYMTAB:
      if (symtab)
	db_printf ("Ignoring additional ELF symbol table at %d\n", i);
      else
	symtab = &shdr[i];
      break;

    case SHT_STRTAB:
      if (strcmp (&shstrtab[shdr[i].sh_name], ".strtab") == 0) {
	if (strtab)
	  db_printf ("Ignoring additional ELF string table at %d\n", i);
	else
	  strtab = &shdr[i];
      }
      break;
    }

  if (symtab == NULL || strtab == NULL)
    return FALSE;

  if (db_add_symbol_table (SYMTAB_ELF,
			   (char *) phystokv (symtab->sh_addr),
			   (char *) phystokv (symtab->sh_addr)+symtab->sh_size,
			   name,
			   (char *) phystokv (strtab->sh_addr),
			   task_addr)) {
    db_printf ("Loaded ELF symbol table for %s (%d symbols)\n",
	       name, symtab->sh_size / sizeof (Elf_Sym));
    return TRUE;
  }

  return FALSE;
}

/*
 * lookup symbol by name
 */
db_sym_t
elf_db_lookup (db_symtab_t *stab,
	       char *symstr)
{
  struct db_symtab_elf *self = (struct db_symtab_elf *) stab;
  Elf_Sym *s;

  for (s = self->start; s < self->end; s++)
    if (strcmp (symstr, &self->strings[s->st_name]) == 0)
      return (db_sym_t) s;

  return NULL;
}

db_sym_t
elf_db_search_symbol (db_symtab_t *stab,
		      db_addr_t off,
		      db_strategy_t strategy,
		      db_expr_t *diffp)	/* in/out */
{
  struct db_symtab_elf *self = (struct db_symtab_elf *) stab;
  unsigned long	diff = *diffp;
  Elf_Sym *s, *symp = NULL;

  for (s = self->start; s < self->end; s++) {
    if (s->st_name == 0)
      continue;

    if (strategy == DB_STGY_XTRN && (ELF_ST_BIND(s->st_info) != STB_GLOBAL))
      continue;

    if (off >= s->st_value) {
      if (ELF_ST_TYPE(s->st_info) != STT_FUNC)
	continue;

      if (off - s->st_value < diff) {
	diff = off - s->st_value;
	symp = s;
	if (diff == 0 && (ELF_ST_BIND(s->st_info) == STB_GLOBAL))
	  break;
      } else if (off - s->st_value == diff) {
	if (symp == NULL)
	  symp = s;
	else if ((ELF_ST_BIND(symp->st_info) != STB_GLOBAL)
		 && (ELF_ST_BIND(s->st_info) == STB_GLOBAL))
	  symp = s;	/* pick the external symbol */
      }
    }
  }

  if (symp == NULL)
    *diffp = off;
  else
    *diffp = diff;

  return (db_sym_t) symp;
}

/*
 * Return the name and value for a symbol.
 */
void
elf_db_symbol_values (db_symtab_t *stab,
		      db_sym_t sym,
		      char **namep,
		      db_expr_t *valuep)
{
  struct db_symtab_elf *self = (struct db_symtab_elf *) stab;
  Elf_Sym *s = (Elf_Sym *) sym;

  if (namep)
    *namep = &self->strings[s->st_name];
  if (valuep)
    *valuep = s->st_value;
}

/*
 * Find filename and lineno within, given the current pc.
 */
boolean_t
elf_db_line_at_pc (db_symtab_t *stab,
		   db_sym_t sym,
		   char **file,
		   int *line,
		   db_addr_t pc)
{
  /* XXX Parse DWARF information.  */
  return FALSE;
}

#endif	/* DB_NO_ELF */

#endif /* MACH_KDB */
