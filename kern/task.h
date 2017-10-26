/*
 * Mach Operating System
 * Copyright (c) 1993-1988 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	File:	task.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	This file contains the structure definitions for tasks.
 *
 */

#ifndef	_KERN_TASK_H_
#define _KERN_TASK_H_

#include <mach/boolean.h>
#include <mach/port.h>
#include <mach/time_value.h>
#include <mach/mach_param.h>
#include <mach/task_info.h>
#include <mach_debug/mach_debug_types.h>
#include <kern/kern_types.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <kern/pc_sample.h>
#include <kern/processor.h>
#include <kern/syscall_emulation.h>
#include <vm/vm_types.h>
#include <machine/task.h>

/*
 * Task name buffer size.  The size is chosen so that struct task fits
 * into three cache lines.  The size of a cache line on a typical CPU
 * is 64 bytes.
 */
#define TASK_NAME_SIZE 32

struct task {
	/* Synchronization/destruction information */
	decl_simple_lock_data(,lock)	/* Task's lock */
	int		ref_count;	/* Number of references to me */

	/* Flags */
	unsigned int	active:1,	/* Task has not been terminated */
	/* boolean_t */ may_assign:1,	/* can assigned pset be changed? */
			assign_active:1;	/* waiting for may_assign */

	/* Miscellaneous */
	vm_map_t	map;		/* Address space description */
	queue_chain_t	pset_tasks;	/* list of tasks assigned to pset */
	int		suspend_count;	/* Internal scheduling only */

	/* Thread information */
	queue_head_t	thread_list;	/* list of threads */
	int		thread_count;	/* number of threads */
	processor_set_t	processor_set;	/* processor set for new threads */

	/* User-visible scheduling information */
	int		user_stop_count;	/* outstanding stops */
	int		priority;		/* for new threads */

	/* Statistics */
	time_value_t	total_user_time;
				/* total user time for dead threads */
	time_value_t	total_system_time;
				/* total system time for dead threads */

	time_value_t	creation_time; /* time stamp at creation */

	/* IPC structures */
	decl_simple_lock_data(, itk_lock_data)
	struct ipc_port *itk_self;	/* not a right, doesn't hold ref */
	struct ipc_port *itk_sself;	/* a send right */
	struct ipc_port *itk_exception;	/* a send right */
	struct ipc_port *itk_bootstrap;	/* a send right */
	struct ipc_port *itk_registered[TASK_PORT_REGISTER_MAX];
					/* all send rights */

	struct ipc_space *itk_space;

	/* User space system call emulation support */
	struct 	eml_dispatch	*eml_dispatch;

	sample_control_t pc_sample;

#if	FAST_TAS
#define TASK_FAST_TAS_NRAS	8
	vm_offset_t	fast_tas_base[TASK_FAST_TAS_NRAS];
	vm_offset_t	fast_tas_end[TASK_FAST_TAS_NRAS];
#endif	/* FAST_TAS */

	/* Hardware specific data.  */
	machine_task_t	machine;

	/* Statistics */
	natural_t	faults;		/* page faults counter */
	natural_t	zero_fills;	/* zero fill pages counter */
	natural_t	reactivations;	/* reactivated pages counter */
	natural_t	pageins;	/* actual pageins couter */
	natural_t	cow_faults;	/* copy-on-write faults counter */
	natural_t	messages_sent;	/* messages sent counter */
	natural_t	messages_received; /* messages received counter */

	char	name[TASK_NAME_SIZE];
};

#define task_lock(task)		simple_lock(&(task)->lock)
#define task_unlock(task)	simple_unlock(&(task)->lock)

#define	itk_lock_init(task)	simple_lock_init(&(task)->itk_lock_data)
#define	itk_lock(task)		simple_lock(&(task)->itk_lock_data)
#define	itk_unlock(task)	simple_unlock(&(task)->itk_lock_data)

/*
 *	Exported routines/macros
 */

extern kern_return_t	task_create(
	task_t		parent_task,
	boolean_t	inherit_memory,
	task_t		*child_task);
extern kern_return_t	task_create_kernel(
	task_t		parent_task,
	boolean_t	inherit_memory,
	task_t		*child_task);
extern kern_return_t	task_terminate(
	task_t		task);
extern kern_return_t	task_suspend(
	task_t		task);
extern kern_return_t	task_resume(
	task_t		task);
extern kern_return_t	task_threads(
	task_t		task,
	thread_array_t	*thread_list,
	natural_t	*count);
extern kern_return_t	task_info(
	task_t		task,
	int		flavor,
	task_info_t	task_info_out,
	natural_t	*task_info_count);
extern kern_return_t	task_get_special_port(
	task_t		task,
	int		which,
	struct ipc_port	**portp);
extern kern_return_t	task_set_special_port(
	task_t		task,
	int		which,
	struct ipc_port	*port);
extern kern_return_t	task_assign(
	task_t		task,
	processor_set_t	new_pset,
	boolean_t	assign_threads);
extern kern_return_t	task_assign_default(
	task_t		task,
	boolean_t	assign_threads);
extern kern_return_t	task_set_name(
	task_t			task,
	kernel_debug_name_t	name);
extern void consider_task_collect(void);

/*
 *	Internal only routines
 */

extern void		task_init(void);
extern void		task_reference(task_t);
extern void		task_deallocate(task_t);
extern void		task_hold_locked(task_t);
extern kern_return_t	task_hold(task_t);
extern kern_return_t	task_dowait(task_t, boolean_t);
extern kern_return_t	task_release(task_t);

extern task_t	kernel_task;

#endif	/* _KERN_TASK_H_ */
