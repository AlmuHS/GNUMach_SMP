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
 *	Date: 	3/89
 */

/*
 * Mach device emulation definitions (i386at version).
 *
 * Copyright (c) 1996 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Shantanu Goel, University of Utah CSL
 */

#ifndef	_DEVICE_DEV_HDR_H_
#define	_DEVICE_DEV_HDR_H_

#include <ipc/ipc_types.h>
#include <mach/port.h>
#include <kern/lock.h>
#include <kern/queue.h>

#include <device/conf.h>

/* This structure is associated with each open device port.
   The port representing the device points to this structure.  */
struct device
{
  struct device_emulation_ops *emul_ops;
  void *emul_data;
};

typedef struct device *device_t;

#define DEVICE_NULL	((device_t) 0)

/*
 * Generic device header.  May be allocated with the device,
 * or built when the device is opened.
 */
struct mach_device {
	decl_simple_lock_data(,ref_lock)/* lock for reference count */
	int		ref_count;	/* reference count */
	decl_simple_lock_data(, lock)	/* lock for rest of state */
	short		state;		/* state: */
#define	DEV_STATE_INIT		0	/* not open  */
#define	DEV_STATE_OPENING	1	/* being opened */
#define	DEV_STATE_OPEN		2	/* open */
#define	DEV_STATE_CLOSING	3	/* being closed */
	short		flag;		/* random flags: */
#define	D_EXCL_OPEN		0x0001	/* open only once */
	short		open_count;	/* number of times open */
	short		io_in_progress;	/* number of IOs in progress */
	boolean_t	io_wait;	/* someone waiting for IO to finish */

	struct ipc_port *port;		/* open port */
	queue_chain_t	number_chain;	/* chain for lookup by number */
	int		dev_number;	/* device number */
	int		bsize;		/* replacement for DEV_BSIZE */
	struct dev_ops	*dev_ops;	/* and operations vector */
	struct device	dev;		/* the real device structure */
};
typedef	struct mach_device *mach_device_t;
#define	MACH_DEVICE_NULL ((mach_device_t)0)

/*
 * To find and remove device entries
 */
mach_device_t	device_lookup(char *);	/* by name */

void		mach_device_reference(mach_device_t);
void		mach_device_deallocate(mach_device_t);

/*
 * To find and remove port-to-device mappings
 */
device_t	dev_port_lookup(ipc_port_t);
void		dev_port_enter(mach_device_t);
void		dev_port_remove(mach_device_t);

/*
 * To call a routine on each device
 */
boolean_t	dev_map(boolean_t (*)(), mach_port_t);

/*
 * To lock and unlock state and open-count
 */
#define	device_lock(device)	simple_lock(&(device)->lock)
#define	device_unlock(device)	simple_unlock(&(device)->lock)

/*
 * device name lookup
 */
extern boolean_t dev_name_lookup(
    char *      name,
    dev_ops_t   *ops,   /* out */
    int     *unit);  /* out */

/*
 * Change an entry in the indirection list.
 */
extern void dev_set_indirection(
    char        *name,
    dev_ops_t   ops,
    int     unit);

#endif	/* _DEVICE_DEV_HDR_H_ */
