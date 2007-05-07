/* Data types for I/O permission bitmap objects.

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

#ifndef _I386_IO_PERM_H_
#define _I386_IO_PERM_H_

#include <device/dev_hdr.h>
#include <ipc/ipc_types.h>


/* The highest possible I/O port.  */
#define	IOPB_MAX	0xffff

/* The number of bytes needed to hold all permission bits.  */
#define	IOPB_BYTES	(((IOPB_MAX + 1) + 7) / 8)

/* An offset that points outside of the permission bitmap, used to
   disable all permission.  */
#define IOPB_INVAL	0x2fff


/* The type of an I/O port address.  */
typedef unsigned short io_port_t;


struct io_perm
{
  /* We use a ``struct device'' for easy management.  */
  struct device device;

  ipc_port_t port;

  io_port_t from, to;
};

typedef struct io_perm *io_perm_t;

#define IO_PERM_NULL ((io_perm_t) 0)

extern io_perm_t convert_port_to_io_perm (ipc_port_t);
extern ipc_port_t convert_io_perm_to_port (io_perm_t);
#if TODO_REMOVE_ME
extern void io_perm_deallocate (io_perm_t);
#endif

#endif
