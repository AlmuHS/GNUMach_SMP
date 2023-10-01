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
 * Object caching and general purpose memory allocator.
 *
 * This allocator is based on the paper "The Slab Allocator: An Object-Caching
 * Kernel Memory Allocator" by Jeff Bonwick.
 *
 * It allows the allocation of objects (i.e. fixed-size typed buffers) from
 * caches and is efficient in both space and time. This implementation follows
 * many of the indications from the paper mentioned. The most notable
 * differences are outlined below.
 *
 * The per-cache self-scaling hash table for buffer-to-bufctl conversion,
 * described in 3.2.3 "Slab Layout for Large Objects", has been replaced by
 * a red-black tree storing slabs, sorted by address. The use of a
 * self-balancing tree for buffer-to-slab conversions provides a few advantages
 * over a hash table. Unlike a hash table, a BST provides a "lookup nearest"
 * operation, so obtaining the slab data (whether it is embedded in the slab or
 * off slab) from a buffer address simply consists of a "lookup nearest towards
 * 0" tree search. Finally, a self-balancing tree is a true self-scaling data
 * structure, whereas a hash table requires periodic maintenance and complete
 * resizing, which is expensive. The only drawback is that releasing a buffer
 * to the slab layer takes logarithmic time instead of constant time.
 *
 * This implementation uses per-cpu pools of objects, which service most
 * allocation requests. These pools act as caches (but are named differently
 * to avoid confusion with CPU caches) that reduce contention on multiprocessor
 * systems. When a pool is empty and cannot provide an object, it is filled by
 * transferring multiple objects from the slab layer. The symmetric case is
 * handled likewise.
 */

#include <string.h>
#include <kern/assert.h>
#include <kern/mach_clock.h>
#include <kern/macros.h>
#include <kern/printf.h>
#include <kern/slab.h>
#include <kern/kalloc.h>
#include <kern/cpu_number.h>
#include <kern/mach_debug.server.h>
#include <mach/vm_param.h>
#include <mach/machine/vm_types.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_types.h>
#include <sys/types.h>

#ifdef MACH_DEBUG
#include <mach_debug/slab_info.h>
#endif

/*
 * Utility macros.
 */
#define P2ALIGNED(x, a) (((x) & ((a) - 1)) == 0)
#define ISP2(x)         P2ALIGNED(x, x)
#define P2ALIGN(x, a)   ((x) & -(a))
#define P2ROUND(x, a)   (-(-(x) & -(a)))
#define P2END(x, a)     (-(~(x) & -(a)))
#define likely(expr)    __builtin_expect(!!(expr), 1)
#define unlikely(expr)  __builtin_expect(!!(expr), 0)

/*
 * Minimum required alignment.
 */
#define KMEM_ALIGN_MIN 8

/*
 * Special buffer size under which slab data is unconditionnally allocated
 * from its associated slab.
 */
#define KMEM_BUF_SIZE_THRESHOLD (PAGE_SIZE / 8)

/*
 * Time (in ticks) between two garbage collection operations.
 */
#define KMEM_GC_INTERVAL (5 * hz)

/*
 * The transfer size of a CPU pool is computed by dividing the pool size by
 * this value.
 */
#define KMEM_CPU_POOL_TRANSFER_RATIO 2

/*
 * Redzone guard word.
 */
#ifdef __LP64__
#if _HOST_BIG_ENDIAN
#define KMEM_REDZONE_WORD 0xfeedfacefeedfaceUL
#else /* _HOST_BIG_ENDIAN */
#define KMEM_REDZONE_WORD 0xcefaedfecefaedfeUL
#endif /* _HOST_BIG_ENDIAN */
#else /* __LP64__ */
#if _HOST_BIG_ENDIAN
#define KMEM_REDZONE_WORD 0xfeedfaceUL
#else /* _HOST_BIG_ENDIAN */
#define KMEM_REDZONE_WORD 0xcefaedfeUL
#endif /* _HOST_BIG_ENDIAN */
#endif /* __LP64__ */

/*
 * Redzone byte for padding.
 */
#define KMEM_REDZONE_BYTE 0xbb

/*
 * Shift for the first kalloc cache size.
 */
#define KALLOC_FIRST_SHIFT 5

/*
 * Number of caches backing general purpose allocations.
 */
#define KALLOC_NR_CACHES 13

/*
 * Values the buftag state member can take.
 */
#ifdef __LP64__
#if _HOST_BIG_ENDIAN
#define KMEM_BUFTAG_ALLOC   0xa110c8eda110c8edUL
#define KMEM_BUFTAG_FREE    0xf4eeb10cf4eeb10cUL
#else /* _HOST_BIG_ENDIAN */
#define KMEM_BUFTAG_ALLOC   0xedc810a1edc810a1UL
#define KMEM_BUFTAG_FREE    0x0cb1eef40cb1eef4UL
#endif /* _HOST_BIG_ENDIAN */
#else /* __LP64__ */
#if _HOST_BIG_ENDIAN
#define KMEM_BUFTAG_ALLOC   0xa110c8edUL
#define KMEM_BUFTAG_FREE    0xf4eeb10cUL
#else /* _HOST_BIG_ENDIAN */
#define KMEM_BUFTAG_ALLOC   0xedc810a1UL
#define KMEM_BUFTAG_FREE    0x0cb1eef4UL
#endif /* _HOST_BIG_ENDIAN */
#endif /* __LP64__ */

/*
 * Free and uninitialized patterns.
 *
 * These values are unconditionnally 64-bit wide since buffers are at least
 * 8-byte aligned.
 */
#if _HOST_BIG_ENDIAN
#define KMEM_FREE_PATTERN   0xdeadbeefdeadbeefULL
#define KMEM_UNINIT_PATTERN 0xbaddcafebaddcafeULL
#else /* _HOST_BIG_ENDIAN */
#define KMEM_FREE_PATTERN   0xefbeaddeefbeaddeULL
#define KMEM_UNINIT_PATTERN 0xfecaddbafecaddbaULL
#endif /* _HOST_BIG_ENDIAN */

/*
 * Cache flags.
 *
 * The flags don't change once set and can be tested without locking.
 */
#define KMEM_CF_SLAB_EXTERNAL   0x01    /* Slab data is off slab */
#define KMEM_CF_PHYSMEM         0x02    /* Allocate from physical memory */
#define KMEM_CF_DIRECT          0x04    /* Direct buf-to-slab translation
                                           (implies !KMEM_CF_SLAB_EXTERNAL) */
#define KMEM_CF_USE_TREE        0x08    /* Use red-black tree to track slab
                                           data */
#define KMEM_CF_USE_PAGE        0x10    /* Use page private data to track slab
                                           data (implies KMEM_CF_SLAB_EXTERNAL
                                           and KMEM_CF_PHYSMEM) */
