/*
 * Copyright (C) 2016 Free Software Foundation, Inc.
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

#ifndef _MACH_MACHINE_STDINT_H_
#define _MACH_MACHINE_STDINT_H_

/*
 * These types are _exactly_ as wide as indicated in their names.
 */

typedef char		__mach_int8_t;
typedef short		__mach_int16_t;
typedef int		__mach_int32_t;
#if __x86_64__
typedef long int	__mach_int64_t;
#else
typedef long long int	__mach_int64_t;
#endif /* __x86_64__ */

typedef unsigned char		__mach_uint8_t;
typedef unsigned short		__mach_uint16_t;
typedef unsigned int		__mach_uint32_t;
#if __x86_64__
typedef unsigned long int	__mach_uint64_t;
#else
typedef unsigned long long int	__mach_uint64_t;
#endif /* __x86_64__ */

/* Types for `void *' pointers.  */
#if __x86_64__
typedef long int		__mach_intptr_t;
typedef unsigned long int	__mach_uintptr_t;
#else
typedef int			__mach_intptr_t;
typedef unsigned int		__mach_uintptr_t;
#endif /* __x86_64__ */

#endif /* _MACH_MACHINE_STDINT_H_ */
