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
    mach_msg_header_t *msg,
    mach_msg_size_t send_size,
    mach_msg_size_t reply_size);

#endif /* _IPC_MIG_H_ */