#define KMEM_CF_VERIFY          0x20    /* Debugging facilities enabled
                                           (implies KMEM_CF_USE_TREE) */

/*
 * Options for kmem_cache_alloc_verify().
 */
#define KMEM_AV_NOCONSTRUCT 0
#define KMEM_AV_CONSTRUCT   1

/*
 * Error codes for kmem_cache_error().
 */
#define KMEM_ERR_INVALID    0   /* Invalid address being freed */
#define KMEM_ERR_DOUBLEFREE 1   /* Freeing already free address */
#define KMEM_ERR_BUFTAG     2   /* Invalid buftag content */
#define KMEM_ERR_MODIFIED   3   /* Buffer modified while free */
#define KMEM_ERR_REDZONE    4   /* Redzone violation */

#if SLAB_USE_CPU_POOLS
/*
 * Available CPU pool types.
 *
 * For each entry, the CPU pool size applies from the entry buf_size
 * (excluded) up to (and including) the buf_size of the preceding entry.
 *
 * See struct kmem_cpu_pool_type for a description of the values.
 */
static struct kmem_cpu_pool_type kmem_cpu_pool_types[] = {
    {  32768,   1, 0,           NULL },
    {   4096,   8, CPU_L1_SIZE, NULL },
    {    256,  64, CPU_L1_SIZE, NULL },
    {      0, 128, CPU_L1_SIZE, NULL }
};

/*
 * Caches where CPU pool arrays are allocated from.
 */
static struct kmem_cache kmem_cpu_array_caches[ARRAY_SIZE(kmem_cpu_pool_types)];
#endif /* SLAB_USE_CPU_POOLS */

/*
 * Cache for off slab data.
 */
static struct kmem_cache kmem_slab_cache;

/*
 * General purpose caches array.
 */
static struct kmem_cache kalloc_caches[KALLOC_NR_CACHES];

/*
 * List of all caches managed by the allocator.
 */
static struct list kmem_cache_list;
static unsigned int kmem_nr_caches;
static simple_lock_data_t __attribute__((used)) kmem_cache_list_lock;

/*
 * Time of the last memory reclaim, in clock ticks.
 */
static unsigned long kmem_gc_last_tick;

