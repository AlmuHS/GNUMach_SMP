/*
 * MIG IPC functions
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
 *     MIG IPC functions.
 *
 */

#ifndef _IPC_MIG_H_
#define _IPC_MIG_H_

#include <mach/std_types.h>
#include <device/device_types.h>

/*
 *  Routine:    mach_msg_send_from_kernel
 *  Purpose:
 *      Send a message from the kernel.
 *
 *      This is used by the client side of KernelUser interfaces
 *      to implement SimpleRoutines.  Currently, this includes
 *      device_reply and memory_object messages.
 *  Conditions:
 *      Nothing locked.
 *  Returns:
 *      MACH_MSG_SUCCESS    Sent the message.
 *      MACH_SEND_INVALID_DATA  Bad destination port.
 */
extern mach_msg_return_t mach_msg_send_from_kernel(
    mach_msg_header_t   *msg,
    mach_msg_size_t     send_size);

/*
 *  Routine:    mach_msg_abort_rpc
 *  Purpose:
 *      Destroy the thread's ith_rpc_reply port.
 *      This will interrupt a mach_msg_rpc_from_kernel
 *      with a MACH_RCV_PORT_DIED return code.
 *  Conditions:
 *      Nothing locked.
 */
extern void mach_msg_abort_rpc (ipc_thread_t);

extern mach_msg_return_t mach_msg_rpc_from_kernel(
    const mach_msg_header_t *msg,
    mach_msg_size_t send_size,
    mach_msg_size_t reply_size);

extern kern_return_t syscall_vm_map(
	mach_port_t	target_map,
	vm_offset_t	*address,
	vm_size_t	size,
	vm_offset_t	mask,
	boolean_t	anywhere,
	mach_port_t	memory_object,
	vm_offset_t	offset,
	boolean_t	copy,
	vm_prot_t	cur_protection,
	vm_prot_t	max_protection,
	vm_inherit_t	inheritance);

extern kern_return_t syscall_vm_allocate(
	mach_port_t		target_map,
	vm_offset_t		*address,
	vm_size_t		size,
	boolean_t		anywhere);

extern kern_return_t syscall_vm_deallocate(
	mach_port_t		target_map,
	vm_offset_t		start,
	vm_size_t		size);

extern kern_return_t syscall_task_create(
	mach_port_t	parent_task,
	boolean_t	inherit_memory,
	mach_port_t	*child_task);

extern kern_return_t syscall_task_terminate(mach_port_t task);

extern kern_return_t syscall_task_suspend(mach_port_t task);

extern kern_return_t syscall_task_set_special_port(
	mach_port_t	task,
	int		which_port,
	mach_port_t	port_name);

extern kern_return_t syscall_mach_port_allocate(
	mach_port_t 		task,
	mach_port_right_t 	right,
	mach_port_t 		*namep);

extern kern_return_t syscall_mach_port_deallocate(
	mach_port_t task,
	mach_port_t name);

extern kern_return_t syscall_mach_port_insert_right(
	mach_port_t task,
	mach_port_t name,
	mach_port_t right,
	mach_msg_type_name_t rightType);

extern kern_return_t syscall_mach_port_allocate_name(
	mach_port_t 		task,
	mach_port_right_t 	right,
	mach_port_t 		name);

extern kern_return_t syscall_thread_depress_abort(mach_port_t thread);

extern io_return_t syscall_device_write_request(
			mach_port_t	device_name,
			mach_port_t	reply_name,
			dev_mode_t	mode,
			recnum_t	recnum,
			vm_offset_t	data,
			vm_size_t	data_count);

io_return_t syscall_device_writev_request(
			mach_port_t	device_name,
			mach_port_t	reply_name,
			dev_mode_t	mode,
			recnum_t	recnum,
			io_buf_vec_t	*iovec,
			vm_size_t	iocount);

#endif /* _IPC_MIG_H_ */
