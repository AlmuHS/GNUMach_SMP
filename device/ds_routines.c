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
 *	Date: 	3/89
 */

/*
 * Mach device server routines (i386at version).
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

#include <kern/printf.h>
#include <string.h>

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/mig_errors.h>
#include <mach/port.h>
#include <mach/vm_param.h>
#include <mach/notify.h>
#include <machine/locore.h>
#include <machine/machspl.h>		/* spl definitions */

#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <kern/ast.h>
#include <kern/counters.h>
#include <kern/debug.h>
#include <kern/printf.h>
#include <kern/queue.h>
#include <kern/slab.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/sched_prim.h>

#include <vm/memory_object.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_user.h>

#include <device/device_types.h>
#include <device/dev_hdr.h>
#include <device/conf.h>
#include <device/io_req.h>
#include <device/ds_routines.h>
#include <device/net_status.h>
#include <device/device_port.h>
#include <device/device_reply.user.h>
#include <device/device_emul.h>

#include <machine/machspl.h>

#ifdef LINUX_DEV
extern struct device_emulation_ops linux_block_emulation_ops;
#ifdef CONFIG_INET
extern struct device_emulation_ops linux_net_emulation_ops;
extern void free_skbuffs (void);
#ifdef CONFIG_PCMCIA
extern struct device_emulation_ops linux_pcmcia_emulation_ops;
#endif /* CONFIG_PCMCIA */
#endif /* CONFIG_INET */
#endif /* LINUX_DEV */
#ifdef MACH_HYP
extern struct device_emulation_ops hyp_block_emulation_ops;
extern struct device_emulation_ops hyp_net_emulation_ops;
#endif /* MACH_HYP */
extern struct device_emulation_ops mach_device_emulation_ops;

/* List of emulations.  */
static struct device_emulation_ops *emulation_list[] =
{
#ifdef LINUX_DEV
  &linux_block_emulation_ops,
#ifdef CONFIG_INET
  &linux_net_emulation_ops,
#ifdef CONFIG_PCMCIA
  &linux_pcmcia_emulation_ops,
#endif /* CONFIG_PCMCIA */
#endif /* CONFIG_INET */
#endif /* LINUX_DEV */
#ifdef MACH_HYP
  &hyp_block_emulation_ops,
  &hyp_net_emulation_ops,
#endif /* MACH_HYP */
  &mach_device_emulation_ops,
};

static struct vm_map	device_io_map_store;
vm_map_t		device_io_map = &device_io_map_store;

#define NUM_EMULATION (sizeof (emulation_list) / sizeof (emulation_list[0]))

io_return_t
ds_device_open (ipc_port_t open_port, ipc_port_t reply_port,
		mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		char *name, device_t *devp)
{
  unsigned i;
  io_return_t err;

  /* Open must be called on the master device port.  */
  if (open_port != master_device_port)
    return D_INVALID_OPERATION;

  /* There must be a reply port.  */
  if (! IP_VALID (reply_port))
    {
      printf ("ds_* invalid reply port\n");
      SoftDebugger ("ds_* reply_port");
      return MIG_NO_REPLY;
    }

  /* Call each emulation's open routine to find the device.  */
  for (i = 0; i < NUM_EMULATION; i++)
    {
      err = (*emulation_list[i]->open) (reply_port, reply_port_type,
					mode, name, devp);
      if (err != D_NO_SUCH_DEVICE)
	break;
    }

  return err;
}

io_return_t
ds_device_close (device_t dev)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  return (dev->emul_ops->close
	  ? (*dev->emul_ops->close) (dev->emul_data)
	  : D_SUCCESS);
}

io_return_t
ds_device_write (device_t dev, ipc_port_t reply_port,
		 mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		 recnum_t recnum, io_buf_ptr_t data, unsigned int count,
		 int *bytes_written)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (data == 0)
    return D_INVALID_SIZE;

  if (! dev->emul_ops->write)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->write) (dev->emul_data, reply_port,
				  reply_port_type, mode, recnum,
				  data, count, bytes_written);
}

io_return_t
ds_device_write_inband (device_t dev, ipc_port_t reply_port,
			mach_msg_type_name_t reply_port_type,
			dev_mode_t mode, recnum_t recnum,
			io_buf_ptr_inband_t data, unsigned count,
			int *bytes_written)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (data == 0)
    return D_INVALID_SIZE;

  if (! dev->emul_ops->write_inband)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->write_inband) (dev->emul_data, reply_port,
					 reply_port_type, mode, recnum,
					 data, count, bytes_written);
}

io_return_t
ds_device_read (device_t dev, ipc_port_t reply_port,
		mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		recnum_t recnum, int count, io_buf_ptr_t *data,
		unsigned *bytes_read)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (! dev->emul_ops->read)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->read) (dev->emul_data, reply_port,
				 reply_port_type, mode, recnum,
				 count, data, bytes_read);
}

io_return_t
ds_device_read_inband (device_t dev, ipc_port_t reply_port,
		       mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		       recnum_t recnum, int count, char *data,
		       unsigned *bytes_read)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (! dev->emul_ops->read_inband)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->read_inband) (dev->emul_data, reply_port,
					reply_port_type, mode, recnum,
					count, data, bytes_read);
}

io_return_t
ds_device_set_status (device_t dev, dev_flavor_t flavor,
		      dev_status_t status, mach_msg_type_number_t status_count)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (! dev->emul_ops->set_status)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->set_status) (dev->emul_data, flavor, status,
				       status_count);
}

io_return_t
ds_device_get_status (device_t dev, dev_flavor_t flavor, dev_status_t status,
		      mach_msg_type_number_t *status_count)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (! dev->emul_ops->get_status)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->get_status) (dev->emul_data, flavor, status,
				       status_count);
}

io_return_t
ds_device_set_filter (device_t dev, ipc_port_t receive_port, int priority,
		      filter_t *filter, unsigned filter_count)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (! dev->emul_ops->set_filter)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->set_filter) (dev->emul_data, receive_port,
				       priority, filter, filter_count);
}

