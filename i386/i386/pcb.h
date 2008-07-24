/*
 *
 * Copyright (C) 2006 Free Software Foundation, Inc.
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
 *
 * Author: Barry deFreese.
 */
/*
 *
 *
 */

#ifndef _I386_PCB_H_
#define _I386_PCB_H_

#include <sys/types.h>
#include <mach/exec/exec.h>

extern void pcb_init (thread_t thread);

extern void pcb_terminate (thread_t thread);

extern void pcb_collect (thread_t thread);

extern kern_return_t thread_setstatus (
   thread_t        thread,
   int             flavor,
   thread_state_t  tstate,
   unsigned int    count);

extern kern_return_t thread_getstatus (
   thread_t        thread,
   int             flavor,
   thread_state_t  tstate,
   unsigned int    *count);

extern void thread_set_syscall_return (
   thread_t        thread,
   kern_return_t   retval);

extern vm_offset_t user_stack_low (vm_size_t stack_size);

extern vm_offset_t set_user_regs (
   vm_offset_t stack_base,
   vm_offset_t stack_size,
   struct exec_info *exec_info,
   vm_size_t   arg_size);

extern void load_context (thread_t new);

extern void stack_attach (
   thread_t thread, 
   vm_offset_t stack, 
   void (*continuation)());

extern vm_offset_t stack_detach (thread_t thread);

extern void switch_ktss (pcb_t pcb);

#endif /* _I386_PCB_H_ */
