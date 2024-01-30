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
 *	MIG helpers for gnumach tests, mainly copied from glibc
 *
 */

#ifndef	_MACH_MIG_SUPPORT_H_
#define	_MACH_MIG_SUPPORT_H_

#include <string.h>

#include <mach/message.h>
#include <mach/mach_types.h>

#include <syscalls.h>

static inline void mig_init(void *_first)
{}

static inline void mig_allocate(vm_address_t *addr, vm_size_t size)
{
    if (syscall_vm_allocate(mach_task_self(), addr, size, 1) != KERN_SUCCESS)
        *addr = 0;
}
static inline void mig_deallocate(vm_address_t addr, vm_size_t size)
{
    syscall_vm_deallocate (mach_task_self(), addr, size);
}
static inline void mig_dealloc_reply_port(mach_port_t port)
{}
static inline void mig_put_reply_port(mach_port_t port)
{}
static inline mach_port_t mig_get_reply_port(void)
{
    return mach_reply_port();
}
static inline void mig_reply_setup(const mach_msg_header_t *_request,
                                   mach_msg_header_t *reply)
{}

static inline vm_size_t mig_strncpy (char *dst, const char *src, vm_size_t len)
{
    return dst - strncpy(dst, src, len);
}

#endif	/* not defined(_MACH_MIG_SUPPORT_H_) */
