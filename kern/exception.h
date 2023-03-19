/*
 * Copyright (c) 2013 Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _KERN_EXCEPTION_H_
#define _KERN_EXCEPTION_H_

#include <ipc/ipc_types.h>
#include <ipc/ipc_kmsg.h>

extern void
exception(
	integer_t 	_exception,
	integer_t	code,
	long_integer_t	subcode) __attribute__ ((noreturn));

extern void
exception_try_task(
	integer_t 	_exception,
	integer_t	code,
	long_integer_t	subcode) __attribute__ ((noreturn));

extern void
exception_no_server(void) __attribute__ ((noreturn));

extern void
exception_raise(
	ipc_port_t dest_port,
	ipc_port_t thread_port,
	ipc_port_t task_port,
	integer_t  _exception,
	integer_t  code,
	long_integer_t  subcode) __attribute__ ((noreturn));

extern kern_return_t
exception_parse_reply(ipc_kmsg_t kmsg);

extern void
exception_raise_continue(void) __attribute__ ((noreturn));

extern void
exception_raise_continue_slow(
	mach_msg_return_t 	mr,
	ipc_kmsg_t 		kmsg,
	mach_port_seqno_t 	seqno) __attribute__ ((noreturn));

extern void
exception_raise_continue_fast(
	ipc_port_t reply_port,
	ipc_kmsg_t kmsg) __attribute__ ((noreturn));

#endif /* _KERN_EXCEPTION_H_ */