io_return_t
ds_device_map (device_t dev, vm_prot_t prot, vm_offset_t offset,
	       vm_size_t size, ipc_port_t *pager, boolean_t unmap)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (! dev->emul_ops->map)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->map) (dev->emul_data, prot,
				offset, size, pager, unmap);
}

boolean_t
ds_notify (mach_msg_header_t *msg)
{
  if (msg->msgh_id == MACH_NOTIFY_NO_SENDERS)
    {
      device_t dev;
      mach_no_senders_notification_t *ns;

      ns = (mach_no_senders_notification_t *) msg;
      dev = dev_port_lookup((ipc_port_t) ns->not_header.msgh_remote_port);
      assert(dev);
      if (dev->emul_ops->no_senders)
	(*dev->emul_ops->no_senders) (ns);
      return TRUE;
    }

  printf ("ds_notify: strange notification %d\n", msg->msgh_id);
  return FALSE;
}

io_return_t
ds_device_write_trap (device_t dev, dev_mode_t mode,
		      recnum_t recnum, vm_offset_t data, vm_size_t count)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (! dev->emul_ops->write_trap)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->write_trap) (dev->emul_data,
				       mode, recnum, data, count);
}

io_return_t
ds_device_writev_trap (device_t dev, dev_mode_t mode,
		       recnum_t recnum, io_buf_vec_t *iovec, vm_size_t count)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  if (! dev->emul_ops->writev_trap)
    return D_INVALID_OPERATION;

  return (*dev->emul_ops->writev_trap) (dev->emul_data,
					mode, recnum, iovec, count);
}

void
device_reference (device_t dev)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return;

  if (dev->emul_ops->reference)
    (*dev->emul_ops->reference) (dev->emul_data);
}

void
device_deallocate (device_t dev)
{
  /* Refuse if device is dead or not completely open.  */
  if (dev == DEVICE_NULL)
    return;

  if (dev->emul_ops->dealloc)
    (*dev->emul_ops->dealloc) (dev->emul_data);
}

/*
 * What follows is the interface for the native Mach devices.
 */

ipc_port_t
mach_convert_device_to_port (mach_device_t device)
{
  ipc_port_t port;

  if (! device)
    return IP_NULL;

  device_lock(device);

  if (device->state == DEV_STATE_OPEN)
    port = ipc_port_make_send(device->port);
  else
    port = IP_NULL;

  device_unlock(device);

  mach_device_deallocate(device);

  return port;
}

static io_return_t
device_open(const ipc_port_t	reply_port,
	    mach_msg_type_name_t reply_port_type,
	    dev_mode_t		mode,
	    char *		name,
	    device_t		*device_p)
{
	mach_device_t		device;
	kern_return_t		result;
	io_req_t		ior;
	ipc_port_t		notify;

	/*
	 * Find the device.
	 */
	device = device_lookup(name);
	if (device == MACH_DEVICE_NULL)
	    return (D_NO_SUCH_DEVICE);

	/*
	 * If the device is being opened or closed,
	 * wait for that operation to finish.
	 */
	device_lock(device);
	while (device->state == DEV_STATE_OPENING ||
		device->state == DEV_STATE_CLOSING) {
	    device->io_wait = TRUE;
	    thread_sleep((event_t)device, simple_lock_addr(device->lock), TRUE);
	    device_lock(device);
	}

	/*
	 * If the device is already open, increment the open count
	 * and return.
	 */
	if (device->state == DEV_STATE_OPEN) {

	    if (device->flag & D_EXCL_OPEN) {
		/*
		 * Cannot open a second time.
		 */
		device_unlock(device);
		mach_device_deallocate(device);
		return (D_ALREADY_OPEN);
	    }

	    device->open_count++;
	    device_unlock(device);
	    *device_p = &device->dev;
	    return (D_SUCCESS);
	    /*
	     * Return deallocates device reference while acquiring
	     * port.
	     */
	}

	/*
	 * Allocate the device port and register the device before
	 * opening it.
	 */
	device->state = DEV_STATE_OPENING;
	device_unlock(device);

	/*
	 * Allocate port, keeping a reference for it.
	 */
	device->port = ipc_port_alloc_kernel();
	if (device->port == IP_NULL) {
	    device_lock(device);
	    device->state = DEV_STATE_INIT;
	    device->port = IP_NULL;
	    if (device->io_wait) {
		device->io_wait = FALSE;
		thread_wakeup((event_t)device);
	    }
	    device_unlock(device);
	    mach_device_deallocate(device);
	    return (KERN_RESOURCE_SHORTAGE);
	}

	dev_port_enter(device);

	/*
	 * Request no-senders notifications on device port.
	 */
	notify = ipc_port_make_sonce(device->port);
	ip_lock(device->port);
	ipc_port_nsrequest(device->port, 1, notify, &notify);
	assert(notify == IP_NULL);

	/*
	 * Open the device.
	 */
	io_req_alloc(ior, 0);

	ior->io_device	= device;
	ior->io_unit	= device->dev_number;
	ior->io_op	= IO_OPEN | IO_CALL;
	ior->io_mode	= mode;
	ior->io_error	= 0;
	ior->io_done	= ds_open_done;
	ior->io_reply_port = reply_port;
	ior->io_reply_port_type = reply_port_type;

	result = (*device->dev_ops->d_open)(device->dev_number, (int)mode, ior);
	if (result == D_IO_QUEUED)
	    return (MIG_NO_REPLY);

	/*
	 * Return result via ds_open_done.
	 */
	ior->io_error = result;
	(void) ds_open_done(ior);

	io_req_free(ior);

	return (MIG_NO_REPLY);	/* reply already sent */
}

