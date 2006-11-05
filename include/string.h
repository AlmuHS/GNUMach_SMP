/*
 * String Handling Functions.
 * Copyright (C) 2006 Barry deFreese.
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
 */
/*
 *     String handling functions.
 *
 */

#ifndef _MACH_SA_SYS_STRING_H_
#define _MACH_SA_SYS_STRING_H_

#include <sys/types.h>

extern void *memcpy (void *dest, const void *src, size_t n);

extern void *memset (void *s, int c, size_t n);

#endif /* _MACH_SA_SYS_STRING_H_ */
