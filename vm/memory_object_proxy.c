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
#include <kern/zalloc.h>
#include <kern/mach_param.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

/* The zone which holds our proxy memory objects.  */
static zone_t memory_object_proxy_zone;

struct memory_object_proxy
{
  struct ipc_port *port;

  ipc_port_t object;
  vm_prot_t max_protection;
};
typedef struct memory_object_proxy *memory_object_proxy_t;


void
memory_object_proxy_init (void)
{
  /* For limit, see PORT_MAX.  */
  memory_object_proxy_zone = zinit (sizeof (struct memory_object_proxy), 0,
				    (TASK_MAX * 3 + THREAD_MAX)
				    * sizeof (struct memory_object_proxy),
				    256 * sizeof (struct memory_object_proxy),
				    ZONE_EXHAUSTIBLE,
				    "proxy memory object zone");
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
      proxy = memory_object_proxy_port_lookup
	((ipc_port_t) ns->not_header.msgh_remote_port);
      assert (proxy);

      ipc_port_release_send (proxy->object);
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
			    vm_offset_t *len, natural_t len_count,
			    ipc_port_t *port)
{
  kern_return_t kr;
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

  /* FIXME: Support a different offset from 0.  */
  if (offset[0] != 0)
    return KERN_INVALID_ARGUMENT;

  /* FIXME: Support a different range from total.  */
  if (start[0] != 0 || len[0] != (vm_offset_t) ~0)
    return KERN_INVALID_ARGUMENT;

  proxy = (memory_object_proxy_t) zalloc (memory_object_proxy_zone);

  /* Allocate port, keeping a reference for it.  */
  proxy->port = ipc_port_alloc_kernel ();
  if (proxy->port == IP_NULL)
    {
      zfree (memory_object_proxy_zone, (vm_offset_t) proxy);
      return KERN_RESOURCE_SHORTAGE;
    }
  /* Associate the port with the proxy memory object.  */
  ipc_kobject_set (proxy->port, (ipc_kobject_t) proxy, IKOT_PAGER_PROXY);

  /* Request no-senders notifications on the port.  */
  notify = ipc_port_make_sonce (proxy->port);
  ip_lock (proxy->port);
  ipc_port_nsrequest (proxy->port, 1, notify, &notify);
  assert (notify == IP_NULL);

  proxy->object = ipc_port_copy_send (object[0]);
  proxy->max_protection = max_protection;

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
			    vm_prot_t *max_protection)
{
  memory_object_proxy_t proxy;

  proxy = memory_object_proxy_port_lookup (port);
  if (!proxy)
    return KERN_INVALID_ARGUMENT;

   *object = proxy->object;
   *max_protection = proxy->max_protection;

  return KERN_SUCCESS;
}
