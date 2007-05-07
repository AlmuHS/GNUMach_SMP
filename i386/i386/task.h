/* Data types for machine specific parts of tasks on i386.

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

#ifndef	_I386_TASK_H_
#define _I386_TASK_H_

#include <kern/kern_types.h>
#include <kern/zalloc.h>

/* The machine specific data of a task.  */
struct machine_task
{
  /* A lock protecting iopb_size and iopb.  */
  decl_simple_lock_data (, iopb_lock);

  /* The highest I/O port number enabled.  */
  int iopb_size;

  /* The I/O permission bitmap.  */
  unsigned char *iopb;
};
typedef struct machine_task machine_task_t;


extern zone_t machine_task_iopb_zone;

/* Initialize the machine task module.  The function is called once at
   start up by task_init in kern/task.c.  */
void machine_task_module_init (void);

/* Initialize the machine specific part of task TASK.  */
void machine_task_init (task_t);

/* Destroy the machine specific part of task TASK and release all
   associated resources.  */
void machine_task_terminate (task_t);

/* Try to release as much memory from the machine specific data in
   task TASK. */
void machine_task_collect (task_t);

#endif	/* _I386_TASK_H_ */
