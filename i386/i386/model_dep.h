/*
 * Arch dependent functions
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
 *	Author: Barry deFreese.
 */
/*
 *     Arch dependent functions.
 *
 */

#ifndef _I386AT_MODEL_DEP_H_
#define _I386AT_MODEL_DEP_H_

#include <mach/std_types.h>

/*
 * Find devices.  The system is alive.
 */
extern void machine_init (void);

/* Conserve power on processor CPU.  */
extern void machine_idle (int cpu);

/*
 * Halt a cpu.
 */
extern void halt_cpu (void) __attribute__ ((noreturn));

/*
 * Halt the system or reboot.
 */
extern void halt_all_cpus (boolean_t reboot) __attribute__ ((noreturn));

extern void resettodr (void);

extern void startrtclock (void);

/*
 *	More-specific code provides these;
 *	they indicate the total extent of physical memory
 *	that we know about and might ever have to manage.
 */
extern vm_offset_t phys_first_addr, phys_last_addr;

#endif /* _I386AT_MODEL_DEP_H_ */