boolean_t
ds_open_done(const io_req_t ior)
{
	kern_return_t		result;
	mach_device_t		device;

	device = ior->io_device;
	result = ior->io_error;

	if (result != D_SUCCESS) {
	    /*
	     * Open failed.  Deallocate port and device.
	     */
	    dev_port_remove(device);
	    ipc_port_dealloc_kernel(device->port);
	    device->port = IP_NULL;

	    device_lock(device);
	    device->state = DEV_STATE_INIT;
	    if (device->io_wait) {
		device->io_wait = FALSE;
		thread_wakeup((event_t)device);
	    }
	    device_unlock(device);

	    mach_device_deallocate(device);
	    device = MACH_DEVICE_NULL;
	}
	else {
	    /*
	     * Open succeeded.
	     */
	    device_lock(device);
	    device->state = DEV_STATE_OPEN;
	    device->open_count = 1;
	    if (device->io_wait) {
		device->io_wait = FALSE;
		thread_wakeup((event_t)device);
	    }
	    device_unlock(device);

	    /* donate device reference to get port */
	}
	/*
	 * Must explicitly convert device to port, since
	 * device_reply interface is built as 'user' side
	 * (thus cannot get translation).
	 */
	if (IP_VALID(ior->io_reply_port)) {
		(void) ds_device_open_reply(ior->io_reply_port,
					    ior->io_reply_port_type,
					    result,
					    mach_convert_device_to_port(device));
	} else
		mach_device_deallocate(device);

	return (TRUE);
}

static io_return_t
device_close(void *dev)
{
	mach_device_t device = dev;

	device_lock(device);

	/*
	 * If device will remain open, do nothing.
	 */
	if (--device->open_count > 0) {
	    device_unlock(device);
	    return (D_SUCCESS);
	}

	/*
	 * If device is being closed, do nothing.
	 */
	if (device->state == DEV_STATE_CLOSING) {
	    device_unlock(device);
	    return (D_SUCCESS);
	}

	/*
	 * Mark device as closing, to prevent new IO.
	 * Outstanding IO will still be in progress.
	 */
	device->state = DEV_STATE_CLOSING;
	device_unlock(device);

	/*
	 * ? wait for IO to end ?
	 *   only if device wants to
	 */

	/*
	 * Remove the device-port association.
	 */
	dev_port_remove(device);
	ipc_port_dealloc_kernel(device->port);

	/*
	 * Close the device
	 */
	(*device->dev_ops->d_close)(device->dev_number, 0);

	/*
	 * Finally mark it closed.  If someone else is trying
	 * to open it, the open can now proceed.
	 */
	device_lock(device);
	device->state = DEV_STATE_INIT;
	if (device->io_wait) {
	    device->io_wait = FALSE;
	    thread_wakeup((event_t)device);
	}
	device_unlock(device);

	return (D_SUCCESS);
}

/*
 * Write to a device.
 */
static io_return_t
device_write(void *dev,
	     const ipc_port_t	reply_port,
	     mach_msg_type_name_t	reply_port_type,
	     dev_mode_t		mode,
	     recnum_t		recnum,
	     const io_buf_ptr_t	data,
	     unsigned int	data_count,
	     int		*bytes_written)
{
	mach_device_t		device = dev;
	io_req_t		ior;
	io_return_t		result;

	if (device->state != DEV_STATE_OPEN)
	    return (D_NO_SUCH_DEVICE);

	/*
	 * XXX Need logic to reject ridiculously big requests.
	 */

	/* XXX note that a CLOSE may proceed at any point */

	/*
	 * Package the write request for the device driver
	 */
	io_req_alloc(ior, data_count);

	ior->io_device		= device;
	ior->io_unit		= device->dev_number;
	ior->io_op		= IO_WRITE | IO_CALL;
	ior->io_mode		= mode;
	ior->io_recnum		= recnum;
	ior->io_data		= data;
	ior->io_count		= data_count;
	ior->io_total		= data_count;
	ior->io_alloc_size	= 0;
	ior->io_residual	= 0;
	ior->io_error		= 0;
	ior->io_done		= ds_write_done;
	ior->io_reply_port	= reply_port;
	ior->io_reply_port_type	= reply_port_type;
	ior->io_copy		= VM_MAP_COPY_NULL;

	/*
	 * The ior keeps an extra reference for the device.
	 */
	mach_device_reference(device);

	/*
	 * And do the write ...
	 *
	 * device_write_dealoc returns false if there's more
	 * to do; it has updated the ior appropriately and expects
	 * its caller to reinvoke it on the device.
	 */

	do {

		result = (*device->dev_ops->d_write)(device->dev_number, ior);

		/*
		 * If the IO was queued, delay reply until it is finished.
		 */
		if (result == D_IO_QUEUED)
		    return (MIG_NO_REPLY);

		/*
		 * Discard the local mapping of the data.
		 */

	} while (!device_write_dealloc(ior));

	/*
	 * Return the number of bytes actually written.
	 */
	*bytes_written = ior->io_total - ior->io_residual;

	/*
	 * Remove the extra reference.
	 */
	mach_device_deallocate(device);

	io_req_free(ior);
	return (result);
}

/*
 * Write to a device, but memory is in message.
 */
