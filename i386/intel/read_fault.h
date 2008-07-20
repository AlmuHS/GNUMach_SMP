/*
 * Kernel read_fault on i386 functions
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
 *     Kernel read_fault on i386 functions.
 *
 */

#ifndef _READ_FAULT_H_
#define _READ_FAULT_H_

#include <mach/std_types.h>

extern kern_return_t intel_read_fault(
        vm_map_t map,
        vm_offset_t vaddr);

#endif /* _READ_FAULT_H_ */
