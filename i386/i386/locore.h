/*
 * Header file for printf type functions.
 * Copyright (C) 2006 Free Software Foundation.
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

#ifndef _MACHINE_LOCORE_H_
#define _MACHINE_LOCORE_H_

#include <sys/types.h>

#include <kern/sched_prim.h>

extern int copyin (const void *userbuf, void *kernelbuf, size_t cn);

extern int copyinmsg (vm_offset_t userbuf, vm_offset_t kernelbuf, size_t cn);

extern int copyout (const void *kernelbuf, void *userbuf, size_t cn);

extern int copyoutmsg (vm_offset_t kernelbuf, vm_offset_t userbuf, size_t cn);

extern int call_continuation (continuation_t continuation);

#endif /* _MACHINE__LOCORE_H_ */