static io_return_t
device_write_inband(void		*dev,
		    const ipc_port_t	reply_port,
		    mach_msg_type_name_t	reply_port_type,
		    dev_mode_t		mode,
		    recnum_t		recnum,
		    io_buf_ptr_inband_t	data,
		    unsigned int	data_count,
		    int			*bytes_written)
{
	mach_device_t		device = dev;
	io_req_t		ior;
	io_return_t		result;

	if (device->state != DEV_STATE_OPEN)
	    return (D_NO_SUCH_DEVICE);

	/* XXX note that a CLOSE may proceed at any point */

	/*
	 * Package the write request for the device driver.
	 */
	io_req_alloc(ior, 0);

	ior->io_device		= device;
	ior->io_unit		= device->dev_number;
	ior->io_op		= IO_WRITE | IO_CALL | IO_INBAND;
	ior->io_mode		= mode;
	ior->io_recnum		= recnum;
	ior->io_data		= data;
	ior->io_count		= data_count;
	ior->io_total		= data_count;
	ior->io_alloc_size	= 0;
	ior->io_residual	= 0;
	ior->io_error		= 0;
	ior->io_done		= ds_write_done;
	ior->io_reply_port	= reply_port;
	ior->io_reply_port_type = reply_port_type;

	/*
	 * The ior keeps an extra reference for the device.
	 */
	mach_device_reference(device);

	/*
	 * And do the write.
	 */
	result = (*device->dev_ops->d_write)(device->dev_number, ior);

	/*
	 * If the IO was queued, delay reply until it is finished.
	 */
	if (result == D_IO_QUEUED)
	    return (MIG_NO_REPLY);

	/*
	 * Return the number of bytes actually written.
	 */
	*bytes_written = ior->io_total - ior->io_residual;

	/*
	 * Remove the extra reference.
	 */
	mach_device_deallocate(device);

	io_req_free(ior);
	return (result);
}

/*
 * Wire down incoming memory to give to device.
 */
kern_return_t
device_write_get(
	io_req_t		ior,
	boolean_t		*wait)
{
	vm_map_copy_t		io_copy;
	vm_offset_t		new_addr;
	kern_return_t		result;
	int			bsize;
	vm_size_t		min_size;

	/*
	 * By default, caller does not have to wait.
	 */
	*wait = FALSE;

	/*
	 * Nothing to do if no data.
	 */
	if (ior->io_count == 0)
	    return (KERN_SUCCESS);

	/*
	 * Loaned iors already have valid data.
	 */
	if (ior->io_op & IO_LOANED)
	    return (KERN_SUCCESS);

	/*
	 * Inband case.
	 */
	if (ior->io_op & IO_INBAND) {
	    assert(ior->io_count <= sizeof (io_buf_ptr_inband_t));
	    new_addr = kmem_cache_alloc(&io_inband_cache);
	    memcpy((void*)new_addr, ior->io_data, ior->io_count);
	    ior->io_data = (io_buf_ptr_t)new_addr;
	    ior->io_alloc_size = sizeof (io_buf_ptr_inband_t);

	    return (KERN_SUCCESS);
	}

	/*
	 *	Figure out how much data to move this time.  If the device
	 *	won't return a block size, then we have to do the whole
	 *	request in one shot (ditto if this is a block fragment),
	 *	otherwise, move at least one block's worth.
	 */
	result = (*ior->io_device->dev_ops->d_dev_info)(
					ior->io_device->dev_number,
					D_INFO_BLOCK_SIZE,
					&bsize);

	if (result != KERN_SUCCESS || ior->io_count < (vm_size_t) bsize)
		min_size = (vm_size_t) ior->io_count;
	else
		min_size = (vm_size_t) bsize;

	/*
	 *	Map the pages from this page list into memory.
	 *	io_data records location of data.
	 *	io_alloc_size is the vm size of the region to deallocate.
	 */
	io_copy = (vm_map_copy_t) ior->io_data;
	result = kmem_io_map_copyout(device_io_map,
			(vm_offset_t*)&ior->io_data, &new_addr,
		        &ior->io_alloc_size, io_copy, min_size);
	if (result != KERN_SUCCESS)
	    return (result);

	if ((ior->io_data + ior->io_count) >
	    (((char *)new_addr) + ior->io_alloc_size)) {

		/*
		 *	Operation has to be split.  Reset io_count for how
		 *	much we can do this time.
		 */
		assert(vm_map_copy_has_cont(io_copy));
		assert(ior->io_count == io_copy->size);
		ior->io_count = ior->io_alloc_size -
			(ior->io_data - ((char *)new_addr));

		/*
		 *	Caller must wait synchronously.
		 */
		ior->io_op &= ~IO_CALL;
		*wait = TRUE;
	}

	ior->io_copy = io_copy;			/* vm_map_copy to discard */
	return (KERN_SUCCESS);
}

/*
 * Clean up memory allocated for IO.
 */
boolean_t
device_write_dealloc(io_req_t ior)
{
	vm_map_copy_t	new_copy = VM_MAP_COPY_NULL;
	vm_map_copy_t	io_copy;
	kern_return_t	result;
	vm_offset_t	size_to_do;
	int		bsize;

	if (ior->io_alloc_size == 0)
	    return (TRUE);

	/*
	 * Inband case.
	 */
	if (ior->io_op & IO_INBAND) {
	    kmem_cache_free(&io_inband_cache, (vm_offset_t)ior->io_data);

	    return (TRUE);
	}

	if ((io_copy = ior->io_copy) == VM_MAP_COPY_NULL)
	    return (TRUE);

	/*
	 *	To prevent a possible deadlock with the default pager,
	 *	we have to release space in the device_io_map before
	 *	we allocate any memory.  (Which vm_map_copy_invoke_cont
	 *	might do.)  See the discussion in mach_device_init.
	 */

	kmem_io_map_deallocate(device_io_map,
		     	 trunc_page(ior->io_data),
			 ior->io_alloc_size);

	if (vm_map_copy_has_cont(io_copy)) {

		/*
		 *	Remember how much is left, then
		 *	invoke or abort the continuation.
		 */
		size_to_do = io_copy->size - ior->io_count;
		if (ior->io_error == 0) {
			vm_map_copy_invoke_cont(io_copy, &new_copy, &result);
		}
		else {
			vm_map_copy_abort_cont(io_copy);
			result = KERN_FAILURE;
		}

		if (result == KERN_SUCCESS && new_copy != VM_MAP_COPY_NULL) {
			int		res;

			/*
			 *	We have a new continuation, reset the ior to
			 *	represent the remainder of the request.  Must
			 *	adjust the recnum because drivers assume
			 *	that the residual is zero.
			 */
			ior->io_op &= ~IO_DONE;
			ior->io_op |= IO_CALL;

			res = (*ior->io_device->dev_ops->d_dev_info)(
					ior->io_device->dev_number,
					D_INFO_BLOCK_SIZE,
					&bsize);

			if (res != D_SUCCESS)
				panic("device_write_dealloc: No block size");

			ior->io_recnum += ior->io_count/bsize;
			ior->io_count = new_copy->size;
		}
		else {

			/*
			 *	No continuation.  Add amount we didn't get
			 *	to into residual.
			 */
			ior->io_residual += size_to_do;
		}
	}

	/*
	 *	Clean up the state for the IO that just completed.
	 */
        vm_map_copy_discard(ior->io_copy);
	ior->io_copy = VM_MAP_COPY_NULL;
	ior->io_data = (char *) new_copy;

	/*
	 *	Return FALSE if there's more IO to do.
	 */

	return(new_copy == VM_MAP_COPY_NULL);
}

