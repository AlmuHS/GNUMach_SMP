/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	8/89
 *
 *	Device service utility routines.
 */

#ifndef	DS_ROUTINES_H
#define	DS_ROUTINES_H

#include <vm/vm_map.h>
#include <device/device_types.h>
#include <device/io_req.h>
#include <mach/machine/vm_types.h>

/*
 * Map for device IO memory.
 */
extern vm_map_t		device_io_map;

extern queue_head_t	io_done_list;

kern_return_t	device_read_alloc(io_req_t, vm_size_t);
kern_return_t	device_write_get(io_req_t, boolean_t *);
boolean_t	device_write_dealloc(io_req_t);
void		device_reference(device_t);

boolean_t	ds_notify(mach_msg_header_t *msg);
boolean_t	ds_open_done(io_req_t);
boolean_t	ds_read_done(io_req_t);
boolean_t	ds_write_done(io_req_t);

void		iowait (io_req_t ior);

kern_return_t	device_pager_setup(
	const mach_device_t	device,
	int			prot,
	vm_offset_t		offset,
	vm_size_t		size,
	mach_port_t		*pager);

extern void mach_device_init(void);
extern void dev_lookup_init(void);
extern void device_pager_init(void);
extern void io_done_thread(void) __attribute__ ((noreturn));

io_return_t ds_device_write_trap(
	device_t 	dev,
	dev_mode_t 	mode,
	recnum_t 	recnum,
	vm_offset_t 	data,
	vm_size_t 	count);

io_return_t ds_device_writev_trap(
	device_t 	dev,
	dev_mode_t 	mode,
	recnum_t 	recnum,
	io_buf_vec_t 	*iovec,
	vm_size_t 	count);

/* XXX arch-specific */
extern ipc_port_t intr_rcv_ports[16];

#endif	/* DS_ROUTINES_H */
