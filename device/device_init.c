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
 * 	Initialize device service as part of kernel task.
 */
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <kern/debug.h>
#include <kern/task.h>
#include <xen/xen.h>

#include <device/device_types.h>
#include <device/device_port.h>
#include <device/tty.h>
#include <device/device_init.h>
#include <device/ds_routines.h>
#include <device/net_io.h>
#include <device/chario.h>


ipc_port_t	master_device_port;

void
device_service_create(void)
{
	master_device_port = ipc_port_alloc_kernel();
	if (master_device_port == IP_NULL)
	    panic("can't allocate master device port");

	mach_device_init();
#ifdef MACH_HYP
	hyp_dev_init();
#endif
	dev_lookup_init();
	net_io_init();
	device_pager_init();
	chario_init();

	(void) kernel_thread(kernel_task, io_done_thread, 0);
	(void) kernel_thread(kernel_task, net_thread, 0);
}
