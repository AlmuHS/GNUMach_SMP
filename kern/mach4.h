/*
 * Copyright (C) 2023 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * GNU Mach is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _KERN_MACH4_H
#define _KERN_MACH4_H

#include <kern/thread.h>

/* RPCs */

kern_return_t
thread_enable_pc_sampling(
    thread_t thread,
    int *tickp,
    sampled_pc_flavor_t flavors);

kern_return_t
thread_disable_pc_sampling(
    thread_t thread,
    int *samplecntp);

kern_return_t
task_enable_pc_sampling(
    task_t task,
    int *tickp,
    sampled_pc_flavor_t flavors);

kern_return_t
task_disable_pc_sampling(
    task_t task,
    int *samplecntp);

kern_return_t
thread_get_sampled_pcs(
	thread_t thread,
	sampled_pc_seqno_t *seqnop,
	sampled_pc_array_t sampled_pcs_out,
	int *sampled_pcs_cntp);

kern_return_t
task_get_sampled_pcs(
	task_t task,
	sampled_pc_seqno_t *seqnop,
	sampled_pc_array_t sampled_pcs_out,
	int *sampled_pcs_cntp);

/* End of RPCs */

#endif /* _KERN_MACH4_H */
