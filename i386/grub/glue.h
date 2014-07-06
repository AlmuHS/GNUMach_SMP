/*
 * Copyright (c) 2014 Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GRUB_GLUE_H
#define _GRUB_GLUE_H

#define GRUB_FILE __FILE__
#define grub_memcmp memcmp
#define grub_printf printf
#define grub_puts_ puts

#include <mach/mach_types.h>
#include <i386/vm_param.h>

/* Warning: this leaks memory maps for now, do not use it yet for something
 * else than Mach shutdown. */
vm_offset_t io_map_cached(vm_offset_t phys_addr, vm_size_t size);

#endif /* _GRUB_GLUE_H */
