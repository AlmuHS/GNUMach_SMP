/*
 * pcmcia-socket `device' driver
 *
 * Copyright (C) 2006, 2007 Free Software Foundation, Inc.
 * Written by Stefan Siegl <stesie@brokenpipe.de>.
 *
 * This file is part of GNU Mach.
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
 */

/* This file is included from linux/pcmcia-cs/modules/ds.c.  */

/*
 * Prepare the namespace for inclusion of Mach header files.
 */

#undef PAGE_SHIFT

/*
 * This is really ugly.  But this is glue code, so...  It's about the `kfree'
 * symbols in <linux/malloc.h> and <kern/kalloc.h>.
 */
#undef kfree

/*
 * <kern/sched_prim.h> defines another event_t which is not used in this
 * file, so name it mach_event_t to avoid a clash.
 */
#define event_t mach_event_t
#include <kern/sched_prim.h>
#undef event_t

#include <mach/port.h>
#include <mach/notify.h>
#include <mach/mig_errors.h>

#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <device/device_types.h>
#include <device/device_port.h>
#include <device/io_req.h>
#include <device/ds_routines.h>

#include <device/device_emul.h>

#include <device/device_reply.user.h>

/* Eliminate the queue_empty macro from Mach header files.  */
#undef queue_empty

struct device_emulation_ops linux_pcmcia_emulation_ops;

/*
 * We have our very own device emulation stack because we need to carry a
 * pointer from the open call via read until the final close call: a
 * pointer to the user's event queue.
 */
struct mach_socket_device {
  /*
   * Pointer to the mach_device we have allocated.  This must be the
   * first entry in this struct, in order to be able to cast to
   * mach_device.
   */
  struct mach_device   mach_dev;

  /*
   * Pointer to the user info of pcmcia data services.
   */
  user_info_t          *user;

  /*
   * Cache for carrying data from set_status to get_status calls.  This
   * is needed for write ioctls.
   */
  ds_ioctl_arg_t       carry;
};


static void
ds_device_deallocate(void *p)
{
  mach_device_t device = (mach_device_t) p;

  simple_lock(&device->ref_lock);
  if (--device->ref_count > 0)
    {
      simple_unlock(&device->ref_lock);
      return;
    }

  simple_unlock(&device->ref_lock);
  
  /*
   * do what the original ds_release would do, ...
   */
  socket_t i = device->dev_number;
  socket_info_t *s;
  user_info_t *user, **link;

  s = &socket_table[i];
  user = ((struct mach_socket_device *) device)->user;

  /* allow to access the device again ... */
  if(device->flag & D_WRITE)
    s->state &= ~SOCKET_BUSY;

  /* Unlink user data structure */
  for (link = &s->user; *link; link = &(*link)->next)
    if (*link == user) break;

  if(link)
    {
      *link = user->next;
      user->user_magic = 0;
      linux_kfree(user);
    }

  /* now finally reap the device */
  linux_kfree(device);
}

/*
 * Return the send right associated with this socket device incarnation.
 */
static ipc_port_t
dev_to_port(void *d)
{
  struct mach_device *dev = d;

  if(! dev) 
    return IP_NULL;

  ipc_port_t port = ipc_port_make_send(dev->port);

  ds_device_deallocate(dev);
  return port;
}


static inline int
atoi(const char *ptr)
{
  if(! ptr) 
    return 0;

  int i = 0;
  while(*ptr >= '0' && *ptr <= '9')
    i = i * 10 + *(ptr ++) - '0';

  return i;
}


/*
 * Try to open the per-socket pseudo device `socket%d'.
 */
