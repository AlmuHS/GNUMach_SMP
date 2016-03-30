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

#ifndef _STDINT_H_
#define _STDINT_H_

/*
 * These types are _exactly_ as wide as indicated in their names.
 */

typedef char		int8_t;
typedef short		int16_t;
typedef int		int32_t;
#if __x86_64__
typedef long int	int64_t;
#else
typedef long long int	int64_t;
#endif /* __x86_64__ */

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;
#if __x86_64__
typedef unsigned long int	uint64_t;
#else
typedef unsigned long long int	uint64_t;
#endif /* __x86_64__ */

/* Types for `void *' pointers.  */
#if __x86_64__
typedef long int		intptr_t;
typedef unsigned long int	uintptr_t;
#else
typedef int			intptr_t;
typedef unsigned int		uintptr_t;
#endif /* __x86_64__ */

#endif /* _STDINT_H_ */
