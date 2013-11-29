/*
 * Copyright (c) 2013 Free Software Foundation.
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

#ifndef _MODEL_DEP_H_
#define _MODEL_DEP_H_

#include <mach/vm_prot.h>

extern int timemmap(int dev, int off, vm_prot_t prot);

/*
 * Halt a cpu.
 */
extern void halt_cpu (void) __attribute__ ((noreturn));

/*
 * Halt the system or reboot.
 */
extern void halt_all_cpus (boolean_t reboot) __attribute__ ((noreturn));

void inittodr(void);

boolean_t init_alloc_aligned(vm_size_t size, vm_offset_t *addrp);

#endif /* _MODEL_DEP_H_ */
