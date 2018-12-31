/* Manipulate I/O permission bitmap objects.

   Copyright (C) 2002, 2007 Free Software Foundation, Inc.

   Written by Marcus Brinkmann.  Glued into GNU Mach by Thomas Schwinge.

   This file is part of GNU Mach.

   GNU Mach is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/*
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
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

#include <string.h>

#include <mach/boolean.h>
#include <mach/kern_return.h>

#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <kern/slab.h>
#include <kern/kalloc.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <kern/thread.h>

#include <device/dev_hdr.h>
#include <device/device_emul.h>
#include <device/device_port.h>

#include "io_perm.h"
#include "gdt.h"
#include "pcb.h"

#define PCI_CFG1_START	0xcf8
#define PCI_CFG1_END	0xcff
#define PCI_CFG2_START	0xc000
#define PCI_CFG2_END	0xcfff

#define CONTAINS_PCI_CFG(from, to) \
  ( ( ( from <= PCI_CFG1_END ) && ( to >= PCI_CFG1_START ) ) || \
    ( ( from <= PCI_CFG2_END ) && ( to >= PCI_CFG2_START ) ) )


/* Our device emulation ops.  See below, at the bottom of this file.  */
static struct device_emulation_ops io_perm_device_emulation_ops;

/* Flag to hold PCI io cfg access lock */
static boolean_t taken_pci_cfg = FALSE;

/* The outtran which allows MIG to convert an io_perm_t object to a port
   representing it.  */
ipc_port_t
convert_io_perm_to_port (io_perm_t io_perm)
{
  if (io_perm == IO_PERM_NULL)
    return IP_NULL;

  ipc_port_t port;

  port = ipc_port_make_send (io_perm->port);

  return port;
}


/* The intran which allows MIG to convert a port representing an
   io_perm_t object to the object itself.  */
io_perm_t
convert_port_to_io_perm (ipc_port_t port)
{
  device_t device;

  device = dev_port_lookup (port);

  if (device == DEVICE_NULL)
    return IO_PERM_NULL;

  io_perm_t io_perm;

  io_perm = device->emul_data;

  return io_perm;
}

/* The destructor which is called when the last send right to a port
   representing an io_perm_t object vanishes.  */
void
io_perm_deallocate (io_perm_t io_perm)
{
  /* We need to check if the io_perm was a PCI cfg one and release it */
  if (CONTAINS_PCI_CFG(io_perm->from, io_perm->to))
    taken_pci_cfg = FALSE;
}

/* Our ``no senders'' handling routine.  Deallocate the object.  */
static
void
no_senders (mach_no_senders_notification_t *notification)
{
  io_perm_t io_perm;

  io_perm = convert_port_to_io_perm
    ((ipc_port_t) notification->not_header.msgh_remote_port);

  assert (io_perm != IO_PERM_NULL);

  ipc_kobject_set (io_perm->port, IKO_NULL, IKOT_NONE);
  ipc_port_dealloc_kernel (io_perm->port);

  kfree ((vm_offset_t) io_perm, sizeof *io_perm);
}


/* Initialize bitmap by setting all bits to OFF == 1.  */
static inline void
io_bitmap_init (unsigned char *iopb)
{
  memset (iopb, ~0, IOPB_BYTES);
}


/* Set selected bits in bitmap to ON == 0.  */
static inline void
io_bitmap_set (unsigned char *iopb, io_port_t from, io_port_t to)
{
  do
    iopb[from >> 3] &= ~(1 << (from & 0x7));
  while (from++ != to);
}


/* Set selected bits in bitmap to OFF == 1.  */
static inline void
io_bitmap_clear (unsigned char *iopb, io_port_t from, io_port_t to)
{
  do
    iopb[from >> 3] |= (1 << (from & 0x7));
  while (from++ != to);
}


/* Request a new port IO_PERM that represents the capability to access
   the I/O ports [FROM; TO] directly.  MASTER_PORT is the master device port.

   The function returns KERN_INVALID_ARGUMENT if TARGET_TASK is not a task,
   or FROM is greater than TO.

   The function is exported.  */
