/*
 * Header file for printf type functions.
 * Copyright (C) 2006, 2007 Free Software Foundation.
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

#ifndef _MACH_SA_SYS_PRINTF_H_
#define _MACH_SA_SYS_PRINTF_H_

#include <sys/types.h>
#include <stdarg.h>

extern void printf_init (void);

extern void _doprnt (const char *fmt,
		     va_list *argp, 
		     void (*putc)(char, vm_offset_t), 
		     int radix, 
		     vm_offset_t putc_arg);

extern void printnum (unsigned long u, int base,
                      void (*putc)(char, vm_offset_t),
                      vm_offset_t putc_arg);

extern int sprintf (char *buf, const char *fmt, ...);

extern int printf (const char *fmt, ...);

extern int indent;
extern void iprintf (const char *fmt, ...);

extern int vprintf(const char *fmt, va_list listp);

extern void safe_gets (char *str, int maxlen);

#endif /* _MACH_SA_SYS_PRINTF_H_ */