#define kmem_error(format, ...)                         \
    panic("mem: error: %s(): " format "\n", __func__,   \
          ## __VA_ARGS__)

#define kmem_warn(format, ...)                              \
    printf("mem: warning: %s(): " format "\n", __func__,    \
           ## __VA_ARGS__)

#define kmem_print(format, ...) \
    printf(format "\n", ## __VA_ARGS__)

static void kmem_cache_error(struct kmem_cache *cache, void *buf, int error,
                             void *arg);
static void * kmem_cache_alloc_from_slab(struct kmem_cache *cache);
static void kmem_cache_free_to_slab(struct kmem_cache *cache, void *buf);

static void * kmem_buf_verify_bytes(void *buf, void *pattern, size_t size)
{
    char *ptr, *pattern_ptr, *end;

    end = buf + size;

    for (ptr = buf, pattern_ptr = pattern; ptr < end; ptr++, pattern_ptr++)
        if (*ptr != *pattern_ptr)
            return ptr;

    return NULL;
}

static void * kmem_buf_verify(void *buf, uint64_t pattern, vm_size_t size)
{
    uint64_t *ptr, *end;

    assert(P2ALIGNED((unsigned long)buf, sizeof(uint64_t)));
    assert(P2ALIGNED(size, sizeof(uint64_t)));

    end = buf + size;

    for (ptr = buf; ptr < end; ptr++)
        if (*ptr != pattern)
            return kmem_buf_verify_bytes(ptr, &pattern, sizeof(pattern));

    return NULL;
}

static void kmem_buf_fill(void *buf, uint64_t pattern, size_t size)
{
    uint64_t *ptr, *end;

    assert(P2ALIGNED((unsigned long)buf, sizeof(uint64_t)));
    assert(P2ALIGNED(size, sizeof(uint64_t)));

    end = buf + size;

    for (ptr = buf; ptr < end; ptr++)
        *ptr = pattern;
}

static void * kmem_buf_verify_fill(void *buf, uint64_t old, uint64_t new,
                                   size_t size)
{
    uint64_t *ptr, *end;

    assert(P2ALIGNED((unsigned long)buf, sizeof(uint64_t)));
    assert(P2ALIGNED(size, sizeof(uint64_t)));

    end = buf + size;

    for (ptr = buf; ptr < end; ptr++) {
        if (*ptr != old)
            return kmem_buf_verify_bytes(ptr, &old, sizeof(old));

        *ptr = new;
    }

    return NULL;
}

static inline union kmem_bufctl *
kmem_buf_to_bufctl(void *buf, struct kmem_cache *cache)
{
    return (union kmem_bufctl *)(buf + cache->bufctl_dist);
}

static inline struct kmem_buftag *
kmem_buf_to_buftag(void *buf, struct kmem_cache *cache)
{
    return (struct kmem_buftag *)(buf + cache->buftag_dist);
}

static inline void * kmem_bufctl_to_buf(union kmem_bufctl *bufctl,
                                        struct kmem_cache *cache)
{
    return (void *)bufctl - cache->bufctl_dist;
}

static vm_offset_t
kmem_pagealloc_physmem(vm_size_t size)
{
    struct vm_page *page;

    assert(size == PAGE_SIZE);

    for (;;) {
        page = vm_page_grab(VM_PAGE_DIRECTMAP);

        if (page != NULL)
            break;

        VM_PAGE_WAIT(NULL);
    }

    return phystokv(vm_page_to_pa(page));
}

static void
kmem_pagefree_physmem(vm_offset_t addr, vm_size_t size)
{
    struct vm_page *page;

    assert(size == PAGE_SIZE);
    page = vm_page_lookup_pa(kvtophys(addr));
    assert(page != NULL);
    vm_page_release(page, FALSE, FALSE);
}

static vm_offset_t
kmem_pagealloc_virtual(vm_size_t size, vm_size_t align)
{
    vm_offset_t addr;
    kern_return_t kr;

    assert(size > PAGE_SIZE);
    size = vm_page_round(size);

    if (align <= PAGE_SIZE)
        kr = kmem_alloc_wired(kernel_map, &addr, size);
    else
        kr = kmem_alloc_aligned(kernel_map, &addr, size);

    if (kr != KERN_SUCCESS)
        return 0;

    return addr;
}

static void
kmem_pagefree_virtual(vm_offset_t addr, vm_size_t size)
{
    if (addr < kernel_virtual_start || addr + size > kernel_virtual_end)
	panic("kmem_pagefree_virtual(%lx-%lx) falls in physical memory area!\n",
		(unsigned long) addr, (unsigned long) addr + size);
    assert(size > PAGE_SIZE);
    size = vm_page_round(size);
    kmem_free(kernel_map, addr, size);
}

static vm_offset_t
kmem_pagealloc(vm_size_t size, vm_size_t align, int flags)
{
    assert(align <= size);
    return (flags & KMEM_CF_PHYSMEM)
           ? kmem_pagealloc_physmem(size)
           : kmem_pagealloc_virtual(size, align);
}

static void
kmem_pagefree(vm_offset_t addr, vm_size_t size, int flags)
{
    return (flags & KMEM_CF_PHYSMEM)
           ? kmem_pagefree_physmem(addr, size)
           : kmem_pagefree_virtual(addr, size);
}

static void kmem_slab_create_verify(struct kmem_slab *slab,
                                    struct kmem_cache *cache)
{
    struct kmem_buftag *buftag;
    size_t buf_size;
    unsigned long buffers;
    void *buf;

    buf_size = cache->buf_size;
    buf = slab->addr;
    buftag = kmem_buf_to_buftag(buf, cache);

    for (buffers = cache->bufs_per_slab; buffers != 0; buffers--) {
        kmem_buf_fill(buf, KMEM_FREE_PATTERN, cache->bufctl_dist);
        buftag->state = KMEM_BUFTAG_FREE;
        buf += buf_size;
        buftag = kmem_buf_to_buftag(buf, cache);
    }
}

/*
 * Create an empty slab for a cache.
 *
 * The caller must drop all locks before calling this function.
 */
static struct kmem_slab * kmem_slab_create(struct kmem_cache *cache,
                                           size_t color)
{
    struct kmem_slab *slab;
    union kmem_bufctl *bufctl;
    size_t buf_size;
    unsigned long buffers;
    vm_offset_t slab_buf;

    slab_buf = kmem_pagealloc(cache->slab_size, cache->align, cache->flags);

    if (slab_buf == 0)
        return NULL;

    if (cache->flags & KMEM_CF_SLAB_EXTERNAL) {
        slab = (struct kmem_slab *)kmem_cache_alloc(&kmem_slab_cache);

        if (slab == NULL) {
            kmem_pagefree(slab_buf, cache->slab_size, cache->flags);
            return NULL;
        }

        if (cache->flags & KMEM_CF_USE_PAGE) {
            struct vm_page *page;

            page = vm_page_lookup_pa(kvtophys(slab_buf));
            assert(page != NULL);
            vm_page_set_priv(page, slab);
        }
    } else {
        slab = (struct kmem_slab *)(slab_buf + cache->slab_size) - 1;
    }

    slab->cache = cache;
    list_node_init(&slab->list_node);
    rbtree_node_init(&slab->tree_node);
    slab->nr_refs = 0;
    slab->first_free = NULL;
    slab->addr = (void *)(slab_buf + color);

    buf_size = cache->buf_size;
    bufctl = kmem_buf_to_bufctl(slab->addr, cache);

    for (buffers = cache->bufs_per_slab; buffers != 0; buffers--) {
        bufctl->next = slab->first_free;
        slab->first_free = bufctl;
        bufctl = (union kmem_bufctl *)((void *)bufctl + buf_size);
    }

    if (cache->flags & KMEM_CF_VERIFY)
        kmem_slab_create_verify(slab, cache);

    return slab;
}

static void kmem_slab_destroy_verify(struct kmem_slab *slab,
                                     struct kmem_cache *cache)
{
    struct kmem_buftag *buftag;
    size_t buf_size;
    unsigned long buffers;
    void *buf, *addr;

    buf_size = cache->buf_size;
    buf = slab->addr;
    buftag = kmem_buf_to_buftag(buf, cache);

    for (buffers = cache->bufs_per_slab; buffers != 0; buffers--) {
        if (buftag->state != KMEM_BUFTAG_FREE)
            kmem_cache_error(cache, buf, KMEM_ERR_BUFTAG, buftag);

        addr = kmem_buf_verify(buf, KMEM_FREE_PATTERN, cache->bufctl_dist);

        if (addr != NULL)
            kmem_cache_error(cache, buf, KMEM_ERR_MODIFIED, addr);

        buf += buf_size;
        buftag = kmem_buf_to_buftag(buf, cache);
    }
}

/*
 * Destroy a slab.
 *
 * The caller must drop all locks before calling this function.
 */
static void kmem_slab_destroy(struct kmem_slab *slab, struct kmem_cache *cache)
{
    vm_offset_t slab_buf;

    assert(slab->nr_refs == 0);
    assert(slab->first_free != NULL);

    if (cache->flags & KMEM_CF_VERIFY)
        kmem_slab_destroy_verify(slab, cache);

    slab_buf = (vm_offset_t)P2ALIGN((unsigned long)slab->addr, PAGE_SIZE);

    if (cache->flags & KMEM_CF_SLAB_EXTERNAL) {
        if (cache->flags & KMEM_CF_USE_PAGE) {
            struct vm_page *page;

            /* Not strictly needed, but let's increase safety */
            page = vm_page_lookup_pa(kvtophys(slab_buf));
            assert(page != NULL);
            vm_page_set_priv(page, NULL);
        }

        kmem_cache_free(&kmem_slab_cache, (vm_offset_t)slab);
    }

    kmem_pagefree(slab_buf, cache->slab_size, cache->flags);
}

static inline int kmem_slab_cmp_lookup(const void *addr,
                                       const struct rbtree_node *node)
{
    struct kmem_slab *slab;

    slab = rbtree_entry(node, struct kmem_slab, tree_node);

    if (addr == slab->addr)
        return 0;
    else if (addr < slab->addr)
        return -1;
    else
        return 1;
}

static inline int kmem_slab_cmp_insert(const struct rbtree_node *a,
                                       const struct rbtree_node *b)
{
    struct kmem_slab *slab;

    slab = rbtree_entry(a, struct kmem_slab, tree_node);
    return kmem_slab_cmp_lookup(slab->addr, b);
}

#if SLAB_USE_CPU_POOLS
static void kmem_cpu_pool_init(struct kmem_cpu_pool *cpu_pool,
                               struct kmem_cache *cache)
{
    simple_lock_init(&cpu_pool->lock);
    cpu_pool->flags = cache->flags;
    cpu_pool->size = 0;
    cpu_pool->transfer_size = 0;
    cpu_pool->nr_objs = 0;
    cpu_pool->array = NULL;
}

/*
 * Return a CPU pool.
 *
 * This function will generally return the pool matching the CPU running the
 * calling thread. Because of context switches and thread migration, the
 * caller might be running on another processor after this function returns.
 * Although not optimal, this should rarely happen, and it doesn't affect the
 * allocator operations in any other way, as CPU pools are always valid, and
 * their access is serialized by a lock.
 */
static inline struct kmem_cpu_pool * kmem_cpu_pool_get(struct kmem_cache *cache)
{
    return &cache->cpu_pools[cpu_number()];
}

static inline void kmem_cpu_pool_build(struct kmem_cpu_pool *cpu_pool,
                                       struct kmem_cache *cache, void **array)
{
    cpu_pool->size = cache->cpu_pool_type->array_size;
    cpu_pool->transfer_size = (cpu_pool->size
                               + KMEM_CPU_POOL_TRANSFER_RATIO - 1)
                              / KMEM_CPU_POOL_TRANSFER_RATIO;
    cpu_pool->array = array;
}

static inline void * kmem_cpu_pool_pop(struct kmem_cpu_pool *cpu_pool)
{
    cpu_pool->nr_objs--;
    return cpu_pool->array[cpu_pool->nr_objs];
}

static inline void kmem_cpu_pool_push(struct kmem_cpu_pool *cpu_pool, void *obj)
{
    cpu_pool->array[cpu_pool->nr_objs] = obj;
    cpu_pool->nr_objs++;
}

static int kmem_cpu_pool_fill(struct kmem_cpu_pool *cpu_pool,
                              struct kmem_cache *cache)
{
    kmem_cache_ctor_t ctor;
    void *buf;
    int i;

    ctor = (cpu_pool->flags & KMEM_CF_VERIFY) ? NULL : cache->ctor;

    simple_lock(&cache->lock);

    for (i = 0; i < cpu_pool->transfer_size; i++) {
        buf = kmem_cache_alloc_from_slab(cache);

        if (buf == NULL)
            break;

        if (ctor != NULL)
            ctor(buf);

        kmem_cpu_pool_push(cpu_pool, buf);
    }

    simple_unlock(&cache->lock);

    return i;
}

static void kmem_cpu_pool_drain(struct kmem_cpu_pool *cpu_pool,
                                struct kmem_cache *cache)
{
    void *obj;
    int i;

    simple_lock(&cache->lock);

    for (i = cpu_pool->transfer_size; i > 0; i--) {
        obj = kmem_cpu_pool_pop(cpu_pool);
        kmem_cache_free_to_slab(cache, obj);
    }

    simple_unlock(&cache->lock);
}
#endif /* SLAB_USE_CPU_POOLS */

static void kmem_cache_error(struct kmem_cache *cache, void *buf, int error,
                             void *arg)
{
    struct kmem_buftag *buftag;

    kmem_warn("cache: %s, buffer: %p", cache->name, (void *)buf);

    switch(error) {
    case KMEM_ERR_INVALID:
        kmem_error("freeing invalid address");
        break;
    case KMEM_ERR_DOUBLEFREE:
        kmem_error("attempting to free the same address twice");
        break;
    case KMEM_ERR_BUFTAG:
        buftag = arg;
        kmem_error("invalid buftag content, buftag state: %p",
                   (void *)buftag->state);
        break;
    case KMEM_ERR_MODIFIED:
        kmem_error("free buffer modified, fault address: %p, "
                   "offset in buffer: %td", arg, arg - buf);
        break;
    case KMEM_ERR_REDZONE:
        kmem_error("write beyond end of buffer, fault address: %p, "
                   "offset in buffer: %td", arg, arg - buf);
        break;
    default:
        kmem_error("unknown error");
    }

    /*
     * Never reached.
     */
}

/*
 * Compute properties such as slab size for the given cache.
 *
 * Once the slab size is known, this function sets the related properties
 * (buffers per slab and maximum color). It can also set some KMEM_CF_xxx
 * flags depending on the resulting layout.
 */
static void kmem_cache_compute_properties(struct kmem_cache *cache, int flags)
{
    size_t size, waste;
    int embed;

    if (cache->buf_size < KMEM_BUF_SIZE_THRESHOLD)
        flags |= KMEM_CACHE_NOOFFSLAB;

    cache->slab_size = PAGE_SIZE;

    for (;;) {
        if (flags & KMEM_CACHE_NOOFFSLAB)
            embed = 1;
        else {
            waste = cache->slab_size % cache->buf_size;
            embed = (sizeof(struct kmem_slab) <= waste);
        }

        size = cache->slab_size;

        if (embed)
            size -= sizeof(struct kmem_slab);

        if (size >= cache->buf_size)
            break;

        cache->slab_size += PAGE_SIZE;
    }

    cache->bufs_per_slab = size / cache->buf_size;
    cache->color_max = size % cache->buf_size;

    if (cache->color_max >= PAGE_SIZE)
        cache->color_max = 0;

    if (!embed)
        cache->flags |= KMEM_CF_SLAB_EXTERNAL;

    if ((flags & KMEM_CACHE_PHYSMEM) || (cache->slab_size == PAGE_SIZE)) {
        cache->flags |= KMEM_CF_PHYSMEM;

        /*
         * Avoid using larger-than-page slabs backed by the direct physical
         * mapping to completely prevent physical memory fragmentation from
         * making slab allocations fail.
         */
        if (cache->slab_size != PAGE_SIZE)
            panic("slab: invalid cache parameters");
    }

    if (cache->flags & KMEM_CF_VERIFY)
        cache->flags |= KMEM_CF_USE_TREE;

    if (cache->flags & KMEM_CF_SLAB_EXTERNAL) {
        if (cache->flags & KMEM_CF_PHYSMEM)
            cache->flags |= KMEM_CF_USE_PAGE;
        else
            cache->flags |= KMEM_CF_USE_TREE;
    } else {
        if (cache->slab_size == PAGE_SIZE)
            cache->flags |= KMEM_CF_DIRECT;
        else
            cache->flags |= KMEM_CF_USE_TREE;
    }
}

void kmem_cache_init(struct kmem_cache *cache, const char *name,
                     size_t obj_size, size_t align,
                     kmem_cache_ctor_t ctor, int flags)
{
#if SLAB_USE_CPU_POOLS
    struct kmem_cpu_pool_type *cpu_pool_type;
    size_t i;
#endif /* SLAB_USE_CPU_POOLS */
    size_t buf_size;

    cache->flags = 0;
#if SLAB_VERIFY
    if (obj_size < PAGE_SIZE - sizeof(union kmem_bufctl) + sizeof(struct kmem_buftag))
        cache->flags |= KMEM_CF_VERIFY;
#endif /* SLAB_VERIFY */

    if (flags & KMEM_CACHE_VERIFY)
        cache->flags |= KMEM_CF_VERIFY;

    if (align < KMEM_ALIGN_MIN)
        align = KMEM_ALIGN_MIN;

    assert(obj_size > 0);
    assert(ISP2(align));

    buf_size = P2ROUND(obj_size, align);

    simple_lock_init(&cache->lock);
    list_node_init(&cache->node);
    list_init(&cache->partial_slabs);
    list_init(&cache->free_slabs);
    rbtree_init(&cache->active_slabs);
    cache->obj_size = obj_size;
    cache->align = align;
    cache->buf_size = buf_size;
    cache->bufctl_dist = buf_size - sizeof(union kmem_bufctl);
    cache->color = 0;
    cache->nr_objs = 0;
    cache->nr_bufs = 0;
    cache->nr_slabs = 0;
    cache->nr_free_slabs = 0;
    cache->ctor = ctor;
    strncpy(cache->name, name, sizeof(cache->name));
    cache->name[sizeof(cache->name) - 1] = '\0';
    cache->buftag_dist = 0;
    cache->redzone_pad = 0;

    if (cache->flags & KMEM_CF_VERIFY) {
        cache->bufctl_dist = buf_size;
        cache->buftag_dist = cache->bufctl_dist + sizeof(union kmem_bufctl);
        cache->redzone_pad = cache->bufctl_dist - cache->obj_size;
        buf_size += sizeof(union kmem_bufctl) + sizeof(struct kmem_buftag);
        buf_size = P2ROUND(buf_size, align);
        cache->buf_size = buf_size;
    }

    kmem_cache_compute_properties(cache, flags);

#if SLAB_USE_CPU_POOLS
    for (cpu_pool_type = kmem_cpu_pool_types;
         buf_size <= cpu_pool_type->buf_size;
         cpu_pool_type++);

    cache->cpu_pool_type = cpu_pool_type;

    for (i = 0; i < ARRAY_SIZE(cache->cpu_pools); i++)
        kmem_cpu_pool_init(&cache->cpu_pools[i], cache);
#endif /* SLAB_USE_CPU_POOLS */

    simple_lock(&kmem_cache_list_lock);
    list_insert_tail(&kmem_cache_list, &cache->node);
    kmem_nr_caches++;
    simple_unlock(&kmem_cache_list_lock);
}

static inline int kmem_cache_empty(struct kmem_cache *cache)
{
    return cache->nr_objs == cache->nr_bufs;
}

static int kmem_cache_grow(struct kmem_cache *cache)
{
    struct kmem_slab *slab;
    size_t color;
    int empty;

    simple_lock(&cache->lock);

    if (!kmem_cache_empty(cache)) {
        simple_unlock(&cache->lock);
        return 1;
    }

    color = cache->color;
    cache->color += cache->align;

    if (cache->color > cache->color_max)
        cache->color = 0;

    simple_unlock(&cache->lock);

    slab = kmem_slab_create(cache, color);

    simple_lock(&cache->lock);

    if (slab != NULL) {
        list_insert_head(&cache->free_slabs, &slab->list_node);
        cache->nr_bufs += cache->bufs_per_slab;
        cache->nr_slabs++;
        cache->nr_free_slabs++;
    }

    /*
     * Even if our slab creation failed, another thread might have succeeded
     * in growing the cache.
     */
    empty = kmem_cache_empty(cache);

    simple_unlock(&cache->lock);

    return !empty;
}

static void kmem_cache_reap(struct kmem_cache *cache, struct list *dead_slabs)
{
    simple_lock(&cache->lock);

    list_concat(dead_slabs, &cache->free_slabs);
    list_init(&cache->free_slabs);
    cache->nr_bufs -= cache->bufs_per_slab * cache->nr_free_slabs;
    cache->nr_slabs -= cache->nr_free_slabs;
    cache->nr_free_slabs = 0;

    simple_unlock(&cache->lock);
}

/*
 * Allocate a raw (unconstructed) buffer from the slab layer of a cache.
 *
 * The cache must be locked before calling this function.
 */
static void * kmem_cache_alloc_from_slab(struct kmem_cache *cache)
{
    struct kmem_slab *slab;
    union kmem_bufctl *bufctl;

    if (!list_empty(&cache->partial_slabs))
        slab = list_first_entry(&cache->partial_slabs, struct kmem_slab,
                                list_node);
    else if (!list_empty(&cache->free_slabs))
        slab = list_first_entry(&cache->free_slabs, struct kmem_slab,
                                list_node);
    else
        return NULL;

    bufctl = slab->first_free;
    assert(bufctl != NULL);
    slab->first_free = bufctl->next;
    slab->nr_refs++;
    cache->nr_objs++;

    if (slab->nr_refs == cache->bufs_per_slab) {
        /* The slab has become complete */
        list_remove(&slab->list_node);

        if (slab->nr_refs == 1)
            cache->nr_free_slabs--;
    } else if (slab->nr_refs == 1) {
        /*
         * The slab has become partial. Insert the new slab at the end of
         * the list to reduce fragmentation.
         */
        list_remove(&slab->list_node);
        list_insert_tail(&cache->partial_slabs, &slab->list_node);
        cache->nr_free_slabs--;
    }

    if ((slab->nr_refs == 1) && (cache->flags & KMEM_CF_USE_TREE))
        rbtree_insert(&cache->active_slabs, &slab->tree_node,
                      kmem_slab_cmp_insert);

    return kmem_bufctl_to_buf(bufctl, cache);
}

/*
 * Release a buffer to the slab layer of a cache.
 *
 * The cache must be locked before calling this function.
 */
static void kmem_cache_free_to_slab(struct kmem_cache *cache, void *buf)
{
    struct kmem_slab *slab;
    union kmem_bufctl *bufctl;

    if (cache->flags & KMEM_CF_DIRECT) {
        assert(cache->slab_size == PAGE_SIZE);
        slab = (struct kmem_slab *)P2END((unsigned long)buf, cache->slab_size)
               - 1;
    } else if (cache->flags & KMEM_CF_USE_PAGE) {
        struct vm_page *page;

        page = vm_page_lookup_pa(kvtophys((vm_offset_t)buf));
        assert(page != NULL);
        slab = vm_page_get_priv(page);
    } else {
        struct rbtree_node *node;

        assert(cache->flags & KMEM_CF_USE_TREE);
        node = rbtree_lookup_nearest(&cache->active_slabs, buf,
                                     kmem_slab_cmp_lookup, RBTREE_LEFT);
        assert(node != NULL);
        slab = rbtree_entry(node, struct kmem_slab, tree_node);
    }

    assert((unsigned long)buf >= (unsigned long)slab->addr);
    assert(((unsigned long)buf + cache->buf_size)
           <= vm_page_trunc((unsigned long)slab->addr + cache->slab_size));

    assert(slab->nr_refs >= 1);
    assert(slab->nr_refs <= cache->bufs_per_slab);
    bufctl = kmem_buf_to_bufctl(buf, cache);
    bufctl->next = slab->first_free;
    slab->first_free = bufctl;
    slab->nr_refs--;
    cache->nr_objs--;

    if (slab->nr_refs == 0) {
        /* The slab has become free */

        if (cache->flags & KMEM_CF_USE_TREE)
            rbtree_remove(&cache->active_slabs, &slab->tree_node);

        if (cache->bufs_per_slab > 1)
            list_remove(&slab->list_node);

        list_insert_head(&cache->free_slabs, &slab->list_node);
        cache->nr_free_slabs++;
    } else if (slab->nr_refs == (cache->bufs_per_slab - 1)) {
        /* The slab has become partial */
        list_insert_head(&cache->partial_slabs, &slab->list_node);
    }
}

static void kmem_cache_alloc_verify(struct kmem_cache *cache, void *buf,
                                    int construct)
{
    struct kmem_buftag *buftag;
    union kmem_bufctl *bufctl;
    void *addr;

    buftag = kmem_buf_to_buftag(buf, cache);

    if (buftag->state != KMEM_BUFTAG_FREE)
        kmem_cache_error(cache, buf, KMEM_ERR_BUFTAG, buftag);

    addr = kmem_buf_verify_fill(buf, KMEM_FREE_PATTERN, KMEM_UNINIT_PATTERN,
                                cache->bufctl_dist);

    if (addr != NULL)
        kmem_cache_error(cache, buf, KMEM_ERR_MODIFIED, addr);

    addr = buf + cache->obj_size;
    memset(addr, KMEM_REDZONE_BYTE, cache->redzone_pad);

    bufctl = kmem_buf_to_bufctl(buf, cache);
    bufctl->redzone = KMEM_REDZONE_WORD;
    buftag->state = KMEM_BUFTAG_ALLOC;

    if (construct && (cache->ctor != NULL))
        cache->ctor(buf);
}

vm_offset_t kmem_cache_alloc(struct kmem_cache *cache)
{
    int filled;
    void *buf;

#if SLAB_USE_CPU_POOLS
    struct kmem_cpu_pool *cpu_pool;

    cpu_pool = kmem_cpu_pool_get(cache);

    if (cpu_pool->flags & KMEM_CF_NO_CPU_POOL)
        goto slab_alloc;

    simple_lock(&cpu_pool->lock);

fast_alloc:
    if (likely(cpu_pool->nr_objs > 0)) {
        buf = kmem_cpu_pool_pop(cpu_pool);
        simple_unlock(&cpu_pool->lock);

        if (cpu_pool->flags & KMEM_CF_VERIFY)
            kmem_cache_alloc_verify(cache, buf, KMEM_AV_CONSTRUCT);

        return (vm_offset_t)buf;
    }

    if (cpu_pool->array != NULL) {
        filled = kmem_cpu_pool_fill(cpu_pool, cache);

        if (!filled) {
            simple_unlock(&cpu_pool->lock);

            filled = kmem_cache_grow(cache);

            if (!filled)
                return 0;

            simple_lock(&cpu_pool->lock);
        }

        goto fast_alloc;
    }

    simple_unlock(&cpu_pool->lock);
#endif /* SLAB_USE_CPU_POOLS */

slab_alloc:
    simple_lock(&cache->lock);
    buf = kmem_cache_alloc_from_slab(cache);
    simple_unlock(&cache->lock);

    if (buf == NULL) {
        filled = kmem_cache_grow(cache);

        if (!filled)
            return 0;

        goto slab_alloc;
    }

    if (cache->flags & KMEM_CF_VERIFY)
        kmem_cache_alloc_verify(cache, buf, KMEM_AV_NOCONSTRUCT);

    if (cache->ctor != NULL)
        cache->ctor(buf);

    return (vm_offset_t)buf;
}

static void kmem_cache_free_verify(struct kmem_cache *cache, void *buf)
{
    struct rbtree_node *node;
    struct kmem_buftag *buftag;
    struct kmem_slab *slab;
    union kmem_bufctl *bufctl;
    unsigned char *redzone_byte;
    unsigned long slabend;

    assert(cache->flags & KMEM_CF_USE_TREE);

    simple_lock(&cache->lock);
    node = rbtree_lookup_nearest(&cache->active_slabs, buf,
                                 kmem_slab_cmp_lookup, RBTREE_LEFT);
    simple_unlock(&cache->lock);

    if (node == NULL)
        kmem_cache_error(cache, buf, KMEM_ERR_INVALID, NULL);

    slab = rbtree_entry(node, struct kmem_slab, tree_node);
    slabend = P2ALIGN((unsigned long)slab->addr + cache->slab_size, PAGE_SIZE);

    if ((unsigned long)buf >= slabend)
        kmem_cache_error(cache, buf, KMEM_ERR_INVALID, NULL);

    if ((((unsigned long)buf - (unsigned long)slab->addr) % cache->buf_size)
        != 0)
        kmem_cache_error(cache, buf, KMEM_ERR_INVALID, NULL);

    /*
     * As the buffer address is valid, accessing its buftag is safe.
     */
    buftag = kmem_buf_to_buftag(buf, cache);

    if (buftag->state != KMEM_BUFTAG_ALLOC) {
        if (buftag->state == KMEM_BUFTAG_FREE)
            kmem_cache_error(cache, buf, KMEM_ERR_DOUBLEFREE, NULL);
        else
            kmem_cache_error(cache, buf, KMEM_ERR_BUFTAG, buftag);
    }

    redzone_byte = buf + cache->obj_size;
    bufctl = kmem_buf_to_bufctl(buf, cache);

    while (redzone_byte < (unsigned char *)bufctl) {
        if (*redzone_byte != KMEM_REDZONE_BYTE)
            kmem_cache_error(cache, buf, KMEM_ERR_REDZONE, redzone_byte);

        redzone_byte++;
    }

    if (bufctl->redzone != KMEM_REDZONE_WORD) {
        unsigned long word;

        word = KMEM_REDZONE_WORD;
        redzone_byte = kmem_buf_verify_bytes(&bufctl->redzone, &word,
                                             sizeof(bufctl->redzone));
        kmem_cache_error(cache, buf, KMEM_ERR_REDZONE, redzone_byte);
    }

    kmem_buf_fill(buf, KMEM_FREE_PATTERN, cache->bufctl_dist);
    buftag->state = KMEM_BUFTAG_FREE;
}

void kmem_cache_free(struct kmem_cache *cache, vm_offset_t obj)
{
#if SLAB_USE_CPU_POOLS
    struct kmem_cpu_pool *cpu_pool;
    void **array;

    cpu_pool = kmem_cpu_pool_get(cache);

    if (cpu_pool->flags & KMEM_CF_VERIFY) {
#else /* SLAB_USE_CPU_POOLS */
    if (cache->flags & KMEM_CF_VERIFY) {
#endif /* SLAB_USE_CPU_POOLS */
        kmem_cache_free_verify(cache, (void *)obj);
    }

#if SLAB_USE_CPU_POOLS
    if (cpu_pool->flags & KMEM_CF_NO_CPU_POOL)
        goto slab_free;

    simple_lock(&cpu_pool->lock);

fast_free:
    if (likely(cpu_pool->nr_objs < cpu_pool->size)) {
        kmem_cpu_pool_push(cpu_pool, (void *)obj);
        simple_unlock(&cpu_pool->lock);
        return;
    }

    if (cpu_pool->array != NULL) {
        kmem_cpu_pool_drain(cpu_pool, cache);
        goto fast_free;
    }

    simple_unlock(&cpu_pool->lock);

    array = (void *)kmem_cache_alloc(cache->cpu_pool_type->array_cache);

    if (array != NULL) {
        simple_lock(&cpu_pool->lock);

        /*
         * Another thread may have built the CPU pool while the lock was
         * dropped.
         */
        if (cpu_pool->array != NULL) {
            simple_unlock(&cpu_pool->lock);
            kmem_cache_free(cache->cpu_pool_type->array_cache,
                            (vm_offset_t)array);
            simple_lock(&cpu_pool->lock);
            goto fast_free;
        }

        kmem_cpu_pool_build(cpu_pool, cache, array);
        goto fast_free;
    }

slab_free:
#endif /* SLAB_USE_CPU_POOLS */

    simple_lock(&cache->lock);
    kmem_cache_free_to_slab(cache, (void *)obj);
    simple_unlock(&cache->lock);
}

void slab_collect(void)
{
    struct kmem_cache *cache;
    struct kmem_slab *slab;
    struct list dead_slabs;

    if (elapsed_ticks <= (kmem_gc_last_tick + KMEM_GC_INTERVAL))
        return;

    kmem_gc_last_tick = elapsed_ticks;

    list_init(&dead_slabs);

    simple_lock(&kmem_cache_list_lock);

    list_for_each_entry(&kmem_cache_list, cache, node)
        kmem_cache_reap(cache, &dead_slabs);

    simple_unlock(&kmem_cache_list_lock);

    while (!list_empty(&dead_slabs)) {
        slab = list_first_entry(&dead_slabs, struct kmem_slab, list_node);
        list_remove(&slab->list_node);
        kmem_slab_destroy(slab, slab->cache);
    }
}

void slab_bootstrap(void)
{
    /* Make sure a bufctl can always be stored in a buffer */
    assert(sizeof(union kmem_bufctl) <= KMEM_ALIGN_MIN);

    list_init(&kmem_cache_list);
    simple_lock_init(&kmem_cache_list_lock);
}

void slab_init(void)
{
#if SLAB_USE_CPU_POOLS
    struct kmem_cpu_pool_type *cpu_pool_type;
    char name[KMEM_CACHE_NAME_SIZE];
    size_t i, size;
#endif /* SLAB_USE_CPU_POOLS */

#if SLAB_USE_CPU_POOLS
    for (i = 0; i < ARRAY_SIZE(kmem_cpu_pool_types); i++) {
        cpu_pool_type = &kmem_cpu_pool_types[i];
        cpu_pool_type->array_cache = &kmem_cpu_array_caches[i];
        sprintf(name, "kmem_cpu_array_%d", cpu_pool_type->array_size);
        size = sizeof(void *) * cpu_pool_type->array_size;
        kmem_cache_init(cpu_pool_type->array_cache, name, size,
                        cpu_pool_type->array_align, NULL, 0);
    }
#endif /* SLAB_USE_CPU_POOLS */

    /*
     * Prevent off slab data for the slab cache to avoid infinite recursion.
     */
    kmem_cache_init(&kmem_slab_cache, "kmem_slab", sizeof(struct kmem_slab),
                    0, NULL, KMEM_CACHE_NOOFFSLAB);
}

void kalloc_init(void)
{
    char name[KMEM_CACHE_NAME_SIZE];
    size_t i, size;

    size = 1 << KALLOC_FIRST_SHIFT;

    for (i = 0; i < ARRAY_SIZE(kalloc_caches); i++) {
        sprintf(name, "kalloc_%lu", size);
        kmem_cache_init(&kalloc_caches[i], name, size, 0, NULL, 0);
        size <<= 1;
    }
}

/*
 * Return the kalloc cache index matching the given allocation size, which
 * must be strictly greater than 0.
 */
static inline size_t kalloc_get_index(unsigned long size)
{
    assert(size != 0);

    size = (size - 1) >> KALLOC_FIRST_SHIFT;

    if (size == 0)
        return 0;
    else
        return (sizeof(long) * 8) - __builtin_clzl(size);
}

static void kalloc_verify(struct kmem_cache *cache, void *buf, size_t size)
{
    size_t redzone_size;
    void *redzone;

    assert(size <= cache->obj_size);

    redzone = buf + size;
    redzone_size = cache->obj_size - size;
    memset(redzone, KMEM_REDZONE_BYTE, redzone_size);
}

vm_offset_t kalloc(vm_size_t size)
{
    size_t index;
    void *buf;

    if (size == 0)
        return 0;

    index = kalloc_get_index(size);

    if (index < ARRAY_SIZE(kalloc_caches)) {
        struct kmem_cache *cache;

        cache = &kalloc_caches[index];
        buf = (void *)kmem_cache_alloc(cache);

        if ((buf != 0) && (cache->flags & KMEM_CF_VERIFY))
            kalloc_verify(cache, buf, size);
    } else if (size <= PAGE_SIZE) {
        buf = (void *)kmem_pagealloc_physmem(PAGE_SIZE);
    } else {
        buf = (void *)kmem_pagealloc_virtual(size, 0);
    }

    return (vm_offset_t)buf;
}

static void kfree_verify(struct kmem_cache *cache, void *buf, size_t size)
{
    unsigned char *redzone_byte, *redzone_end;

    assert(size <= cache->obj_size);

    redzone_byte = buf + size;
    redzone_end = buf + cache->obj_size;

    while (redzone_byte < redzone_end) {
        if (*redzone_byte != KMEM_REDZONE_BYTE)
            kmem_cache_error(cache, buf, KMEM_ERR_REDZONE, redzone_byte);

        redzone_byte++;
    }
}

void kfree(vm_offset_t data, vm_size_t size)
{
    size_t index;

    if ((data == 0) || (size == 0))
        return;

    index = kalloc_get_index(size);

    if (index < ARRAY_SIZE(kalloc_caches)) {
        struct kmem_cache *cache;

        cache = &kalloc_caches[index];

        if (cache->flags & KMEM_CF_VERIFY)
            kfree_verify(cache, (void *)data, size);

        kmem_cache_free(cache, data);
    } else if (size <= PAGE_SIZE) {
        kmem_pagefree_physmem(data, PAGE_SIZE);
    } else {
        kmem_pagefree_virtual(data, size);
    }
}

static void _slab_info(int (printx)(const char *fmt, ...))
{
    struct kmem_cache *cache;
    vm_size_t mem_usage, mem_reclaimable, mem_total, mem_total_reclaimable;

    mem_total = 0;
    mem_total_reclaimable = 0;

    printx("cache                         obj slab  bufs   objs   bufs"
           "    total reclaimable\n"
           "name                 flags   size size /slab  usage  count"
           "   memory      memory\n");

    simple_lock(&kmem_cache_list_lock);

    list_for_each_entry(&kmem_cache_list, cache, node) {
        simple_lock(&cache->lock);

        mem_usage = (cache->nr_slabs * cache->slab_size) >> 10;
        mem_reclaimable = (cache->nr_free_slabs * cache->slab_size) >> 10;

        printx("%-20s %04x %7lu %3luk  %4lu %6lu %6lu %7uk %10uk\n",
               cache->name, cache->flags, cache->obj_size,
               cache->slab_size >> 10,
               cache->bufs_per_slab, cache->nr_objs, cache->nr_bufs,
               mem_usage, mem_reclaimable);

        simple_unlock(&cache->lock);

        mem_total += mem_usage;
        mem_total_reclaimable += mem_reclaimable;
    }

    simple_unlock(&kmem_cache_list_lock);

    printx("total: %uk, reclaimable: %uk\n",
           mem_total, mem_total_reclaimable);
}

void slab_info(void)
{
    _slab_info(printf);
}

#if MACH_KDB
#include <ddb/db_output.h>

void db_show_slab_info(void)
{
    _slab_info(db_printf);
}

void db_whatis_slab(vm_offset_t a)
{
    struct kmem_cache *cache;
    int done = 0;

#ifndef SLAB_VERIFY
    db_printf("enabling SLAB_VERIFY is recommended\n");
#endif

    simple_lock(&kmem_cache_list_lock);

    list_for_each_entry(&kmem_cache_list, cache, node) {
        if (a >= (vm_offset_t) cache
                && a < (vm_offset_t) cache + sizeof(*cache))
            db_printf("Cache %s\n", cache->name);

        simple_lock(&cache->lock);

        if (cache->flags & KMEM_CF_USE_TREE) {
            struct rbtree_node *node;

            node = rbtree_lookup_nearest(&cache->active_slabs, (void*) a,
                                         kmem_slab_cmp_lookup, RBTREE_LEFT);
            if (node) {
                struct kmem_slab *slab;
                slab = rbtree_entry(node, struct kmem_slab, tree_node);
                if (a >= (vm_offset_t) slab->addr
                        && a < (vm_offset_t) slab->addr + cache->slab_size) {
                    db_printf("Allocated from cache %s\n", cache->name);
                    done = 1;
                    goto out_cache;
                }
            }
        }

        union kmem_bufctl *free;
        struct kmem_slab *slab;

        list_for_each_entry(&cache->partial_slabs, slab, list_node) {
            if (a >= (vm_offset_t) slab->addr
                && a < (vm_offset_t) slab->addr + cache->slab_size) {
                db_printf("In cache %s\n", cache->name);

                for (free = slab->first_free; free; free = free->next) {
                    void *buf = kmem_bufctl_to_buf(free, cache);

                    if (a >= (vm_offset_t) buf
                            && a < (vm_offset_t) buf + cache->buf_size) {
                        db_printf("  In free list\n");
                        break;
                    }
                }

                done = 1;
                goto out_cache;
            }
        }

        list_for_each_entry(&cache->free_slabs, slab, list_node) {
            if (a >= (vm_offset_t) slab->addr
                && a < (vm_offset_t) slab->addr + cache->slab_size) {
                db_printf("In cache %s\n", cache->name);

                for (free = slab->first_free; free; free = free->next) {
                    void *buf = kmem_bufctl_to_buf(free, cache);

                    if (a >= (vm_offset_t) buf
                            && a < (vm_offset_t) buf + cache->buf_size) {
                        db_printf("  In free list\n");
                        break;
                    }
                }

                done = 1;
                goto out_cache;
            }
        }

out_cache:
        simple_unlock(&cache->lock);
        if (done)
            goto out;
    }

out:
    simple_unlock(&kmem_cache_list_lock);
}

#endif /* MACH_KDB */

#if MACH_DEBUG
kern_return_t host_slab_info(host_t host, cache_info_array_t *infop,
                             unsigned int *infoCntp)
{
    struct kmem_cache *cache;
    cache_info_t *info;
    unsigned int i, nr_caches;
    vm_size_t info_size;
    kern_return_t kr;

    if (host == HOST_NULL)
        return KERN_INVALID_HOST;

    /* Assume the cache list is mostly unaltered once the kernel is ready */

retry:
    /* Harmless unsynchronized access, real value checked later */
    nr_caches = kmem_nr_caches;
    info_size = nr_caches * sizeof(*info);
    info = (cache_info_t *)kalloc(info_size);

    if (info == NULL)
        return KERN_RESOURCE_SHORTAGE;

    i = 0;

    simple_lock(&kmem_cache_list_lock);

    if (nr_caches != kmem_nr_caches) {
        simple_unlock(&kmem_cache_list_lock);
        kfree((vm_offset_t)info, info_size);
        goto retry;
    }

    list_for_each_entry(&kmem_cache_list, cache, node) {
        simple_lock(&cache->lock);
        info[i].flags = cache->flags;
#if SLAB_USE_CPU_POOLS
        info[i].cpu_pool_size = cache->cpu_pool_type->array_size;
#else /* SLAB_USE_CPU_POOLS */
        info[i].cpu_pool_size = 0;
#endif /* SLAB_USE_CPU_POOLS */
        info[i].obj_size = cache->obj_size;
        info[i].align = cache->align;
        info[i].buf_size = cache->buf_size;
        info[i].slab_size = cache->slab_size;
        info[i].bufs_per_slab = cache->bufs_per_slab;
        info[i].nr_objs = cache->nr_objs;
        info[i].nr_bufs = cache->nr_bufs;
        info[i].nr_slabs = cache->nr_slabs;
        info[i].nr_free_slabs = cache->nr_free_slabs;
        strncpy(info[i].name, cache->name, sizeof(info[i].name));
        info[i].name[sizeof(info[i].name) - 1] = '\0';
        simple_unlock(&cache->lock);

        i++;
    }

    simple_unlock(&kmem_cache_list_lock);

    if (nr_caches <= *infoCntp) {
        memcpy(*infop, info, info_size);
    } else {
        vm_offset_t info_addr;
        vm_size_t total_size;
        vm_map_copy_t copy;

        kr = kmem_alloc_pageable(ipc_kernel_map, &info_addr, info_size);

        if (kr != KERN_SUCCESS)
            goto out;

        memcpy((char *)info_addr, info, info_size);
        total_size = round_page(info_size);

        if (info_size < total_size)
            memset((char *)(info_addr + info_size),
                   0, total_size - info_size);

        kr = vm_map_copyin(ipc_kernel_map, info_addr, info_size, TRUE, &copy);
        assert(kr == KERN_SUCCESS);
        *infop = (cache_info_t *)copy;
    }

    *infoCntp = nr_caches;
    kr = KERN_SUCCESS;

out:
    kfree((vm_offset_t)info, info_size);

    return kr;
}
#endif /* MACH_DEBUG */
