/*
 * Copyright (C) 2007 Free Software Foundation, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify it
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
 * 
 * Written by Thomas Schwinge.
 */

#ifndef VM_VM_TYPES_H
#define VM_VM_TYPES_H

/*
 *	Types defined:
 *
 *	vm_map_t		the high-level address map data structure.
 *	vm_object_t		Virtual memory object.
 *	vm_page_t		See `vm/vm_page.h'.
 */

typedef struct vm_map *vm_map_t;
#define VM_MAP_NULL ((vm_map_t) 0)

typedef struct vm_object *vm_object_t;
#define VM_OBJECT_NULL ((vm_object_t) 0)

typedef struct vm_page *vm_page_t;
#define VM_PAGE_NULL ((vm_page_t) 0)


#endif /* VM_VM_TYPES_H */
