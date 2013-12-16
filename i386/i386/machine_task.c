/* Machine specific data for a task on i386.

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

#include <kern/lock.h>
#include <mach/mach_types.h>
#include <kern/slab.h>
#include <machine/task.h>

#include <machine/io_perm.h>


/* The cache which holds our IO permission bitmaps.  */
struct kmem_cache machine_task_iopb_cache;


/* Initialize the machine task module.  The function is called once at
   start up by task_init in kern/task.c.  */
void
machine_task_module_init (void)
{
  kmem_cache_init (&machine_task_iopb_cache, "i386_task_iopb", IOPB_BYTES, 0,
		   NULL, NULL, NULL, 0);
}


/* Initialize the machine specific part of task TASK.  */
void
machine_task_init (task_t task)
{
  task->machine.iopb_size = 0;
  task->machine.iopb = 0;
  simple_lock_init (&task->machine.iopb_lock);
}


/* Destroy the machine specific part of task TASK and release all
   associated resources.  */
void
machine_task_terminate (const task_t task)
{
  if (task->machine.iopb)
    kmem_cache_free (&machine_task_iopb_cache,
		     (vm_offset_t) task->machine.iopb);
}


/* Try to release as much memory from the machine specific data in
   task TASK.  */
void
machine_task_collect (task_t task)
{
  simple_lock (&task->machine.iopb_lock);
  if (task->machine.iopb_size == 0 && task->machine.iopb)
    {
      kmem_cache_free (&machine_task_iopb_cache,
		       (vm_offset_t) task->machine.iopb);
      task->machine.iopb = 0;
    }
  simple_unlock (&task->machine.iopb_lock);
}
