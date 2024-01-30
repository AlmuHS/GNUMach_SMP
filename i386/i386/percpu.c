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
#include <i386/smp.h>
#include <i386/apic.h>
#include <kern/cpu_number.h>
#include <i386/percpu.h>

struct percpu percpu_array[NCPUS] = {0};

#ifndef MACH_XEN
void init_percpu(int cpu)
{
    int apic_id = apic_get_current_cpu();

    percpu_array[cpu].self = &percpu_array[cpu];
    percpu_array[cpu].apic_id = apic_id;
    percpu_array[cpu].cpu_id = cpu;
}
#endif
