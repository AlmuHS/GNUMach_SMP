/* memory_object_proxy.h - Proxy memory objects for Mach.
   Copyright (C) 2005, 2011 Free Software Foundation, Inc.
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

#ifndef _VM_MEMORY_OBJECT_PROXY_H_
#define _VM_MEMORY_OBJECT_PROXY_H_

#include <ipc/ipc_types.h>
#include <mach/boolean.h>
#include <mach/machine/kern_return.h>
#include <mach/machine/vm_types.h>
#include <mach/message.h>
#include <mach/vm_prot.h>

extern void memory_object_proxy_init (void);
extern boolean_t memory_object_proxy_notify (mach_msg_header_t *msg);
extern kern_return_t memory_object_create_proxy (const ipc_space_t space,
                                                 vm_prot_t max_protection,
                                                 ipc_port_t *object,
                                                 natural_t object_count,
                                                 const vm_offset_t *offset,
                                                 natural_t offset_count,
                                                 const vm_offset_t *start,
                                                 natural_t start_count,
                                                 const vm_offset_t *len,
                                                 natural_t len_count,
                                                 ipc_port_t *port);
extern kern_return_t memory_object_proxy_lookup (ipc_port_t port,
                                                 ipc_port_t *object,
                                                 vm_prot_t *max_protection);

#endif /* _VM_MEMORY_OBJECT_PROXY_H_ */
