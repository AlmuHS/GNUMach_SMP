/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef _MACH_DEBUG_SLAB_INFO_H_
#define _MACH_DEBUG_SLAB_INFO_H_

#include <sys/types.h>

/*
 *	Remember to update the mig type definitions
 *	in mach_debug_types.defs when adding/removing fields.
 */

#define CACHE_NAME_MAX_LEN 32

#define CACHE_FLAGS_NO_CPU_POOL		0x01
#define CACHE_FLAGS_SLAB_EXTERNAL	0x02
#define CACHE_FLAGS_NO_RECLAIM		0x04
#define CACHE_FLAGS_VERIFY		0x08
#define CACHE_FLAGS_DIRECT		0x10

typedef struct cache_info {
	int flags;
	size_t cpu_pool_size;
	size_t obj_size;
	size_t align;
	size_t buf_size;
	size_t slab_size;
	unsigned long bufs_per_slab;
	unsigned long nr_objs;
	unsigned long nr_bufs;
	unsigned long nr_slabs;
	unsigned long nr_free_slabs;
	char name[CACHE_NAME_MAX_LEN];
} cache_info_t;

typedef cache_info_t *cache_info_array_t;

#endif	/* _MACH_DEBUG_SLAB_INFO_H_ */
