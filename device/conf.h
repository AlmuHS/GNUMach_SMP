/* 
 * Mach Operating System
 * Copyright (c) 1993,1991,1990,1989 Carnegie Mellon University
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
 *	Date: 	8/88
 */

#ifndef	_DEVICE_CONF_H_
#define	_DEVICE_CONF_H_

#include <mach/machine/vm_types.h>
#include <sys/types.h>
#include <mach/port.h>
#include <mach/vm_prot.h>
#include <device/device_types.h>
#include <device/net_status.h>

struct io_req;
typedef struct io_req *io_req_t;

typedef int io_return_t;

/*
 * Operations list for major device types.
 */
struct dev_ops {
	char *    	d_name;				/* name for major device */
	int		(*d_open)(dev_t, int, io_req_t);/* open device */
	void		(*d_close)(dev_t, int);		/* close device */
	int		(*d_read)(dev_t, io_req_t);	/* read */
	int		(*d_write)(dev_t, io_req_t);	/* write */
	int		(*d_getstat)(dev_t, dev_flavor_t, dev_status_t, mach_msg_type_number_t *);	/* get status/control */
	int		(*d_setstat)(dev_t, dev_flavor_t, dev_status_t, mach_msg_type_number_t);	/* set status/control */
	vm_offset_t	(*d_mmap)(dev_t, vm_offset_t, vm_prot_t);	/* map memory */
	int		(*d_async_in)(dev_t, const ipc_port_t, int, filter_t*, unsigned int);		/* asynchronous input setup */
	int		(*d_reset)(dev_t);		/* reset device */
	int		(*d_port_death)(dev_t, mach_port_t);
					/* clean up reply ports */
	int		d_subdev;	/* number of sub-devices per
					   unit */
	int		(*d_dev_info)(dev_t, int, int*); /* driver info for kernel */
};
typedef struct dev_ops *dev_ops_t;

/*
 * Routines for null entries.
 */
extern int	nulldev_reset(dev_t dev);
extern int	nulldev_open(dev_t dev, int flag, io_req_t ior);
extern void	nulldev_close(dev_t dev, int flags);
extern int	nulldev_read(dev_t dev, io_req_t ior);
extern int	nulldev_write(dev_t dev, io_req_t ior);
extern io_return_t	nulldev_getstat(dev_t dev, dev_flavor_t flavor, dev_status_t data, mach_msg_type_number_t *count);
extern io_return_t	nulldev_setstat(dev_t dev, dev_flavor_t flavor, dev_status_t data, mach_msg_type_number_t count);
extern io_return_t	nulldev_portdeath(dev_t dev, mach_port_t port);
extern int nodev_async_in(dev_t, const ipc_port_t, int, filter_t*, unsigned int); /* no operation - error */
extern int nodev_info(dev_t, int, int*); /* no operation - error */
extern vm_offset_t	nomap(dev_t dev, vm_offset_t off, int prot);		/* no operation - error */

/*
 * Flavor constants for d_dev_info routine
 */
#define D_INFO_BLOCK_SIZE	1

/*
 * Head of list of attached devices
 */
extern struct dev_ops	dev_name_list[];
extern int		dev_name_count;

/*
 * Macro to search device list
 */
#define	dev_search(dp)	\
	for (dp = dev_name_list; \
	     dp < &dev_name_list[dev_name_count]; \
	     dp++)

/*
 * Indirection vectors for certain devices.
 */
struct dev_indirect {
	char *		d_name;		/* name for device */
	dev_ops_t	d_ops;		/* operations (major device) */
	int		d_unit;		/* and unit number */
};
typedef struct dev_indirect	*dev_indirect_t;

/*
 * List of indirect devices.
 */
extern struct dev_indirect	dev_indirect_list[];
extern int			dev_indirect_count;

/*
 * Macro to search indirect list
 */
#define	dev_indirect_search(di) \
	for (di = dev_indirect_list; \
	     di < &dev_indirect_list[dev_indirect_count]; \
	     di++)

#endif	/* _DEVICE_CONF_H_ */