static io_return_t
device_open (ipc_port_t reply_port, mach_msg_type_name_t reply_port_type,
	     dev_mode_t mode, char *name, device_t *devp /* out */)
{
  if(! socket_table)
    return D_NO_SUCH_DEVICE;

  if(strlen(name) < 7 || strncmp(name, "socket", 6))
    return D_NO_SUCH_DEVICE;

  socket_t i = atoi(name + 6);
  if(i >= MAX_SOCKS || i >= sockets)
    return D_NO_SUCH_DEVICE;

  io_return_t err = D_SUCCESS;

  struct mach_device *dev;
  dev = linux_kmalloc(sizeof(struct mach_socket_device), GFP_KERNEL);
  if(! dev)
    {
      err = D_NO_MEMORY;
      goto out;
    }

  memset(dev, 0, sizeof(struct mach_socket_device));
  mach_device_reference(dev);

  /* now do, what ds_open would do if it would be in charge */
  socket_info_t *s = &socket_table[i];

  if(mode & D_WRITE)
    {
      if(s->state & SOCKET_BUSY)
	{
	  err = D_ALREADY_OPEN;
	  goto out;
	}
      else
	s->state |= SOCKET_BUSY;
    }

  user_info_t *user = linux_kmalloc(sizeof(user_info_t), GFP_KERNEL);
  if(! user) 
    {
      err = D_NO_MEMORY;
      goto out;
    }

  user->event_tail = user->event_head = 0;
  user->next = s->user;
  user->user_magic = USER_MAGIC;
  s->user = user;

  ((struct mach_socket_device *) dev)->user = user;

  if(s->state & SOCKET_PRESENT)
    queue_event(user, CS_EVENT_CARD_INSERTION);

  /* just set up the rest of our mach_device now ... */
  dev->dev.emul_ops = &linux_pcmcia_emulation_ops;
  dev->dev.emul_data = dev;

  dev->dev_number = i;
  dev->flag = mode;

  dev->port = ipc_port_alloc_kernel();
  if(dev->port == IP_NULL)
    {
      err = KERN_RESOURCE_SHORTAGE;
      goto out;
    }

  mach_device_reference(dev);
  ipc_kobject_set(dev->port, (ipc_kobject_t) &dev->dev, IKOT_DEVICE);

  /* request no-senders notifications on device port */
  ipc_port_t notify = ipc_port_make_sonce(dev->port);
  ip_lock(dev->port);
  ipc_port_nsrequest(dev->port, 1, notify, &notify);
  assert (notify == IP_NULL);

 out:
  if(err)
    {
      if(dev)
	{
	  if(dev->port != IP_NULL)
	    {
	      ipc_kobject_set(dev->port, IKO_NULL, IKOT_NONE);
	      ipc_port_dealloc_kernel(dev->port);
	    }

	  linux_kfree(dev);
	  dev = NULL;
	}
    }
  else
    dev->state = DEV_STATE_OPEN;

  *devp = &dev->dev;

  if (IP_VALID (reply_port))
    ds_device_open_reply(reply_port, reply_port_type,
			 err, dev_to_port(dev));
  return MIG_NO_REPLY;
}


/*
 * Close the device DEV.
 */
static int
device_close (void *devp)
{
  struct mach_device *dev = (struct mach_device *) devp;

  dev->state = DEV_STATE_CLOSING;

  /* check whether there is a blocked read request pending,
   * in that case, abort that one before closing 
   */
  while(dev->ref_count > 2)
    {
      socket_t i = dev->dev_number;
      socket_info_t *s = &socket_table[i];
      wake_up_interruptible(&s->queue);
      
      /* wait for device_read to exit */
      return D_INVALID_OPERATION;
    }

  dev_port_remove(dev);
  ipc_port_dealloc_kernel(dev->port);

  return 0;
}

