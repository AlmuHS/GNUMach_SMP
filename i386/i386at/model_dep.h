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

#include <i386/vm_param.h>
#include <mach/vm_prot.h>

/*
 * Interrupt stack.
 */
extern vm_offset_t int_stack_top[NCPUS], int_stack_base[NCPUS];

/* Check whether P points to the per-cpu interrupt stack.  */
#define ON_INT_STACK(P, CPU)	(((P) & ~(INTSTACK_SIZE-1)) == int_stack_base[CPU])

extern vm_offset_t timemmap(dev_t dev, vm_offset_t off, vm_prot_t prot);

void inittodr(void);

boolean_t init_alloc_aligned(vm_size_t size, vm_offset_t *addrp);

#endif /* _MODEL_DEP_H_ */