/*
 * Send write completion message to client, and discard the data.
 */
boolean_t
ds_write_done(const io_req_t ior)
{
	/*
	 *	device_write_dealloc discards the data that has been
	 *	written, but may decide that there is more to write.
	 */
	while (!device_write_dealloc(ior)) {
		io_return_t		result;
		mach_device_t		device;

		/*
		 *     More IO to do -- invoke it.
		 */
		device = ior->io_device;
		result = (*device->dev_ops->d_write)(device->dev_number, ior);

		/*
		 * If the IO was queued, return FALSE -- not done yet.
		 */
		if (result == D_IO_QUEUED)
		    return (FALSE);
	}

	/*
	 *	Now the write is really complete.  Send reply.
	 */

	if (IP_VALID(ior->io_reply_port)) {
	    (void) (*((ior->io_op & IO_INBAND) ?
		      ds_device_write_reply_inband :
		      ds_device_write_reply))(ior->io_reply_port,
					      ior->io_reply_port_type,
					      ior->io_error,
					      (int) (ior->io_total -
						     ior->io_residual));
	}
	mach_device_deallocate(ior->io_device);

	return (TRUE);
}

/*
 * Read from a device.
 */
static io_return_t
device_read(void *dev,
	    const ipc_port_t	reply_port,
	    mach_msg_type_name_t	reply_port_type,
	    dev_mode_t		mode,
	    recnum_t		recnum,
	    int			bytes_wanted,
	    io_buf_ptr_t	*data,
	    unsigned int	*data_count)
{
	mach_device_t 		device = dev;
	io_req_t		ior;
	io_return_t		result;

	if (device->state != DEV_STATE_OPEN)
	    return (D_NO_SUCH_DEVICE);

	/* XXX note that a CLOSE may proceed at any point */

	/*
	 * There must be a reply port.
	 */
	if (!IP_VALID(reply_port)) {
	    printf("ds_* invalid reply port\n");
	    SoftDebugger("ds_* reply_port");
	    return (MIG_NO_REPLY);	/* no sense in doing anything */
	}

	/*
	 * Package the read request for the device driver
	 */
	io_req_alloc(ior, 0);

	ior->io_device		= device;
	ior->io_unit		= device->dev_number;
	ior->io_op		= IO_READ | IO_CALL;
	ior->io_mode		= mode;
	ior->io_recnum		= recnum;
	ior->io_data		= 0;		/* driver must allocate data */
	ior->io_count		= bytes_wanted;
	ior->io_alloc_size	= 0;		/* no data allocated yet */
	ior->io_residual	= 0;
	ior->io_error		= 0;
	ior->io_done		= ds_read_done;
	ior->io_reply_port	= reply_port;
	ior->io_reply_port_type	= reply_port_type;

	/*
	 * The ior keeps an extra reference for the device.
	 */
	mach_device_reference(device);

	/*
	 * And do the read.
	 */
	result = (*device->dev_ops->d_read)(device->dev_number, ior);

	/*
	 * If the IO was queued, delay reply until it is finished.
	 */
	if (result == D_IO_QUEUED)
	    return (MIG_NO_REPLY);

	/*
	 * Return result via ds_read_done.
	 */
	ior->io_error = result;
	(void) ds_read_done(ior);
	io_req_free(ior);

	return (MIG_NO_REPLY);	/* reply has already been sent. */
}

/*
 * Read from a device, but return the data 'inband.'
 */
static io_return_t
device_read_inband(void			*dev,
		   const ipc_port_t	reply_port,
		   mach_msg_type_name_t	reply_port_type,
		   dev_mode_t		mode,
		   recnum_t		recnum,
		   int			bytes_wanted,
		   char			*data,
		   unsigned int		*data_count)
{
	mach_device_t		device = dev;
	io_req_t		ior;
	io_return_t		result;

	if (device->state != DEV_STATE_OPEN)
	    return (D_NO_SUCH_DEVICE);

	/* XXX note that a CLOSE may proceed at any point */

	/*
	 * There must be a reply port.
	 */
	if (!IP_VALID(reply_port)) {
	    printf("ds_* invalid reply port\n");
	    SoftDebugger("ds_* reply_port");
	    return (MIG_NO_REPLY);	/* no sense in doing anything */
	}

	/*
	 * Package the read for the device driver
	 */
	io_req_alloc(ior, 0);

	ior->io_device		= device;
	ior->io_unit		= device->dev_number;
	ior->io_op		= IO_READ | IO_CALL | IO_INBAND;
	ior->io_mode		= mode;
	ior->io_recnum		= recnum;
	ior->io_data		= 0;		/* driver must allocate data */
	ior->io_count		=
	    ((bytes_wanted < sizeof(io_buf_ptr_inband_t)) ?
		bytes_wanted : sizeof(io_buf_ptr_inband_t));
	ior->io_alloc_size	= 0;		/* no data allocated yet */
	ior->io_residual	= 0;
	ior->io_error		= 0;
	ior->io_done		= ds_read_done;
	ior->io_reply_port	= reply_port;
	ior->io_reply_port_type	= reply_port_type;

	/*
	 * The ior keeps an extra reference for the device.
	 */
	mach_device_reference(device);

	/*
	 * Do the read.
	 */
	result = (*device->dev_ops->d_read)(device->dev_number, ior);

	/*
	 * If the io was queued, delay reply until it is finished.
	 */
	if (result == D_IO_QUEUED)
	    return (MIG_NO_REPLY);

	/*
	 * Return result, via ds_read_done.
	 */
	ior->io_error = result;
	(void) ds_read_done(ior);
	io_req_free(ior);

	return (MIG_NO_REPLY);	/* reply has already been sent. */
}