static io_return_t
device_read(void *d, ipc_port_t reply_port,
	    mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	    recnum_t recnum, int bytes_wanted,
	    io_buf_ptr_t *data, unsigned int *data_count)
{
  struct mach_device *dev = (struct mach_device *) d;

  if(dev->state != DEV_STATE_OPEN)
    return D_NO_SUCH_DEVICE;

  if(! IP_VALID(reply_port)) {
    printk(KERN_INFO "ds: device_read: invalid reply port.\n");
    return (MIG_NO_REPLY);      /* no sense in doing anything */
  }

  /* prepare an io request structure */
  io_req_t ior;
  io_req_alloc(ior, 0);

  ior->io_device = dev;
  ior->io_unit = dev->dev_number;
  ior->io_op = IO_READ | IO_CALL;
  ior->io_mode = mode;
  ior->io_recnum = recnum;
  ior->io_data = 0;
  ior->io_count	= bytes_wanted;
  ior->io_alloc_size = 0;
  ior->io_residual = 0;
  ior->io_error = 0;
  ior->io_done = ds_read_done;
  ior->io_reply_port = reply_port;
  ior->io_reply_port_type = reply_port_type;

  /*
   * The ior keeps an extra reference for the device.
   */
  mach_device_reference(dev);

  /* do the read finally */
  io_return_t result = D_SUCCESS;
  
  result = device_read_alloc(ior, ior->io_count);
  if(result != KERN_SUCCESS)
    goto out;

  socket_t i = dev->dev_number;
  socket_info_t *s = &socket_table[i];
  user_info_t *user = ((struct mach_socket_device *) dev)->user;

  if(ior->io_count < 4)
    return D_INVALID_SIZE;

  if(CHECK_USER(user))
    {
      result = D_IO_ERROR;
      goto out;
    }

  while(queue_empty(user))
    {
      if(ior->io_mode & D_NOWAIT)
	{
	  result = D_WOULD_BLOCK;
	  goto out;
	}
      else
	interruptible_sleep_on(&s->queue);
   
      if(dev->state == DEV_STATE_CLOSING)
	{
	  result = D_DEVICE_DOWN;
	  goto out;
	}
    }
  
  event_t ev = get_queued_event(user);
  memcpy(ior->io_data, &ev, sizeof(event_t));

  ior->io_residual = ior->io_count - sizeof(event_t);
  
 out:
  /*
   * Return result via ds_read_done.
   */
  ior->io_error = result;
  (void) ds_read_done(ior);
  io_req_free(ior);

  return (MIG_NO_REPLY);	/* reply has already been sent. */
}


static io_return_t
device_set_status(void *d, dev_flavor_t req, dev_status_t arg,
		  mach_msg_type_number_t sz)
{
  struct mach_socket_device *dev = (struct mach_socket_device *) d;

  if(sz * sizeof(int) > sizeof(ds_ioctl_arg_t))
    return D_INVALID_OPERATION;

  if(dev->mach_dev.state != DEV_STATE_OPEN)
    return D_NO_SUCH_DEVICE;

  unsigned int ioctl_sz = (req & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
  memcpy(&dev->carry, arg, ioctl_sz);

  return D_SUCCESS;
}

static io_return_t
device_get_status(void *d, dev_flavor_t req, dev_status_t arg,
		  mach_msg_type_number_t *sz)
{
  struct mach_socket_device *dev = (struct mach_socket_device *) d;

  if(dev->mach_dev.state != DEV_STATE_OPEN)
    return D_NO_SUCH_DEVICE;

  struct inode inode;
  inode.i_rdev = dev->mach_dev.dev_number;
  int ret = ds_ioctl(&inode, NULL, req, (u_long) &dev->carry);

  if(ret)
    return D_IO_ERROR;

  unsigned int ioctl_sz = (req & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
  if(req & IOC_OUT) memcpy(arg, &dev->carry, ioctl_sz);

  return D_SUCCESS;
}


struct device_emulation_ops linux_pcmcia_emulation_ops =
  {
    (void*) mach_device_reference,
    ds_device_deallocate,
    dev_to_port,
    device_open,
    device_close,
    NULL, /* device_write */
    NULL, /* write_inband */
    device_read,
    NULL, /* read_inband */
    device_set_status,
    device_get_status,
    NULL, /* set_filter */
    NULL, /* map */
    NULL, /* no_senders */
    NULL, /* write_trap */
    NULL /* writev_trap */
  };
