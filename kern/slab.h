/*
 * Copyright (c) 2011 Free Software Foundation.
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

/*
 * Copyright (c) 2010, 2011 Richard Braun.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Object caching memory allocator.
 */

#ifndef _KERN_SLAB_H
#define _KERN_SLAB_H

#include <kern/lock.h>
#include <kern/list.h>
#include <kern/rbtree.h>
#include <mach/machine/vm_types.h>
#include <sys/types.h>
#include <vm/vm_types.h>

#if SLAB_USE_CPU_POOLS
/*
 * L1 cache line size.
 */
#define CPU_L1_SIZE (1 << CPU_L1_SHIFT)

/*
 * Per-processor cache of pre-constructed objects.
 *
 * The flags member is a read-only CPU-local copy of the parent cache flags.
 */
struct kmem_cpu_pool {
    simple_lock_data_t lock;
    int flags;
    int size;
    int transfer_size;
    int nr_objs;
    void **array;
} __attribute__((aligned(CPU_L1_SIZE)));

/*
 * When a cache is created, its CPU pool type is determined from the buffer
 * size. For small buffer sizes, many objects can be cached in a CPU pool.
 * Conversely, for large buffer sizes, this would incur much overhead, so only
 * a few objects are stored in a CPU pool.
 */
struct kmem_cpu_pool_type {
    size_t buf_size;
    int array_size;
    size_t array_align;
    struct kmem_cache *array_cache;
};
#endif /* SLAB_USE_CPU_POOLS */

/*
 * Buffer descriptor.
 *
 * For normal caches (i.e. without SLAB_CF_VERIFY), bufctls are located at the
 * end of (but inside) each buffer. If SLAB_CF_VERIFY is set, bufctls are
 * located after each buffer.
 *
 * When an object is allocated to a client, its bufctl isn't used. This memory
 * is instead used for redzoning if cache debugging is in effect.
 */
union kmem_bufctl {
    union kmem_bufctl *next;
    unsigned long redzone;
};

/*
 * Buffer tag.
 *
 * This structure is only used for SLAB_CF_VERIFY caches. It is located after
 * the bufctl and includes information about the state of the buffer it
 * describes (allocated or not). It should be thought of as a debugging
 * extension of the bufctl.
 */
struct kmem_buftag {
    unsigned long state;
};

/*
 * Page-aligned collection of unconstructed buffers.
 */
struct kmem_slab {
    struct list list_node;
    struct rbtree_node tree_node;
    unsigned long nr_refs;
    union kmem_bufctl *first_free;
    void *addr;
};

/*
 * Type for constructor functions.
 *
 * The pre-constructed state of an object is supposed to include only
 * elements such as e.g. linked lists, locks, reference counters. Therefore
 * constructors are expected to 1) never fail and 2) not need any
 * user-provided data. The first constraint implies that object construction
 * never performs dynamic resource allocation, which also means there is no
 * need for destructors.
 */
typedef void (*kmem_cache_ctor_t)(void *obj);

/*
 * Types for slab allocation/free functions.
 *
 * All addresses and sizes must be page-aligned.
 */
typedef vm_offset_t (*kmem_slab_alloc_fn_t)(vm_size_t);
typedef void (*kmem_slab_free_fn_t)(vm_offset_t, vm_size_t);

/*
 * Cache name buffer size.
 */
#define KMEM_CACHE_NAME_SIZE 24

/*
 * Cache of objects.
 *
 * Locking order : cpu_pool -> cache. CPU pools locking is ordered by CPU ID.
 */
struct kmem_cache {
#if SLAB_USE_CPU_POOLS
    /* CPU pool layer */
    struct kmem_cpu_pool cpu_pools[NCPUS];
    struct kmem_cpu_pool_type *cpu_pool_type;
#endif /* SLAB_USE_CPU_POOLS */

    /* Slab layer */
    simple_lock_data_t lock;
    struct list node;   /* Cache list linkage */
    struct list partial_slabs;
    struct list free_slabs;
    struct rbtree active_slabs;
    int flags;
    size_t obj_size;    /* User-provided size */
    size_t align;
    size_t buf_size;    /* Aligned object size  */
    size_t bufctl_dist; /* Distance from buffer to bufctl   */
    size_t slab_size;
    size_t color;
    size_t color_max;
    unsigned long bufs_per_slab;
    unsigned long nr_objs;  /* Number of allocated objects */
    unsigned long nr_bufs;  /* Total number of buffers */
    unsigned long nr_slabs;
    unsigned long nr_free_slabs;
    kmem_cache_ctor_t ctor;
    kmem_slab_alloc_fn_t slab_alloc_fn;
    kmem_slab_free_fn_t slab_free_fn;
    char name[KMEM_CACHE_NAME_SIZE];
    size_t buftag_dist; /* Distance from buffer to buftag */
    size_t redzone_pad; /* Bytes from end of object to redzone word */
};

/*
 * Mach-style declarations for struct kmem_cache.
 */
typedef struct kmem_cache *kmem_cache_t;
#define KMEM_CACHE_NULL ((kmem_cache_t) 0)

/*
 * VM submap for slab allocations.
 */
extern vm_map_t kmem_map;

/*
 * Cache initialization flags.
 */
#define KMEM_CACHE_NOCPUPOOL    0x1 /* Don't use the per-cpu pools */
#define KMEM_CACHE_NOOFFSLAB    0x2 /* Don't allocate external slab data */
#define KMEM_CACHE_NORECLAIM    0x4 /* Never give slabs back to their source,
                                       implies KMEM_CACHE_NOOFFSLAB */
#define KMEM_CACHE_VERIFY       0x8 /* Use debugging facilities */

/*
 * Initialize a cache.
 */
void kmem_cache_init(struct kmem_cache *cache, const char *name,
                     size_t obj_size, size_t align, kmem_cache_ctor_t ctor,
                     kmem_slab_alloc_fn_t slab_alloc_fn,
                     kmem_slab_free_fn_t slab_free_fn, int flags);

/*
 * Allocate an object from a cache.
 */
vm_offset_t kmem_cache_alloc(struct kmem_cache *cache);

/*
 * Release an object to its cache.
 */
void kmem_cache_free(struct kmem_cache *cache, vm_offset_t obj);

/*
 * Initialize the memory allocator module.
 */
void slab_bootstrap(void);
void slab_init(void);

/*
 * Release free slabs to the VM system.
 */
void slab_collect(void);

/*
 * Display a summary of all kernel caches.
 */
void slab_info(void);

#endif /* _KERN_SLAB_H */