/*
 * Allocate wired-down memory for device read.
 */
kern_return_t device_read_alloc(
	io_req_t		ior,
	vm_size_t		size)
{
	vm_offset_t		addr;
	kern_return_t		kr;

	/*
	 * Nothing to do if no data.
	 */
	if (ior->io_count == 0)
	    return (KERN_SUCCESS);

	if (ior->io_op & IO_INBAND) {
	    ior->io_data = (io_buf_ptr_t) kmem_cache_alloc(&io_inband_cache);
	    ior->io_alloc_size = sizeof(io_buf_ptr_inband_t);
	} else {
	    size = round_page(size);
	    kr = kmem_alloc(kernel_map, &addr, size);
	    if (kr != KERN_SUCCESS)
		return (kr);

	    ior->io_data = (io_buf_ptr_t) addr;
	    ior->io_alloc_size = size;
	}

	return (KERN_SUCCESS);
}

boolean_t ds_read_done(const io_req_t ior)
{
	vm_offset_t		start_data, end_data;
	vm_offset_t		start_sent, end_sent;
	vm_size_t		size_read;

	if (ior->io_error)
	    size_read = 0;
	else
	    size_read = ior->io_count - ior->io_residual;

	start_data  = (vm_offset_t)ior->io_data;
	end_data    = start_data + size_read;

	start_sent  = (ior->io_op & IO_INBAND) ? start_data :
						trunc_page(start_data);
	end_sent    = (ior->io_op & IO_INBAND) ?
		start_data + ior->io_alloc_size : round_page(end_data);

	/*
	 * Zero memory that the device did not fill.
	 */
	if (start_sent < start_data)
	    memset((void *)start_sent, 0, start_data - start_sent);
	if (end_sent > end_data)
	    memset((void *)end_data, 0, end_sent - end_data);


	/*
	 * Touch the data being returned, to mark it dirty.
	 * If the pages were filled by DMA, the pmap module
	 * may think that they are clean.
	 */
	{
	    vm_offset_t			touch;
	    int				c;

	    for (touch = start_sent; touch < end_sent; touch += PAGE_SIZE) {
		c = *(volatile char *)touch;
		*(volatile char *)touch = c;
	    }
	}

	/*
	 * Send the data to the reply port - this
	 * unwires and deallocates it.
	 */
	if (ior->io_op & IO_INBAND) {
	    (void)ds_device_read_reply_inband(ior->io_reply_port,
					      ior->io_reply_port_type,
					      ior->io_error,
					      (char *) start_data,
					      size_read);
	} else {
	    vm_map_copy_t copy;
	    kern_return_t kr;

	    kr = vm_map_copyin_page_list(kernel_map, start_data,
						 size_read, TRUE, TRUE,
						 &copy, FALSE);

	    if (kr != KERN_SUCCESS)
		panic("read_done: vm_map_copyin_page_list failed");

	    (void)ds_device_read_reply(ior->io_reply_port,
				       ior->io_reply_port_type,
				       ior->io_error,
				       (char *) copy,
				       size_read);
	}

	/*
	 * Free any memory that was allocated but not sent.
	 */
	if (ior->io_count != 0) {
	    if (ior->io_op & IO_INBAND) {
		if (ior->io_alloc_size > 0)
		    kmem_cache_free(&io_inband_cache, (vm_offset_t)ior->io_data);
	    } else {
		vm_offset_t		end_alloc;

		end_alloc = start_sent + round_page(ior->io_alloc_size);
		if (end_alloc > end_sent)
		    (void) vm_deallocate(kernel_map,
					 end_sent,
					 end_alloc - end_sent);
	    }
	}

	mach_device_deallocate(ior->io_device);

	return (TRUE);
}

static io_return_t
device_set_status(
	void 			*dev,
	dev_flavor_t		flavor,
	dev_status_t		status,
	mach_msg_type_number_t	status_count)
{
	mach_device_t		device = dev;
	if (device->state != DEV_STATE_OPEN)
	    return (D_NO_SUCH_DEVICE);

	/* XXX note that a CLOSE may proceed at any point */

	return ((*device->dev_ops->d_setstat)(device->dev_number,
					      flavor,
					      status,
					      status_count));
}

io_return_t
mach_device_get_status(
	void			*dev,
	dev_flavor_t		flavor,
	dev_status_t		status,		/* pointer to OUT array */
	mach_msg_type_number_t	*status_count)	/* out */
{
	mach_device_t		device = dev;
	if (device->state != DEV_STATE_OPEN)
	    return (D_NO_SUCH_DEVICE);

	/* XXX note that a CLOSE may proceed at any point */

	return ((*device->dev_ops->d_getstat)(device->dev_number,
					      flavor,
					      status,
					      status_count));
}

static io_return_t
device_set_filter(void			*dev,
		  const ipc_port_t	receive_port,
		  int			priority,
		  filter_t		filter[],
		  unsigned int		filter_count)
{
	mach_device_t		device = dev;
	if (device->state != DEV_STATE_OPEN)
	    return (D_NO_SUCH_DEVICE);

	/* XXX note that a CLOSE may proceed at any point */

	/*
	 * Request is absurd if no receive port is specified.
	 */
	if (!IP_VALID(receive_port))
	    return (D_INVALID_OPERATION);

	return ((*device->dev_ops->d_async_in)(device->dev_number,
					       receive_port,
					       priority,
					       filter,
					       filter_count));
}

