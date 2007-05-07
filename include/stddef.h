/*
 * Copyright (C) 2007 Free Software Foundation, Inc.
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

#ifndef _STDDEF_H_
#define _STDDEF_H_

/* From GCC's `/lib/gcc/X/X/include/stddef.h'.  */

/* Offset of member MEMBER in a struct of type TYPE.  */
#define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)

#endif /* _STDDEF_H_ */
