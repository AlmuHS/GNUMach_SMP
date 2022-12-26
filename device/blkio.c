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
 *	Date: 	7/89
 *
 * 	Block IO driven from generic kernel IO interface.
 */
#include <mach/kern_return.h>

#include <device/blkio.h>
#include <device/buf.h>
#include <device/param.h>
#include <device/device_types.h>
#include <device/io_req.h>
#include <device/ds_routines.h>


/*
 * 'standard' max_count routine.  VM continuations mean that this
 * code can cope with arbitrarily-sized write operations (they won't be
 * atomic, but any caller that cares will do the op synchronously).
 */
#define MAX_PHYS        (256 * 1024)

void minphys(io_req_t ior)
{
	if ((ior->io_op & (IO_WRITE | IO_READ | IO_OPEN)) == IO_WRITE)
	    return;

        if (ior->io_count > MAX_PHYS)
            ior->io_count = MAX_PHYS;
}

/*
 * Dummy routine placed in device switch entries to indicate that
 * block device may be mapped.
 */
vm_offset_t block_io_mmap(dev_t dev, vm_offset_t off, int prot)
{
	return (0);
}