static io_return_t
device_map(
	void 			*dev,
	vm_prot_t		protection,
	vm_offset_t		offset,
	vm_size_t		size,
	ipc_port_t		*pager,	/* out */
	boolean_t		unmap)	/* ? */
{
	mach_device_t		device = dev;
	if (protection & ~VM_PROT_ALL)
		return (KERN_INVALID_ARGUMENT);

	if (device->state != DEV_STATE_OPEN)
	    return (D_NO_SUCH_DEVICE);

	/* XXX note that a CLOSE may proceed at any point */

	return (device_pager_setup(device, protection, offset, size,
				    (mach_port_t*)pager));
}

/*
 * Doesn't do anything (yet).
 */
static void
ds_no_senders(mach_no_senders_notification_t *notification)
{
	printf("ds_no_senders called! device_port=0x%lx count=%d\n",
	       notification->not_header.msgh_remote_port,
	       notification->not_count);
}

queue_head_t		io_done_list;
decl_simple_lock_data(,	io_done_list_lock)

#define	splio	splsched	/* XXX must block ALL io devices */

void iodone(io_req_t ior)
{
	spl_t			s;

	/*
	 * If this ior was loaned to us, return it directly.
	 */
	if (ior->io_op & IO_LOANED) {
		(*ior->io_done)(ior);
		return;
	}
	/*
	 * If !IO_CALL, some thread is waiting for this.  Must lock
	 * structure to interlock correctly with iowait().  Else can
	 * toss on queue for io_done thread to call completion.
	 */
	s = splio();
	if ((ior->io_op & IO_CALL) == 0) {
	    ior_lock(ior);
	    ior->io_op |= IO_DONE;
	    ior->io_op &= ~IO_WANTED;
	    ior_unlock(ior);
	    thread_wakeup((event_t)ior);
	} else {
	    ior->io_op |= IO_DONE;
	    simple_lock(&io_done_list_lock);
	    enqueue_tail(&io_done_list, (queue_entry_t)ior);
	    thread_wakeup((event_t)&io_done_list);
	    simple_unlock(&io_done_list_lock);
	}
	splx(s);
}

void  __attribute__ ((noreturn)) io_done_thread_continue(void)
{
	for (;;) {
	    spl_t		s;
	    io_req_t		ior;

#if defined (LINUX_DEV) && defined (CONFIG_INET)
	    free_skbuffs ();
#endif
	    s = splio();
	    simple_lock(&io_done_list_lock);
	    while ((ior = (io_req_t)dequeue_head(&io_done_list)) != 0) {
		simple_unlock(&io_done_list_lock);
		(void) splx(s);

		if ((*ior->io_done)(ior)) {
		    /*
		     * IO done - free io_req_elt
		     */
		    io_req_free(ior);
		}
		/* else routine has re-queued it somewhere */

		s = splio();
		simple_lock(&io_done_list_lock);
	    }

	    assert_wait(&io_done_list, FALSE);
	    simple_unlock(&io_done_list_lock);
	    (void) splx(s);
	    counter(c_io_done_thread_block++);
	    thread_block(io_done_thread_continue);
	}
}

void io_done_thread(void)
{
	/*
	 * Set thread privileges and highest priority.
	 */
	current_thread()->vm_privilege = 1;
	stack_privilege(current_thread());
	thread_set_own_priority(0);

	io_done_thread_continue();
	/*NOTREACHED*/
}

#define	DEVICE_IO_MAP_SIZE	(16 * 1024 * 1024)

static void mach_device_trap_init(void);		/* forward */

void mach_device_init(void)
{
	vm_offset_t	device_io_min, device_io_max;

	queue_init(&io_done_list);
	simple_lock_init(&io_done_list_lock);

	kmem_submap(device_io_map, kernel_map, &device_io_min, &device_io_max,
		    DEVICE_IO_MAP_SIZE);

	/*
	 *	If the kernel receives many device_write requests, the
	 *	device_io_map might run out of space.  To prevent
	 *	device_write_get from failing in this case, we enable
	 *	wait_for_space on the map.  This causes kmem_io_map_copyout
	 *	to block until there is sufficient space.
	 *	(XXX Large writes may be starved by small writes.)
	 *
	 *	There is a potential deadlock problem with this solution,
	 *	if a device_write from the default pager has to wait
	 *	for the completion of a device_write which needs to wait
	 *	for memory allocation.  Hence, once device_write_get
	 *	allocates space in device_io_map, no blocking memory
	 *	allocations should happen until device_write_dealloc
	 *	frees the space.  (XXX A large write might starve
	 *	a small write from the default pager.)
	 */
	device_io_map->wait_for_space = TRUE;

	kmem_cache_init(&io_inband_cache, "io_buf_ptr_inband",
			sizeof(io_buf_ptr_inband_t), 0, NULL, 0);

	mach_device_trap_init();
}

void iowait(io_req_t ior)
{
    spl_t s;

    s = splio();
    ior_lock(ior);
    while ((ior->io_op&IO_DONE)==0) {
	assert_wait((event_t)ior, FALSE);
	ior_unlock(ior);
	thread_block((void (*)()) 0);
        ior_lock(ior);
    }
    ior_unlock(ior);
    splx(s);
}


/*
 * Device trap support.
 */

/*
 * Memory Management
 *
 * This currently has a single pool of 2k wired buffers
 * since we only handle writes to an ethernet device.
 * Should be more general.
 */
#define IOTRAP_REQSIZE 2048

struct kmem_cache io_trap_cache;

/*
 * Initialization.  Called from mach_device_init().
 */
static void
mach_device_trap_init(void)
{
	kmem_cache_init(&io_trap_cache, "io_req", IOTRAP_REQSIZE, 0,
			NULL, 0);
}

