/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 *  Abstract:
 *	Syscall functions
 *
 */

#ifndef	_SYSCALLS_
#define	_SYSCALLS_

#include <device/device_types.h>
#include <mach/message.h>

// TODO: there is probably a better way to define these

#define MACH_SYSCALL0(syscallid, retval, name)  \
  retval name(void) __attribute__((naked));

#define MACH_SYSCALL1(syscallid, retval, name, arg1)    \
  retval name(arg1 a1) __attribute__((naked));

#define MACH_SYSCALL2(syscallid, retval, name, arg1, arg2)  \
  retval name(arg1 a1, arg2 a2) __attribute__((naked));

#define MACH_SYSCALL3(syscallid, retval, name, arg1, arg2, arg3)  \
  retval name(arg1 a1, arg2 a2, arg3 a3) __attribute__((naked));

#define MACH_SYSCALL4(syscallid, retval, name, arg1, arg2, arg3, arg4)  \
  retval name(arg1 a1, arg2 a2, arg3 a3, arg4 a4) __attribute__((naked));

#define MACH_SYSCALL6(syscallid, retval, name, arg1, arg2, arg3, arg4, arg5, arg6)  \
  retval name(arg1 a1, arg2 a2, arg3 a3, arg4 a4, arg5 a5, arg6 a6) __attribute__((naked));

#define MACH_SYSCALL7(syscallid, retval, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
  retval name(arg1 a1, arg2 a2, arg3 a3, arg4 a4, arg5 a5, arg6 a6, arg7 a7) __attribute__((naked));

#define mach_msg mach_msg_trap

MACH_SYSCALL0(26, mach_port_name_t, mach_reply_port)
MACH_SYSCALL0(27, mach_port_name_t, mach_thread_self)
MACH_SYSCALL0(28, mach_port_name_t, mach_task_self)
MACH_SYSCALL0(29, mach_port_name_t, mach_host_self)
MACH_SYSCALL1(30, void, mach_print, const char*)
MACH_SYSCALL0(31, kern_return_t, invalid_syscall)
MACH_SYSCALL4(65, kern_return_t, syscall_vm_allocate, mach_port_t, vm_offset_t*, vm_size_t, boolean_t)
MACH_SYSCALL3(66, kern_return_t, syscall_vm_deallocate, mach_port_t, vm_offset_t, vm_size_t)
MACH_SYSCALL3(72, kern_return_t, syscall_mach_port_allocate, mach_port_t, mach_port_right_t, mach_port_t*)
MACH_SYSCALL2(73, kern_return_t, syscall_mach_port_deallocate, mach_port_t, mach_port_t)

/*
  todo: swtch_pri swtch ...
  these seem obsolete: evc_wait
    evc_wait_clear syscall_device_writev_request
    syscall_device_write_request ...
 */
MACH_SYSCALL6(40, io_return_t, syscall_device_write_request, mach_port_name_t,
              mach_port_name_t, dev_mode_t, recnum_t, vm_offset_t, vm_size_t)

#endif	/* SYSCALLS */
