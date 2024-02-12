/*
 * Copyright (c) 2023 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PERCPU_H_
#define _PERCPU_H_

struct percpu;

#if NCPUS > 1

#define percpu_assign(stm, val)     \
    asm("mov %[src], %%gs:%c[offs]" \
         : /* No outputs */         \
         : [src] "r" (val), [offs] "e" (__builtin_offsetof(struct percpu, stm)) \
         : );

#define percpu_get(typ, stm)        \
MACRO_BEGIN                         \
    typ val_;                       \
                                    \
    asm("mov %%gs:%c[offs], %[dst]" \
         : [dst] "=r" (val_)        \
         : [offs] "e" (__builtin_offsetof(struct percpu, stm)) \
         : );                       \
                                    \
    val_;                           \
MACRO_END

#define percpu_ptr(typ, stm)        \
MACRO_BEGIN                         \
    typ *ptr_ = (typ *)__builtin_offsetof(struct percpu, stm); \
                                    \
    asm("add %%gs:0, %[pointer]"    \
         : [pointer] "+r" (ptr_)    \
         : /* No inputs */          \
         : );                       \
                                    \
    ptr_;                           \
MACRO_END

#else

#define percpu_assign(stm, val)     \
MACRO_BEGIN                         \
        percpu_array[0].stm = val;  \
MACRO_END
#define percpu_get(typ, stm)        \
        (percpu_array[0].stm)
#define percpu_ptr(typ, stm)        \
        (&percpu_array[0].stm)

#endif

#include <kern/processor.h>
#include <mach/mach_types.h>

struct percpu {
    struct percpu	*self;
    int			apic_id;
    int			cpu_id;
    struct processor	processor;
    thread_t		active_thread;
    vm_offset_t		active_stack;
/*
    struct machine_slot	machine_slot;
    struct mp_desc_table mp_desc_table;
    vm_offset_t		int_stack_top;
    vm_offset_t		int_stack_base;
    ast_t		need_ast;
    ipc_kmsg_t		ipc_kmsg_cache;
    pmap_update_list	cpu_update_list;
    spl_t		saved_ipl;
    spl_t		curr_ipl;
    timer_data_t	kernel_timer;
    timer_t		current_timer;
    unsigned long	in_interrupt;
*/
};

extern struct percpu percpu_array[NCPUS];

void init_percpu(int cpu);

#endif /* _PERCPU_H_ */
