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
 * Address to hold AP boot code, held in ASM
 */
extern phys_addr_t apboot_addr;

/*
 * Find devices.  The system is alive.
 */
extern void machine_init (void);

/* Conserve power on processor CPU.  */
extern void machine_idle (int cpu);

extern void resettodr (void);

extern void startrtclock (void);

/*
 * Halt a cpu.
 */
extern void halt_cpu (void) __attribute__ ((noreturn));

/*
 * Halt the system or reboot.
 */
extern void halt_all_cpus (boolean_t reboot) __attribute__ ((noreturn));

/*
 * Make cpu pause a bit.
 */
extern void machine_relax (void);

/*
 * C boot entrypoint - called by boot_entry in boothdr.S.
 */
extern void c_boot_entry(vm_offset_t bi);

#endif /* _I386AT_MODEL_DEP_H_ */
