/*
 *  Copyright (C) 2011 Free Software Foundation
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef LINUX_DEV_GLUE_GLUE_H
#define LINUX_DEV_GLUE_GLUE_H

#include <vm/vm_types.h>
#include <mach/machine/vm_types.h>

extern int linux_auto_config;

extern unsigned long alloc_contig_mem (unsigned, unsigned, unsigned, vm_page_t *);
extern void free_contig_mem (vm_page_t, unsigned);
extern void init_IRQ (void);
extern void restore_IRQ (void);
extern void linux_kmem_init (void);
extern void linux_net_emulation_init (void);
extern void device_setup (void);
extern void linux_timer_intr (void);
extern void linux_sched_init (void);
extern void pcmcia_init (void);
extern void linux_soft_intr (void);
extern int issig (void);
extern int linux_to_mach_error (int);
extern char *get_options(char *str, int *ints);

#endif /* LINUX_DEV_GLUE_GLUE_H */
