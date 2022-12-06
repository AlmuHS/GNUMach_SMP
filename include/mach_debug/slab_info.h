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

typedef struct cache_info {
	int flags;
	rpc_vm_size_t cpu_pool_size;
	rpc_vm_size_t obj_size;
	rpc_vm_size_t align;
	rpc_vm_size_t buf_size;
	rpc_vm_size_t slab_size;
	rpc_long_natural_t bufs_per_slab;
	rpc_long_natural_t nr_objs;
	rpc_long_natural_t nr_bufs;
	rpc_long_natural_t nr_slabs;
	rpc_long_natural_t nr_free_slabs;
	char name[CACHE_NAME_MAX_LEN];
} cache_info_t;

typedef cache_info_t *cache_info_array_t;

#endif	/* _MACH_DEBUG_SLAB_INFO_H_ */
