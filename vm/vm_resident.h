/*
 * Resident memory management module functions.
 * Copyright (C) 2008 Free Software Foundation, Inc.
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
 *  Author: Barry deFreese.
 */
/*
 *     Resident memory management module functions.
 *
 */

#ifndef _VM_RESIDENT_H_
#define _VM_RESIDENT_H_

#include <mach/std_types.h>

/*
 *  vm_page_replace:
 *
 *  Exactly like vm_page_insert, except that we first
 *  remove any existing page at the given offset in object
 *  and we don't do deactivate-behind.
 *
 *  The object and page must be locked.
 */
extern void vm_page_replace (
    register vm_page_t mem,
    register vm_object_t object,
    register vm_offset_t offset);

#endif /* _VM_RESIDENT_H_ */
