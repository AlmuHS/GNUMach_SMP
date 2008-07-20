/*
 * Machine abstraction functions
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
 *  Author: Barry deFreese.
 */
/*
 *     Machine abstraction functions.
 *
 */

#ifndef _MACHINE_H_
#define _MACHINE_H_

#include <mach/std_types.h>

/*
 *  cpu_up:
 *
 *  Flag specified cpu as up and running.  Called when a processor comes
 *  online.
 */
extern void cpu_up (int);

/*
 *  processor_assign() changes the processor set that a processor is
 *  assigned to.  Any previous assignment in progress is overridden.
 *  Synchronizes with assignment completion if wait is TRUE.
 */
extern kern_return_t processor_assign (processor_t, processor_set_t, boolean_t);

/*
 *  processor_shutdown() queues a processor up for shutdown.
 *  Any assignment in progress is overriden.  It does not synchronize
 *  with the shutdown (can be called from interrupt level).
 */
extern kern_return_t processor_shutdown (processor_t);

/*
 *  action_thread() shuts down processors or changes their assignment.
 */
extern void action_thread_continue (void);

#endif /* _MACHINE_H_ */