kern_return_t
i386_io_perm_create (const ipc_port_t master_port, io_port_t from, io_port_t to,
		     io_perm_t *new)
{
  if (master_port != master_device_port)
    return KERN_INVALID_ARGUMENT;

  /* We do not have to check FROM and TO for the limits [0;IOPB_MAX], as
     they're short integers and all values are within these very limits.  */
  if (from > to)
    return KERN_INVALID_ARGUMENT;

  /* Only one process may take a range that includes PCI cfg registers */
  if (taken_pci_cfg && CONTAINS_PCI_CFG(from, to))
    return KERN_PROTECTION_FAILURE;

  io_perm_t io_perm;

  io_perm = (io_perm_t) kalloc (sizeof *io_perm);
  if (io_perm == NULL)
    return KERN_RESOURCE_SHORTAGE;

  io_perm->from = from;
  io_perm->to = to;

  io_perm->port = ipc_port_alloc_kernel ();
  if (io_perm->port == IP_NULL)
    {
      kfree ((vm_offset_t) io_perm, sizeof *io_perm);
      return KERN_RESOURCE_SHORTAGE;
    }

  /* Set up the dummy device.  */
  ipc_kobject_set(io_perm->port,
		  (ipc_kobject_t) &io_perm->device, IKOT_DEVICE);
  io_perm->device.emul_data = io_perm;
  io_perm->device.emul_ops = &io_perm_device_emulation_ops;

  ipc_port_t notify;

  notify = ipc_port_make_sonce(io_perm->port);
  ip_lock(io_perm->port);
  ipc_port_nsrequest(io_perm->port, 1, notify, &notify);
  assert(notify == IP_NULL);

  *new = io_perm;

  if (CONTAINS_PCI_CFG(from, to))
    taken_pci_cfg = TRUE;

  return KERN_SUCCESS;
}

/* Modify the I/O permissions for TARGET_TASK.  If ENABLE is TRUE, the
   permission to access the I/O ports specified by IO_PERM is granted,
   otherwise it is withdrawn.

   The function returns KERN_INVALID_ARGUMENT if TARGET_TASK is not a valid
   task or IO_PERM not a valid I/O permission port.

   The function is exported.  */
kern_return_t
i386_io_perm_modify (task_t target_task, io_perm_t io_perm, boolean_t enable)
{
  io_port_t from, to;
  unsigned char *iopb;
  io_port_t iopb_size;

  if (target_task == TASK_NULL || io_perm == IO_PERM_NULL)
    return KERN_INVALID_ARGUMENT;

  from = io_perm->from;
  to = io_perm->to;

  simple_lock (&target_task->machine.iopb_lock);
  iopb = target_task->machine.iopb;
  iopb_size = target_task->machine.iopb_size;

  if (!enable && !iopb_size)
    {
      simple_unlock (&target_task->machine.iopb_lock);
      return KERN_SUCCESS;
    }

  if (!iopb)
    {
      simple_unlock (&target_task->machine.iopb_lock);
      iopb = (unsigned char *) kmem_cache_alloc (&machine_task_iopb_cache);
      simple_lock (&target_task->machine.iopb_lock);
      if (target_task->machine.iopb)
	{
	  if (iopb)
	    kmem_cache_free (&machine_task_iopb_cache, (vm_offset_t) iopb);
	  iopb = target_task->machine.iopb;
	  iopb_size = target_task->machine.iopb_size;
	}
      else if (iopb)
	{
	  target_task->machine.iopb = iopb;
	  io_bitmap_init (iopb);
	}
      else
	{
	  simple_unlock (&target_task->machine.iopb_lock);
	  return KERN_RESOURCE_SHORTAGE;
	}
    }

  if (enable)
    {
      io_bitmap_set (iopb, from, to);
      if ((to >> 3) + 1 > iopb_size)
	target_task->machine.iopb_size = (to >> 3) + 1;
    }
  else
    {
      if ((from >> 3) + 1 > iopb_size)
	{
	  simple_unlock (&target_task->machine.iopb_lock);
	  return KERN_SUCCESS;
	}

      io_bitmap_clear (iopb, from, to);
      while (iopb_size > 0 && iopb[iopb_size - 1] == 0xff)
	iopb_size--;
      target_task->machine.iopb_size = iopb_size;
    }

#if NCPUS>1
#warning SMP support missing (notify all CPUs running threads in that of the I/O bitmap change).
#endif
  if (target_task == current_task())
    update_ktss_iopb (iopb, target_task->machine.iopb_size);

  simple_unlock (&target_task->machine.iopb_lock);
  return KERN_SUCCESS;
}

/* We are some sort of Mach device...  */
static struct device_emulation_ops io_perm_device_emulation_ops =
{
  /* ... in order to be easily able to receive a ``no senders'' notification
     which we then use to deallocate ourselves.  */
  .no_senders = no_senders
};
