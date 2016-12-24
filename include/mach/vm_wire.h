/*
 * Copyright (C) 2017 Free Software Foundation
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

#ifndef _MACH_VM_WIRE_H_
#define _MACH_VM_WIRE_H_

typedef int vm_wire_t;

#define VM_WIRE_NONE    0
#define VM_WIRE_CURRENT 1
#define VM_WIRE_FUTURE  2

#define VM_WIRE_ALL     (VM_WIRE_CURRENT | VM_WIRE_FUTURE)

#endif /* _MACH_VM_WIRE_H_ */
