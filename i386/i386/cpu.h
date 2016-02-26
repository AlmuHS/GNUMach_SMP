/*
 * Copyright (c) 2010-2014 Richard Braun.
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

#ifndef _X86_CPU_H
#define _X86_CPU_H

#include <kern/macros.h>

/*
 * EFLAGS register flags.
 */
#define CPU_EFL_ONE 0x00000002
#define CPU_EFL_IF  0x00000200

/*
 * Return the content of the EFLAGS register.
 *
 * Implies a compiler barrier.
 */
static __always_inline unsigned long
cpu_get_eflags(void)
{
    unsigned long eflags;

    asm volatile("pushf\n"
                 "pop %0\n"
                 : "=r" (eflags)
                 : : "memory");

    return eflags;
}

/*
 * Enable local interrupts.
 *
 * Implies a compiler barrier.
 */
static __always_inline void
cpu_intr_enable(void)
{
    asm volatile("sti" : : : "memory");
}

/*
 * Disable local interrupts.
 *
 * Implies a compiler barrier.
 */
static __always_inline void
cpu_intr_disable(void)
{
    asm volatile("cli" : : : "memory");
}

/*
 * Restore the content of the EFLAGS register, possibly enabling interrupts.
 *
 * Implies a compiler barrier.
 */
static __always_inline void
cpu_intr_restore(unsigned long flags)
{
    asm volatile("push %0\n"
                 "popf\n"
                 : : "r" (flags)
                 : "memory");
}

/*
 * Disable local interrupts, returning the previous content of the EFLAGS
 * register.
 *
 * Implies a compiler barrier.
 */
static __always_inline void
cpu_intr_save(unsigned long *flags)
{
    *flags = cpu_get_eflags();
    cpu_intr_disable();
}

/*
 * Return true if interrupts are enabled.
 *
 * Implies a compiler barrier.
 */
static __always_inline int
cpu_intr_enabled(void)
{
    unsigned long eflags;

    eflags = cpu_get_eflags();
    return (eflags & CPU_EFL_IF) ? 1 : 0;
}

#endif /* _X86_CPU_H */
