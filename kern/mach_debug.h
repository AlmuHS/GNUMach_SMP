/*
 * Copyright (C) 2023 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * GNU Mach is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _KERN_MACH_DEBUG_H
#define _KERN_MACH_DEBUG_H

#include <mach/mach_types.h>	/* task_t, pointer_t */
#include <kern/task.h>

/* RPCs */

#if defined(MACH_KDB) && defined(MACH_DEBUG)
kern_return_t host_load_symbol_table(
		host_t		host,
		task_t		task,
		char		*name,
		pointer_t	symtab,
		unsigned int	symbtab_count);
#endif /* defined(MACH_KDB) && defined(MACH_DEBUG) */

/* End of RPCs */

#endif /* _KERN_MACH_DEBUG_H */
