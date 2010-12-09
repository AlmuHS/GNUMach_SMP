/*
 * String Handling Functions.
 * Copyright (C) 2006 Free Software Foundation, Inc.
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
 *	Author: Barry deFreese.
 */
/*
 *     String handling functions.
 *
 */

#ifndef _MACH_SA_SYS_STRING_H_
#define _MACH_SA_SYS_STRING_H_

#include <sys/types.h>

extern void *memcpy (void *dest, const void *src, size_t n);

extern void *memmove (void *dest, const void *src, size_t n);

extern int *memcmp (const void *s1, const void *s2, size_t n);

extern void *memset (void *s, int c, size_t n);

extern char *strchr (const char *s, int c);

extern char *strcpy (char *dest, const char *src);

extern char *strncpy (char *dest, const char *src, size_t n);

extern char *strrchr (const char *s, int c);

extern char *strsep (char **strp, const char *delim);

extern int strcmp (const char *s1, const char *s2);

extern int strncmp (const char *s1, const char *s2, size_t n);

extern size_t strlen (const char *s);

extern char *strstr(const char *haystack, const char *needle);

#endif /* _MACH_SA_SYS_STRING_H_ */
