/*
 * Copyright (c) 2022 Free Software Foundation, Inc.
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

#include <i386/cpu_number.h>
#include <i386/apic.h>
#include <i386/smp.h>
#include <i386/cpu.h>
#include <kern/printf.h>

#if NCPUS > 1
int cpu_number(void)
{
	int kernel_id, apic_id;
	apic_id = apic_get_current_cpu();
	if (apic_id < 0) {
		printf("apic_get_current_cpu() failed, assuming BSP\n");
		apic_id = 0;
	}

	kernel_id = apic_get_cpu_kernel_id(apic_id);
	if (kernel_id < 0) {
		printf("apic_get_cpu_kernel_id() failed, assuming BSP\n");
		kernel_id = 0;
	}
}
#endif
