/* memory_object_proxy.c - Proxy memory objects for Mach.
   Copyright (C) 2005 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of GNU Mach.

   GNU Mach is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU Mach is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

/* A proxy memory object is a kernel port that can be used like a real
   memory object in a vm_map call, except that the current and maximum
   protection are restricted to the proxy object's maximum protection
   at the time the mapping is established.  The kernel port will hold
   a reference to the real memory object for the life time of the
   proxy object.

   Note that we don't need to do any reference counting on the proxy
   object.  Our caller will hold a reference to the proxy object when
   looking it up, and is expected to acquire its own reference to the
   real memory object if needed before releasing the reference to the
   proxy object.

   The user provided real memory object and the maximum protection are
   not checked for validity.  The maximum protection is only used as a
   mask, and the memory object is validated at the time the mapping is
   established.  */

#include <mach/port.h>
#include <mach/kern_return.h>
#include <mach/notify.h>
#include <mach/vm_prot.h>
#include <kern/printf.h>
#include <kern/slab.h>
#include <kern/mach4.server.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <vm/memory_object_proxy.h>

/* The cache which holds our proxy memory objects.  */
static struct kmem_cache memory_object_proxy_cache;

struct memory_object_proxy
{
  struct ipc_port *port;

  ipc_port_t object;
  ipc_port_t notify;
  vm_prot_t max_protection;
  vm_offset_t start;
  vm_offset_t len;
};
typedef struct memory_object_proxy *memory_object_proxy_t;


void
memory_object_proxy_init (void)
{
  kmem_cache_init (&memory_object_proxy_cache, "memory_object_proxy",
		   sizeof (struct memory_object_proxy), 0, NULL, 0);
}

/* Lookup a proxy memory object by its port.  */
static memory_object_proxy_t
memory_object_proxy_port_lookup (ipc_port_t port)
{
  memory_object_proxy_t proxy;

  if (!IP_VALID(port))
    return 0;

  ip_lock (port);
  if (ip_active (port) && (ip_kotype (port) == IKOT_PAGER_PROXY))
    proxy = (memory_object_proxy_t) port->ip_kobject;
  else
    proxy = 0;
  ip_unlock (port);
  return proxy;
}


/* Process a no-sender notification for the proxy memory object
   port.  */
boolean_t
memory_object_proxy_notify (mach_msg_header_t *msg)
{
  if (msg->msgh_id == MACH_NOTIFY_NO_SENDERS)
    {
      memory_object_proxy_t proxy;
      mach_no_senders_notification_t *ns;

      ns = (mach_no_senders_notification_t *) msg;

      proxy = (memory_object_proxy_t)
	      ((ipc_port_t) ns->not_header.msgh_remote_port)->ip_kobject;
      if (!proxy)
	return FALSE;
      if ((ipc_port_t) ns->not_header.msgh_remote_port != proxy->notify)
	return FALSE;

      ipc_port_release_send (proxy->object);

      ipc_kobject_set (proxy->port, IKO_NULL, IKOT_NONE);
      ipc_port_dealloc_kernel (proxy->port);
      ipc_kobject_set (proxy->notify, IKO_NULL, IKOT_NONE);
      ipc_port_dealloc_kernel (proxy->notify);

      kmem_cache_free (&memory_object_proxy_cache, (vm_offset_t) proxy);

      return TRUE;
    }

  printf ("memory_object_proxy_notify: strange notification %d\n",
	  msg->msgh_id);
  return FALSE;
}


/* Create a new proxy memory object from [START;START+LEN) in the
   given OBJECT at OFFSET in the new object with the maximum
   protection MAX_PROTECTION and return it in *PORT.  */
kern_return_t
memory_object_create_proxy (ipc_space_t space, vm_prot_t max_protection,
			    ipc_port_t *object, natural_t object_count,
			    vm_offset_t *offset, natural_t offset_count,
			    vm_offset_t *start, natural_t start_count,
			    vm_size_t *len, natural_t len_count,
			    ipc_port_t *port)
{
  memory_object_proxy_t proxy;
  ipc_port_t notify;

  if (space == IS_NULL)
    return KERN_INVALID_TASK;

  if (offset_count != object_count || start_count != object_count
      || len_count != object_count)
    return KERN_INVALID_ARGUMENT;

  /* FIXME: Support more than one memory object.  */
  if (object_count != 1)
    return KERN_INVALID_ARGUMENT;

  if (!IP_VALID(object[0]))
    return KERN_INVALID_NAME;

#ifdef USER32
  /* FIXME: simplify RPC, fix mig, or add a new VM data type in message.h */
  *offset &= 0xFFFFFFFFU;
  *start &= 0xFFFFFFFFU;
  *len &= 0xFFFFFFFFU;
#endif

  /* FIXME: Support a different offset from 0.  */
  if (offset[0] != 0)
    return KERN_INVALID_ARGUMENT;

  if (start[0] + len[0] < start[0])
    return KERN_INVALID_ARGUMENT;

  proxy = (memory_object_proxy_t) kmem_cache_alloc (&memory_object_proxy_cache);

  /* Allocate port, keeping a reference for it.  */
  proxy->port = ipc_port_alloc_kernel ();
  if (proxy->port == IP_NULL)
    {
      kmem_cache_free (&memory_object_proxy_cache, (vm_offset_t) proxy);
      return KERN_RESOURCE_SHORTAGE;
    }
  /* Associate the port with the proxy memory object.  */
  ipc_kobject_set (proxy->port, (ipc_kobject_t) proxy, IKOT_PAGER_PROXY);

  /* Request no-senders notifications on the port.  */
  proxy->notify = ipc_port_alloc_kernel ();
  ipc_kobject_set (proxy->notify, (ipc_kobject_t) proxy, IKOT_PAGER_PROXY);
  notify = ipc_port_make_sonce (proxy->notify);
  ip_lock (proxy->port);
  ipc_port_nsrequest (proxy->port, 1, notify, &notify);
  assert (notify == IP_NULL);

  /* Consumes the port right */
  proxy->object = object[0];
  proxy->max_protection = max_protection;
  proxy->start = start[0];
  proxy->len = len[0];

  *port = ipc_port_make_send (proxy->port);
  return KERN_SUCCESS;
}

/* Lookup the real memory object and maximum protection for the proxy
   memory object port PORT, for which the caller holds a reference.
   *OBJECT is only guaranteed to be valid as long as the caller holds
   the reference to PORT (unless the caller acquires its own reference
   to it).  If PORT is not a proxy memory object, return
   KERN_INVALID_ARGUMENT.  */
kern_return_t
memory_object_proxy_lookup (ipc_port_t port, ipc_port_t *object,
			    vm_prot_t *max_protection, vm_offset_t *start,
			    vm_offset_t *len)
{
  memory_object_proxy_t proxy;

  proxy = memory_object_proxy_port_lookup (port);
  if (!proxy)
    return KERN_INVALID_ARGUMENT;

  *max_protection = proxy->max_protection;
  *start = 0;
  *len = (vm_offset_t) ~0;

  do
    {
      *object = proxy->object;
      if (proxy->len <= *start)
	*len = 0;
      else
	*len = MIN(*len, proxy->len - *start);
      *start += proxy->start;
    }
  while ((proxy = memory_object_proxy_port_lookup (proxy->object)));

  return KERN_SUCCESS;
}