/*
 * Allocate an io_req_t.
 * Currently allocates from io_trap_cache.
 *
 * Could have lists of different size caches.
 * Could call a device-specific routine.
 */
io_req_t
ds_trap_req_alloc(const mach_device_t device, vm_size_t data_size)
{
	return (io_req_t) kmem_cache_alloc(&io_trap_cache);
}

/*
 * Called by iodone to release ior.
 */
boolean_t
ds_trap_write_done(const io_req_t ior)
{
	mach_device_t 	dev;

	dev = ior->io_device;

	/*
	 * Should look at reply port and maybe send a message.
	 */
	kmem_cache_free(&io_trap_cache, (vm_offset_t) ior);

	/*
	 * Give up device reference from ds_write_trap.
	 */
	mach_device_deallocate(dev);
	return TRUE;
}

/*
 * Like device_write except that data is in user space.
 */
static io_return_t
device_write_trap (mach_device_t device, dev_mode_t mode,
		   recnum_t recnum, vm_offset_t data, vm_size_t data_count)
{
	io_req_t ior;
	io_return_t result;

	if (device->state != DEV_STATE_OPEN)
		return (D_NO_SUCH_DEVICE);

	/* XXX note that a CLOSE may proceed at any point */

	/*
	 * Get a buffer to hold the ioreq.
	 */
	ior = ds_trap_req_alloc(device, data_count);

	/*
	 * Package the write request for the device driver.
	 */

	ior->io_device          = device;
	ior->io_unit            = device->dev_number;
	ior->io_op              = IO_WRITE | IO_CALL | IO_LOANED;
	ior->io_mode            = mode;
	ior->io_recnum          = recnum;
	ior->io_data            = (io_buf_ptr_t)
		(vm_offset_t)ior + sizeof(struct io_req);
	ior->io_count           = data_count;
	ior->io_total           = data_count;
	ior->io_alloc_size      = 0;
	ior->io_residual        = 0;
	ior->io_error           = 0;
	ior->io_done            = ds_trap_write_done;
	ior->io_reply_port      = IP_NULL;	/* XXX */
	ior->io_reply_port_type = 0;		/* XXX */

	/*
	 * Copy the data from user space.
	 */
	if (data_count > 0)
		copyin((void *)data, ior->io_data, data_count);

	/*
	 * The ior keeps an extra reference for the device.
	 */
	mach_device_reference(device);

	/*
	 * And do the write.
	 */
	result = (*device->dev_ops->d_write)(device->dev_number, ior);

	/*
	 * If the IO was queued, delay reply until it is finished.
	 */
	if (result == D_IO_QUEUED)
		return (MIG_NO_REPLY);

	/*
	 * Remove the extra reference.
	 */
	mach_device_deallocate(device);

	kmem_cache_free(&io_trap_cache, (vm_offset_t) ior);
	return (result);
}

static io_return_t
device_writev_trap (mach_device_t device, dev_mode_t mode,
		    recnum_t recnum, io_buf_vec_t *iovec, vm_size_t iocount)
{
	io_req_t ior;
	io_return_t result;
	io_buf_vec_t	stack_iovec[16]; /* XXX */
	vm_size_t data_count;
	unsigned i;

	if (device->state != DEV_STATE_OPEN)
		return (D_NO_SUCH_DEVICE);

	/* XXX note that a CLOSE may proceed at any point */

	/*
	 * Copyin user addresses.
	 */
	if (iocount > 16)
		return KERN_INVALID_VALUE; /* lame */
	copyin(iovec,
	       stack_iovec,
	       iocount * sizeof(io_buf_vec_t));
	for (data_count = 0, i = 0; i < iocount; i++)
		data_count += stack_iovec[i].count;

	/*
	 * Get a buffer to hold the ioreq.
	 */
	ior = ds_trap_req_alloc(device, data_count);

	/*
	 * Package the write request for the device driver.
	 */

	ior->io_device          = device;
	ior->io_unit            = device->dev_number;
	ior->io_op              = IO_WRITE | IO_CALL | IO_LOANED;
	ior->io_mode            = mode;
	ior->io_recnum          = recnum;
	ior->io_data            = (io_buf_ptr_t)
		(vm_offset_t)ior + sizeof(struct io_req);
	ior->io_count           = data_count;
	ior->io_total           = data_count;
	ior->io_alloc_size      = 0;
	ior->io_residual        = 0;
	ior->io_error           = 0;
	ior->io_done            = ds_trap_write_done;
	ior->io_reply_port      = IP_NULL;	/* XXX */
	ior->io_reply_port_type = 0;		/* XXX */

	/*
	 * Copy the data from user space.
	 */
	if (data_count > 0) {
		vm_offset_t p;

		p = (vm_offset_t) ior->io_data;
		for (i = 0; i < iocount; i++) {
			copyin((void *) stack_iovec[i].data,
			       (void *) p,
			       stack_iovec[i].count);
			p += stack_iovec[i].count;
		}
	}

	/*
	 * The ior keeps an extra reference for the device.
	 */
	mach_device_reference(device);

	/*
	 * And do the write.
	 */
	result = (*device->dev_ops->d_write)(device->dev_number, ior);

	/*
	 * If the IO was queued, delay reply until it is finished.
	 */
	if (result == D_IO_QUEUED)
		return (MIG_NO_REPLY);

	/*
	 * Remove the extra reference.
	 */
	mach_device_deallocate(device);

	kmem_cache_free(&io_trap_cache, (vm_offset_t) ior);
	return (result);
}

struct device_emulation_ops mach_device_emulation_ops =
{
  (void*) mach_device_reference,
  (void*) mach_device_deallocate,
  (void*) mach_convert_device_to_port,
  device_open,
  device_close,
  device_write,
  device_write_inband,
  device_read,
  device_read_inband,
  device_set_status,
  mach_device_get_status,
  device_set_filter,
  device_map,
  ds_no_senders,
  (void*) device_write_trap,
  (void*) device_writev_trap
};
