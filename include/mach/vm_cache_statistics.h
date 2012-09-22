/*
 * Copyright (C) 2012 Free Software Foundation
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _MACH_VM_CACHE_STATISTICS_H_
#define _MACH_VM_CACHE_STATISTICS_H_

#include <mach/machine/vm_types.h>

struct vm_cache_statistics {
	integer_t	cache_object_count;	/* # of cached objects */
	integer_t	cache_count;		/* # of cached pages */
	integer_t	active_tmp_count;	/* # of active temporary pages */
	integer_t	inactive_tmp_count;	/* # of inactive temporary pages */
	integer_t	active_perm_count;	/* # of active permanent pages */
	integer_t	inactive_perm_count;	/* # of inactive permanent pages */
	integer_t	dirty_count;		/* # of dirty pages */
	integer_t	laundry_count;		/* # of pages being laundered */
	integer_t	writeback_count;	/* # of pages being written back */
	integer_t	slab_count;		/* # of slab allocator pages */
	integer_t	slab_reclaim_count;	/* # of reclaimable slab pages */
};

typedef struct vm_cache_statistics	*vm_cache_statistics_t;
typedef struct vm_cache_statistics	vm_cache_statistics_data_t;

#endif /* _MACH_VM_CACHE_STATISTICS_H_ */
