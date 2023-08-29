/*
 * Copyright (c) 2010-2014 Richard Braun.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * This implementation uses the binary buddy system to manage its heap.
 * Descriptions of the buddy system can be found in the following works :
 * - "UNIX Internals: The New Frontiers", by Uresh Vahalia.
 * - "Dynamic Storage Allocation: A Survey and Critical Review",
 *    by Paul R. Wilson, Mark S. Johnstone, Michael Neely, and David Boles.
 *
 * In addition, this allocator uses per-CPU pools of pages for order 0
 * (i.e. single page) allocations. These pools act as caches (but are named
 * differently to avoid confusion with CPU caches) that reduce contention on
 * multiprocessor systems. When a pool is empty and cannot provide a page,
 * it is filled by transferring multiple pages from the backend buddy system.
 * The symmetric case is handled likewise.
 *
 * TODO Limit number of dirty pages, block allocations above a top limit.
 */

#include <string.h>
#include <kern/assert.h>
#include <kern/counters.h>
#include <kern/cpu_number.h>
#include <kern/debug.h>
#include <kern/list.h>
#include <kern/lock.h>
#include <kern/macros.h>
#include <kern/printf.h>
#include <kern/thread.h>
#include <mach/vm_param.h>
#include <machine/pmap.h>
#include <sys/types.h>
#include <vm/memory_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>

#define DEBUG 0

#define __init
#define __initdata
#define __read_mostly

#define thread_pin()
#define thread_unpin()

/*
 * Number of free block lists per segment.
 */
#define VM_PAGE_NR_FREE_LISTS 11

/*
 * The size of a CPU pool is computed by dividing the number of pages in its
 * containing segment by this value.
 */
#define VM_PAGE_CPU_POOL_RATIO 1024

/*
 * Maximum number of pages in a CPU pool.
 */
#define VM_PAGE_CPU_POOL_MAX_SIZE 128

/*
 * The transfer size of a CPU pool is computed by dividing the pool size by
 * this value.
 */
#define VM_PAGE_CPU_POOL_TRANSFER_RATIO 2

/*
 * Per-processor cache of pages.
 */
struct vm_page_cpu_pool {
    simple_lock_data_t lock;
    int size;
    int transfer_size;
    int nr_pages;
    struct list pages;
} __aligned(CPU_L1_SIZE);

/*
 * Special order value for pages that aren't in a free list. Such pages are
 * either allocated, or part of a free block of pages but not the head page.
 */
#define VM_PAGE_ORDER_UNLISTED (VM_PAGE_NR_FREE_LISTS + 1)

/*
 * Doubly-linked list of free blocks.
 */
struct vm_page_free_list {
    unsigned long size;
    struct list blocks;
};

/*
 * XXX Because of a potential deadlock involving the default pager (see
 * vm_map_lock()), it's currently impossible to reliably determine the
 * minimum number of free pages required for successful pageout. Since
 * that process is dependent on the amount of physical memory, we scale
 * the minimum number of free pages from it, in the hope that memory
 * exhaustion happens as rarely as possible...
 */

/*
 * Ratio used to compute the minimum number of pages in a segment.
 */
#define VM_PAGE_SEG_THRESHOLD_MIN_NUM   5
#define VM_PAGE_SEG_THRESHOLD_MIN_DENOM 100

/*
 * Number of pages reserved for privileged allocations in a segment.
 */
#define VM_PAGE_SEG_THRESHOLD_MIN 500

/*
 * Ratio used to compute the threshold below which pageout is started.
 */
#define VM_PAGE_SEG_THRESHOLD_LOW_NUM   6
#define VM_PAGE_SEG_THRESHOLD_LOW_DENOM 100

/*
 * Minimum value the low threshold can have for a segment.
 */
#define VM_PAGE_SEG_THRESHOLD_LOW 600

#if VM_PAGE_SEG_THRESHOLD_LOW <= VM_PAGE_SEG_THRESHOLD_MIN
#error VM_PAGE_SEG_THRESHOLD_LOW invalid
#endif /* VM_PAGE_SEG_THRESHOLD_LOW >= VM_PAGE_SEG_THRESHOLD_MIN */

/*
 * Ratio used to compute the threshold above which pageout is stopped.
 */
#define VM_PAGE_SEG_THRESHOLD_HIGH_NUM      10
#define VM_PAGE_SEG_THRESHOLD_HIGH_DENOM    100

/*
 * Minimum value the high threshold can have for a segment.
 */
#define VM_PAGE_SEG_THRESHOLD_HIGH 1000

#if VM_PAGE_SEG_THRESHOLD_HIGH <= VM_PAGE_SEG_THRESHOLD_LOW
#error VM_PAGE_SEG_THRESHOLD_HIGH invalid
#endif /* VM_PAGE_SEG_THRESHOLD_HIGH <= VM_PAGE_SEG_THRESHOLD_LOW */

/*
 * Minimum number of pages allowed for a segment.
 */
#define VM_PAGE_SEG_MIN_PAGES 2000

#if VM_PAGE_SEG_MIN_PAGES <= VM_PAGE_SEG_THRESHOLD_HIGH
#error VM_PAGE_SEG_MIN_PAGES invalid
#endif /* VM_PAGE_SEG_MIN_PAGES <= VM_PAGE_SEG_THRESHOLD_HIGH */

/*
 * Ratio used to compute the threshold of active pages beyond which
 * to refill the inactive queue.
 */
#define VM_PAGE_HIGH_ACTIVE_PAGE_NUM    1
#define VM_PAGE_HIGH_ACTIVE_PAGE_DENOM  3

/*
 * Page cache queue.
 *
 * XXX The current implementation hardcodes a preference to evict external
 * pages first and keep internal ones as much as possible. This is because
 * the Hurd default pager implementation suffers from bugs that can easily
 * cause the system to freeze.
 */
struct vm_page_queue {
    struct list internal_pages;
    struct list external_pages;
};

/*
 * Segment name buffer size.
 */
#define VM_PAGE_NAME_SIZE 16

/*
 * Segment of contiguous memory.
 *
 * XXX Per-segment locking is probably useless, since one or both of the
 * page queues lock and the free page queue lock is held on any access.
 * However it should first be made clear which lock protects access to
 * which members of a segment.
 */
struct vm_page_seg {
    struct vm_page_cpu_pool cpu_pools[NCPUS];

    phys_addr_t start;
    phys_addr_t end;
    struct vm_page *pages;
    struct vm_page *pages_end;
    simple_lock_data_t lock;
    struct vm_page_free_list free_lists[VM_PAGE_NR_FREE_LISTS];
    unsigned long nr_free_pages;

    /* Free memory thresholds */
    unsigned long min_free_pages; /* Privileged allocations only */
    unsigned long low_free_pages; /* Pageout daemon starts scanning */
    unsigned long high_free_pages; /* Pageout daemon stops scanning,
                                      unprivileged allocations resume */

    /* Page cache related data */
    struct vm_page_queue active_pages;
    unsigned long nr_active_pages;
    unsigned long high_active_pages;
    struct vm_page_queue inactive_pages;
    unsigned long nr_inactive_pages;
};

/*
 * Bootstrap information about a segment.
 */
struct vm_page_boot_seg {
    phys_addr_t start;
    phys_addr_t end;
    boolean_t heap_present;
    phys_addr_t avail_start;
    phys_addr_t avail_end;
};

static int vm_page_is_ready __read_mostly;

/*
 * Segment table.
 *
 * The system supports a maximum of 4 segments :
 *  - DMA: suitable for DMA
 *  - DMA32: suitable for DMA when devices support 32-bits addressing
 *  - DIRECTMAP: direct physical mapping, allows direct access from
 *    the kernel with a simple offset translation
 *  - HIGHMEM: must be mapped before it can be accessed
 *
 * Segments are ordered by priority, 0 being the lowest priority. Their
 * relative priorities are DMA < DMA32 < DIRECTMAP < HIGHMEM or
 * DMA < DIRECTMAP < DMA32 < HIGHMEM.
 * Some segments may actually be aliases for others, e.g. if DMA is always
 * possible from the direct physical mapping, DMA and DMA32 are aliases for
 * DIRECTMAP, in which case the segment table contains DIRECTMAP and HIGHMEM
 * only.
 */
static struct vm_page_seg vm_page_segs[VM_PAGE_MAX_SEGS];

/*
 * Bootstrap segment table.
 */
static struct vm_page_boot_seg vm_page_boot_segs[VM_PAGE_MAX_SEGS] __initdata;

/*
 * Number of loaded segments.
 */
static unsigned int vm_page_segs_size __read_mostly;

/*
 * If true, unprivileged allocations are blocked, disregarding any other
 * condition.
 *
 * This variable is also used to resume clients once pages are available.
 *
 * The free page queue lock must be held when accessing this variable.
 */
static boolean_t vm_page_alloc_paused;

static void __init
vm_page_init_pa(struct vm_page *page, unsigned short seg_index, phys_addr_t pa)
{
    memset(page, 0, sizeof(*page));
    vm_page_init(page); /* vm_resident members */
    page->type = VM_PT_RESERVED;
    page->seg_index = seg_index;
    page->order = VM_PAGE_ORDER_UNLISTED;
    page->priv = NULL;
    page->phys_addr = pa;
}

void
vm_page_set_type(struct vm_page *page, unsigned int order, unsigned short type)
{
    unsigned int i, nr_pages;

    nr_pages = 1 << order;

    for (i = 0; i < nr_pages; i++)
        page[i].type = type;
}

static boolean_t
vm_page_pageable(const struct vm_page *page)
{
    return (page->object != NULL)
           && (page->wire_count == 0)
           && (page->active || page->inactive);
}

static boolean_t
vm_page_can_move(const struct vm_page *page)
{
    /*
     * This function is called on pages pulled from the page queues,
     * implying they're pageable, which is why the wire count isn't
     * checked here.
     */

    return !page->busy
           && !page->wanted
           && !page->absent
           && page->object->alive;
}

static void
vm_page_remove_mappings(struct vm_page *page)
{
    page->busy = TRUE;
    pmap_page_protect(page->phys_addr, VM_PROT_NONE);

    if (!page->dirty) {
        page->dirty = pmap_is_modified(page->phys_addr);
    }
}

static void __init
vm_page_free_list_init(struct vm_page_free_list *free_list)
{
    free_list->size = 0;
    list_init(&free_list->blocks);
}

static inline void
vm_page_free_list_insert(struct vm_page_free_list *free_list,
                         struct vm_page *page)
{
    assert(page->order == VM_PAGE_ORDER_UNLISTED);

    free_list->size++;
    list_insert_head(&free_list->blocks, &page->node);
}

static inline void
vm_page_free_list_remove(struct vm_page_free_list *free_list,
                         struct vm_page *page)
{
    assert(page->order != VM_PAGE_ORDER_UNLISTED);

    free_list->size--;
    list_remove(&page->node);
}

static struct vm_page *
vm_page_seg_alloc_from_buddy(struct vm_page_seg *seg, unsigned int order)
{
    struct vm_page_free_list *free_list = free_list;
    struct vm_page *page, *buddy;
    unsigned int i;

    assert(order < VM_PAGE_NR_FREE_LISTS);

    if (vm_page_alloc_paused && current_thread()
        && !current_thread()->vm_privilege) {
        return NULL;
    } else if (seg->nr_free_pages <= seg->low_free_pages) {
        vm_pageout_start();

        if ((seg->nr_free_pages <= seg->min_free_pages)
            && current_thread() && !current_thread()->vm_privilege) {
            vm_page_alloc_paused = TRUE;
            return NULL;
        }
    }

    for (i = order; i < VM_PAGE_NR_FREE_LISTS; i++) {
        free_list = &seg->free_lists[i];

        if (free_list->size != 0)
            break;
    }

    if (i == VM_PAGE_NR_FREE_LISTS)
        return NULL;

    page = list_first_entry(&free_list->blocks, struct vm_page, node);
    vm_page_free_list_remove(free_list, page);
    page->order = VM_PAGE_ORDER_UNLISTED;

    while (i > order) {
        i--;
        buddy = &page[1 << i];
        vm_page_free_list_insert(&seg->free_lists[i], buddy);
        buddy->order = i;
    }

    seg->nr_free_pages -= (1 << order);

    if (seg->nr_free_pages < seg->min_free_pages) {
        vm_page_alloc_paused = TRUE;
    }

    return page;
}

static void
vm_page_seg_free_to_buddy(struct vm_page_seg *seg, struct vm_page *page,
                          unsigned int order)
{
    struct vm_page *buddy;
    phys_addr_t pa, buddy_pa;
    unsigned int nr_pages;

    assert(page >= seg->pages);
    assert(page < seg->pages_end);
    assert(page->order == VM_PAGE_ORDER_UNLISTED);
    assert(order < VM_PAGE_NR_FREE_LISTS);

    nr_pages = (1 << order);
    pa = page->phys_addr;

    while (order < (VM_PAGE_NR_FREE_LISTS - 1)) {
        buddy_pa = pa ^ vm_page_ptoa(1ULL << order);

        if ((buddy_pa < seg->start) || (buddy_pa >= seg->end))
            break;

        buddy = &seg->pages[vm_page_atop(buddy_pa - seg->start)];

        if (buddy->order != order)
            break;

        vm_page_free_list_remove(&seg->free_lists[order], buddy);
        buddy->order = VM_PAGE_ORDER_UNLISTED;
        order++;
        pa &= -vm_page_ptoa(1ULL << order);
        page = &seg->pages[vm_page_atop(pa - seg->start)];
    }

    vm_page_free_list_insert(&seg->free_lists[order], page);
    page->order = order;
    seg->nr_free_pages += nr_pages;
}

static void __init
vm_page_cpu_pool_init(struct vm_page_cpu_pool *cpu_pool, int size)
{
    simple_lock_init(&cpu_pool->lock);
    cpu_pool->size = size;
    cpu_pool->transfer_size = (size + VM_PAGE_CPU_POOL_TRANSFER_RATIO - 1)
                              / VM_PAGE_CPU_POOL_TRANSFER_RATIO;
    cpu_pool->nr_pages = 0;
    list_init(&cpu_pool->pages);
}

static inline struct vm_page_cpu_pool *
vm_page_cpu_pool_get(struct vm_page_seg *seg)
{
    return &seg->cpu_pools[cpu_number()];
}

static inline struct vm_page *
vm_page_cpu_pool_pop(struct vm_page_cpu_pool *cpu_pool)
{
    struct vm_page *page;

    assert(cpu_pool->nr_pages != 0);
    cpu_pool->nr_pages--;
    page = list_first_entry(&cpu_pool->pages, struct vm_page, node);
    list_remove(&page->node);
    return page;
}

static inline void
vm_page_cpu_pool_push(struct vm_page_cpu_pool *cpu_pool, struct vm_page *page)
{
    assert(cpu_pool->nr_pages < cpu_pool->size);
    cpu_pool->nr_pages++;
    list_insert_head(&cpu_pool->pages, &page->node);
}

static int
vm_page_cpu_pool_fill(struct vm_page_cpu_pool *cpu_pool,
                      struct vm_page_seg *seg)
{
    struct vm_page *page;
    int i;

    assert(cpu_pool->nr_pages == 0);

    simple_lock(&seg->lock);

    for (i = 0; i < cpu_pool->transfer_size; i++) {
        page = vm_page_seg_alloc_from_buddy(seg, 0);

        if (page == NULL)
            break;

        vm_page_cpu_pool_push(cpu_pool, page);
    }

    simple_unlock(&seg->lock);

    return i;
}

static void
vm_page_cpu_pool_drain(struct vm_page_cpu_pool *cpu_pool,
                       struct vm_page_seg *seg)
{
    struct vm_page *page;
    int i;

    assert(cpu_pool->nr_pages == cpu_pool->size);

    simple_lock(&seg->lock);

    for (i = cpu_pool->transfer_size; i > 0; i--) {
        page = vm_page_cpu_pool_pop(cpu_pool);
        vm_page_seg_free_to_buddy(seg, page, 0);
    }

    simple_unlock(&seg->lock);
}

static void
vm_page_queue_init(struct vm_page_queue *queue)
{
    list_init(&queue->internal_pages);
    list_init(&queue->external_pages);
}

static void
vm_page_queue_push(struct vm_page_queue *queue, struct vm_page *page)
{
    if (page->external) {
        list_insert_tail(&queue->external_pages, &page->node);
    } else {
        list_insert_tail(&queue->internal_pages, &page->node);
    }
}

static void
vm_page_queue_remove(struct vm_page_queue *queue, struct vm_page *page)
{
    (void)queue;
    list_remove(&page->node);
}

static struct vm_page *
vm_page_queue_first(struct vm_page_queue *queue, boolean_t external_only)
{
    struct vm_page *page;

    if (!list_empty(&queue->external_pages)) {
        page = list_first_entry(&queue->external_pages, struct vm_page, node);
        return page;
    }

    if (!external_only && !list_empty(&queue->internal_pages)) {
        page = list_first_entry(&queue->internal_pages, struct vm_page, node);
        return page;
    }

    return NULL;
}

static struct vm_page_seg *
vm_page_seg_get(unsigned short index)
{
    assert(index < vm_page_segs_size);
    return &vm_page_segs[index];
}

static unsigned int
vm_page_seg_index(const struct vm_page_seg *seg)
{
    unsigned int index;

    index = seg - vm_page_segs;
    assert(index < vm_page_segs_size);
    return index;
}

static phys_addr_t __init
vm_page_seg_size(struct vm_page_seg *seg)
{
    return seg->end - seg->start;
}

static int __init
vm_page_seg_compute_pool_size(struct vm_page_seg *seg)
{
    phys_addr_t size;

    size = vm_page_atop(vm_page_seg_size(seg)) / VM_PAGE_CPU_POOL_RATIO;

    if (size == 0)
        size = 1;
    else if (size > VM_PAGE_CPU_POOL_MAX_SIZE)
        size = VM_PAGE_CPU_POOL_MAX_SIZE;

    return size;
}

static void __init
vm_page_seg_compute_pageout_thresholds(struct vm_page_seg *seg)
{
    unsigned long nr_pages;

    nr_pages = vm_page_atop(vm_page_seg_size(seg));

    if (nr_pages < VM_PAGE_SEG_MIN_PAGES) {
        panic("vm_page: segment too small");
    }

    seg->min_free_pages = nr_pages * VM_PAGE_SEG_THRESHOLD_MIN_NUM
                          / VM_PAGE_SEG_THRESHOLD_MIN_DENOM;

    if (seg->min_free_pages < VM_PAGE_SEG_THRESHOLD_MIN) {
        seg->min_free_pages = VM_PAGE_SEG_THRESHOLD_MIN;
    }

    seg->low_free_pages = nr_pages * VM_PAGE_SEG_THRESHOLD_LOW_NUM
                          / VM_PAGE_SEG_THRESHOLD_LOW_DENOM;

    if (seg->low_free_pages < VM_PAGE_SEG_THRESHOLD_LOW) {
        seg->low_free_pages = VM_PAGE_SEG_THRESHOLD_LOW;
    }

    seg->high_free_pages = nr_pages * VM_PAGE_SEG_THRESHOLD_HIGH_NUM
                           / VM_PAGE_SEG_THRESHOLD_HIGH_DENOM;

    if (seg->high_free_pages < VM_PAGE_SEG_THRESHOLD_HIGH) {
        seg->high_free_pages = VM_PAGE_SEG_THRESHOLD_HIGH;
    }
}

static void __init
vm_page_seg_init(struct vm_page_seg *seg, phys_addr_t start, phys_addr_t end,
                 struct vm_page *pages)
{
    phys_addr_t pa;
    int pool_size;
    unsigned int i;

    seg->start = start;
    seg->end = end;
    pool_size = vm_page_seg_compute_pool_size(seg);

    for (i = 0; i < ARRAY_SIZE(seg->cpu_pools); i++)
        vm_page_cpu_pool_init(&seg->cpu_pools[i], pool_size);

    seg->pages = pages;
    seg->pages_end = pages + vm_page_atop(vm_page_seg_size(seg));
    simple_lock_init(&seg->lock);

    for (i = 0; i < ARRAY_SIZE(seg->free_lists); i++)
        vm_page_free_list_init(&seg->free_lists[i]);

    seg->nr_free_pages = 0;

    vm_page_seg_compute_pageout_thresholds(seg);

    vm_page_queue_init(&seg->active_pages);
    seg->nr_active_pages = 0;
    vm_page_queue_init(&seg->inactive_pages);
    seg->nr_inactive_pages = 0;

    i = vm_page_seg_index(seg);

    for (pa = seg->start; pa < seg->end; pa += PAGE_SIZE)
        vm_page_init_pa(&pages[vm_page_atop(pa - seg->start)], i, pa);
}

static struct vm_page *
vm_page_seg_alloc(struct vm_page_seg *seg, unsigned int order,
                  unsigned short type)
{
    struct vm_page_cpu_pool *cpu_pool;
    struct vm_page *page;
    int filled;

    assert(order < VM_PAGE_NR_FREE_LISTS);

    if (order == 0) {
        thread_pin();
        cpu_pool = vm_page_cpu_pool_get(seg);
        simple_lock(&cpu_pool->lock);

        if (cpu_pool->nr_pages == 0) {
            filled = vm_page_cpu_pool_fill(cpu_pool, seg);

            if (!filled) {
                simple_unlock(&cpu_pool->lock);
                thread_unpin();
                return NULL;
            }
        }

        page = vm_page_cpu_pool_pop(cpu_pool);
        simple_unlock(&cpu_pool->lock);
        thread_unpin();
    } else {
        simple_lock(&seg->lock);
        page = vm_page_seg_alloc_from_buddy(seg, order);
        simple_unlock(&seg->lock);

        if (page == NULL)
            return NULL;
    }

    assert(page->type == VM_PT_FREE);
    vm_page_set_type(page, order, type);
    return page;
}

static void
vm_page_seg_free(struct vm_page_seg *seg, struct vm_page *page,
                 unsigned int order)
{
    struct vm_page_cpu_pool *cpu_pool;

    assert(page->type != VM_PT_FREE);
    assert(order < VM_PAGE_NR_FREE_LISTS);

    vm_page_set_type(page, order, VM_PT_FREE);

    if (order == 0) {
        thread_pin();
        cpu_pool = vm_page_cpu_pool_get(seg);
        simple_lock(&cpu_pool->lock);

        if (cpu_pool->nr_pages == cpu_pool->size)
            vm_page_cpu_pool_drain(cpu_pool, seg);

        vm_page_cpu_pool_push(cpu_pool, page);
        simple_unlock(&cpu_pool->lock);
        thread_unpin();
    } else {
        simple_lock(&seg->lock);
        vm_page_seg_free_to_buddy(seg, page, order);
        simple_unlock(&seg->lock);
    }
}

static void
vm_page_seg_add_active_page(struct vm_page_seg *seg, struct vm_page *page)
{
    assert(page->object != NULL);
    assert(page->seg_index == vm_page_seg_index(seg));
    assert(page->type != VM_PT_FREE);
    assert(page->order == VM_PAGE_ORDER_UNLISTED);
    assert(!page->free && !page->active && !page->inactive);
    page->active = TRUE;
    page->reference = TRUE;
    vm_page_queue_push(&seg->active_pages, page);
    seg->nr_active_pages++;
    vm_page_active_count++;
}

static void
vm_page_seg_remove_active_page(struct vm_page_seg *seg, struct vm_page *page)
{
    assert(page->object != NULL);
    assert(page->seg_index == vm_page_seg_index(seg));
    assert(page->type != VM_PT_FREE);
    assert(page->order == VM_PAGE_ORDER_UNLISTED);
    assert(!page->free && page->active && !page->inactive);
    page->active = FALSE;
    vm_page_queue_remove(&seg->active_pages, page);
    seg->nr_active_pages--;
    vm_page_active_count--;
}

static void
vm_page_seg_add_inactive_page(struct vm_page_seg *seg, struct vm_page *page)
{
    assert(page->object != NULL);
    assert(page->seg_index == vm_page_seg_index(seg));
    assert(page->type != VM_PT_FREE);
    assert(page->order == VM_PAGE_ORDER_UNLISTED);
    assert(!page->free && !page->active && !page->inactive);
    page->inactive = TRUE;
    vm_page_queue_push(&seg->inactive_pages, page);
    seg->nr_inactive_pages++;
    vm_page_inactive_count++;
}

static void
vm_page_seg_remove_inactive_page(struct vm_page_seg *seg, struct vm_page *page)
{
    assert(page->object != NULL);
    assert(page->seg_index == vm_page_seg_index(seg));
    assert(page->type != VM_PT_FREE);
    assert(page->order == VM_PAGE_ORDER_UNLISTED);
    assert(!page->free && !page->active && page->inactive);
    page->inactive = FALSE;
    vm_page_queue_remove(&seg->inactive_pages, page);
    seg->nr_inactive_pages--;
    vm_page_inactive_count--;
}

/*
 * Attempt to pull an active page.
 *
 * If successful, the object containing the page is locked.
 */
static struct vm_page *
vm_page_seg_pull_active_page(struct vm_page_seg *seg, boolean_t external_only)
{
    struct vm_page *page, *first;
    boolean_t locked;

    first = NULL;

    for (;;) {
        page = vm_page_queue_first(&seg->active_pages, external_only);

        if (page == NULL) {
            break;
        } else if (first == NULL) {
            first = page;
        } else if (first == page) {
            break;
        }

        vm_page_seg_remove_active_page(seg, page);
        locked = vm_object_lock_try(page->object);

        if (!locked) {
            vm_page_seg_add_active_page(seg, page);
            continue;
        }

        if (!vm_page_can_move(page)) {
            vm_page_seg_add_active_page(seg, page);
            vm_object_unlock(page->object);
            continue;
        }

        return page;
    }

    return NULL;
}

/*
 * Attempt to pull an inactive page.
 *
 * If successful, the object containing the page is locked.
 *
 * XXX See vm_page_seg_pull_active_page (duplicated code).
 */
static struct vm_page *
vm_page_seg_pull_inactive_page(struct vm_page_seg *seg, boolean_t external_only)
{
    struct vm_page *page, *first;
    boolean_t locked;

    first = NULL;

    for (;;) {
        page = vm_page_queue_first(&seg->inactive_pages, external_only);

        if (page == NULL) {
            break;
        } else if (first == NULL) {
            first = page;
        } else if (first == page) {
            break;
        }

        vm_page_seg_remove_inactive_page(seg, page);
        locked = vm_object_lock_try(page->object);

        if (!locked) {
            vm_page_seg_add_inactive_page(seg, page);
            continue;
        }

        if (!vm_page_can_move(page)) {
            vm_page_seg_add_inactive_page(seg, page);
            vm_object_unlock(page->object);
            continue;
        }

        return page;
    }

    return NULL;
}

/*
 * Attempt to pull a page cache page.
 *
 * If successful, the object containing the page is locked.
 */
static struct vm_page *
vm_page_seg_pull_cache_page(struct vm_page_seg *seg,
                            boolean_t external_only,
                            boolean_t *was_active)
{
    struct vm_page *page;

    page = vm_page_seg_pull_inactive_page(seg, external_only);

    if (page != NULL) {
        *was_active = FALSE;
        return page;
    }

    page = vm_page_seg_pull_active_page(seg, external_only);

    if (page != NULL) {
        *was_active = TRUE;
        return page;
    }

    return NULL;
}

static boolean_t
vm_page_seg_page_available(const struct vm_page_seg *seg)
{
    return (seg->nr_free_pages > seg->high_free_pages);
}

static boolean_t
vm_page_seg_usable(const struct vm_page_seg *seg)
{
    if ((seg->nr_active_pages + seg->nr_inactive_pages) == 0) {
        /* Nothing to page out, assume segment is usable */
        return TRUE;
    }

    return (seg->nr_free_pages >= seg->high_free_pages);
}

static void
vm_page_seg_double_lock(struct vm_page_seg *seg1, struct vm_page_seg *seg2)
{
    assert(seg1 != seg2);

    if (seg1 < seg2) {
        simple_lock(&seg1->lock);
        simple_lock(&seg2->lock);
    } else {
        simple_lock(&seg2->lock);
        simple_lock(&seg1->lock);
    }
}

static void
vm_page_seg_double_unlock(struct vm_page_seg *seg1, struct vm_page_seg *seg2)
{
    simple_unlock(&seg1->lock);
    simple_unlock(&seg2->lock);
}

/*
 * Attempt to balance a segment by moving one page to another segment.
 *
 * Return TRUE if a page was actually moved.
 */
static boolean_t
vm_page_seg_balance_page(struct vm_page_seg *seg,
                         struct vm_page_seg *remote_seg)
{
    struct vm_page *src, *dest;
    vm_object_t object;
    vm_offset_t offset;
    boolean_t was_active;

    vm_page_lock_queues();
    simple_lock(&vm_page_queue_free_lock);
    vm_page_seg_double_lock(seg, remote_seg);

    if (vm_page_seg_usable(seg)
        || !vm_page_seg_page_available(remote_seg)) {
        goto error;
    }

    src = vm_page_seg_pull_cache_page(seg, FALSE, &was_active);

    if (src == NULL) {
        goto error;
    }

    assert(src->object != NULL);
    assert(!src->fictitious && !src->private);
    assert(src->wire_count == 0);
    assert(src->type != VM_PT_FREE);
    assert(src->order == VM_PAGE_ORDER_UNLISTED);

    dest = vm_page_seg_alloc_from_buddy(remote_seg, 0);
    assert(dest != NULL);

    vm_page_seg_double_unlock(seg, remote_seg);
    simple_unlock(&vm_page_queue_free_lock);

    if (!was_active && !src->reference && pmap_is_referenced(src->phys_addr)) {
        src->reference = TRUE;
    }

    object = src->object;
    offset = src->offset;
    vm_page_remove(src);

    vm_page_remove_mappings(src);

    vm_page_set_type(dest, 0, src->type);
    memcpy(&dest->vm_page_header, &src->vm_page_header,
           VM_PAGE_BODY_SIZE);
    vm_page_copy(src, dest);

    if (!src->dirty) {
        pmap_clear_modify(dest->phys_addr);
    }

    dest->busy = FALSE;

    simple_lock(&vm_page_queue_free_lock);
    vm_page_init(src);
    src->free = TRUE;
    simple_lock(&seg->lock);
    vm_page_set_type(src, 0, VM_PT_FREE);
    vm_page_seg_free_to_buddy(seg, src, 0);
    simple_unlock(&seg->lock);
    simple_unlock(&vm_page_queue_free_lock);

    vm_object_lock(object);
    vm_page_insert(dest, object, offset);
    vm_object_unlock(object);

    if (was_active) {
        vm_page_activate(dest);
    } else {
        vm_page_deactivate(dest);
    }

    vm_page_unlock_queues();

    return TRUE;

error:
    vm_page_seg_double_unlock(seg, remote_seg);
    simple_unlock(&vm_page_queue_free_lock);
    vm_page_unlock_queues();
    return FALSE;
}

static boolean_t
vm_page_seg_balance(struct vm_page_seg *seg)
{
    struct vm_page_seg *remote_seg;
    unsigned int i;
    boolean_t balanced;

    /*
     * It's important here that pages are moved to lower priority
     * segments first.
     */

    for (i = vm_page_segs_size - 1; i < vm_page_segs_size; i--) {
        remote_seg = vm_page_seg_get(i);

        if (remote_seg == seg) {
            continue;
        }

        balanced = vm_page_seg_balance_page(seg, remote_seg);

        if (balanced) {
            return TRUE;
        }
    }

    return FALSE;
}

static boolean_t
vm_page_seg_evict(struct vm_page_seg *seg, boolean_t external_only,
                  boolean_t alloc_paused)
{
    struct vm_page *page;
    boolean_t reclaim, double_paging;
    vm_object_t object;
    boolean_t was_active;

    page = NULL;
    object = NULL;
    double_paging = FALSE;

restart:
    vm_page_lock_queues();
    simple_lock(&seg->lock);

    if (page != NULL) {
        vm_object_lock(page->object);
    } else {
        page = vm_page_seg_pull_cache_page(seg, external_only, &was_active);

        if (page == NULL) {
            goto out;
        }
    }

    assert(page->object != NULL);
    assert(!page->fictitious && !page->private);
    assert(page->wire_count == 0);
    assert(page->type != VM_PT_FREE);
    assert(page->order == VM_PAGE_ORDER_UNLISTED);

    object = page->object;

    if (!was_active
        && (page->reference || pmap_is_referenced(page->phys_addr))) {
        vm_page_seg_add_active_page(seg, page);
        simple_unlock(&seg->lock);
        vm_object_unlock(object);
        vm_stat.reactivations++;
        current_task()->reactivations++;
        vm_page_unlock_queues();
        page = NULL;
        goto restart;
    }

    vm_page_remove_mappings(page);

    if (!page->dirty && !page->precious) {
        reclaim = TRUE;
        goto out;
    }

    reclaim = FALSE;

    /*
     * If we are very low on memory, then we can't rely on an external
     * pager to clean a dirty page, because external pagers are not
     * vm-privileged.
     *
     * The laundry bit tells vm_pageout_setup not to do any special
     * processing of this page since it's immediately going to be
     * double paged out to the default pager. The laundry bit is
     * reset and the page is inserted into an internal object by
     * vm_pageout_setup before the second double paging pass.
     *
     * There is one important special case: the default pager can
     * back external memory objects. When receiving the first
     * pageout request, where the page is no longer present, a
     * fault could occur, during which the map would be locked.
     * This fault would cause a new paging request to the default
     * pager. Receiving that request would deadlock when trying to
     * lock the map again. Instead, the page isn't double paged
     * and vm_pageout_setup wires the page down, trusting the
     * default pager as for internal pages.
     */

    assert(!page->laundry);
    assert(!(double_paging && page->external));

    if (object->internal || !alloc_paused ||
        memory_manager_default_port(object->pager)) {
        double_paging = FALSE;
    } else {
        double_paging = page->laundry = TRUE;
    }

out:
    simple_unlock(&seg->lock);

    if (object == NULL) {
        vm_page_unlock_queues();
        return FALSE;
    }

    if (reclaim) {
        vm_page_free(page);
        vm_page_unlock_queues();

        if (vm_object_collectable(object)) {
            vm_object_collect(object);
        } else {
            vm_object_unlock(object);
        }

        return TRUE;
    }

    vm_page_unlock_queues();

    /*
     * If there is no memory object for the page, create one and hand it
     * to the default pager. First try to collapse, so we don't create
     * one unnecessarily.
     */

    if (!object->pager_initialized) {
        vm_object_collapse(object);
    }

    if (!object->pager_initialized) {
        vm_object_pager_create(object);
    }

    if (!object->pager_initialized) {
        panic("vm_page_seg_evict");
    }

    vm_pageout_page(page, FALSE, TRUE); /* flush it */
    vm_object_unlock(object);

    if (double_paging) {
        goto restart;
    }

    return TRUE;
}

static void
vm_page_seg_compute_high_active_page(struct vm_page_seg *seg)
{
    unsigned long nr_pages;

    nr_pages = seg->nr_active_pages + seg->nr_inactive_pages;
    seg->high_active_pages = nr_pages * VM_PAGE_HIGH_ACTIVE_PAGE_NUM
                             / VM_PAGE_HIGH_ACTIVE_PAGE_DENOM;
}

static void
vm_page_seg_refill_inactive(struct vm_page_seg *seg)
{
    struct vm_page *page;

    simple_lock(&seg->lock);

    vm_page_seg_compute_high_active_page(seg);

    while (seg->nr_active_pages > seg->high_active_pages) {
        page = vm_page_seg_pull_active_page(seg, FALSE);

        if (page == NULL) {
            break;
        }

        page->reference = FALSE;
        pmap_clear_reference(page->phys_addr);
        vm_page_seg_add_inactive_page(seg, page);
        vm_object_unlock(page->object);
    }

    simple_unlock(&seg->lock);
}

void __init
vm_page_load(unsigned int seg_index, phys_addr_t start, phys_addr_t end)
{
    struct vm_page_boot_seg *seg;

    assert(seg_index < ARRAY_SIZE(vm_page_boot_segs));
    assert(vm_page_aligned(start));
    assert(vm_page_aligned(end));
    assert(start < end);
    assert(vm_page_segs_size < ARRAY_SIZE(vm_page_boot_segs));

    seg = &vm_page_boot_segs[seg_index];
    seg->start = start;
    seg->end = end;
    seg->heap_present = FALSE;

#if DEBUG
    printf("vm_page: load: %s: %llx:%llx\n",
           vm_page_seg_name(seg_index),
           (unsigned long long)start, (unsigned long long)end);
#endif

    vm_page_segs_size++;
}

void
vm_page_load_heap(unsigned int seg_index, phys_addr_t start, phys_addr_t end)
{
    struct vm_page_boot_seg *seg;

    assert(seg_index < ARRAY_SIZE(vm_page_boot_segs));
    assert(vm_page_aligned(start));
    assert(vm_page_aligned(end));

    seg = &vm_page_boot_segs[seg_index];

    assert(seg->start <= start);
    assert(end <= seg-> end);

    seg->avail_start = start;
    seg->avail_end = end;
    seg->heap_present = TRUE;

#if DEBUG
    printf("vm_page: heap: %s: %llx:%llx\n",
           vm_page_seg_name(seg_index),
           (unsigned long long)start, (unsigned long long)end);
#endif
}

int
vm_page_ready(void)
{
    return vm_page_is_ready;
}

static unsigned int
vm_page_select_alloc_seg(unsigned int selector)
{
    unsigned int seg_index;

    switch (selector) {
    case VM_PAGE_SEL_DMA:
        seg_index = VM_PAGE_SEG_DMA;
        break;
    case VM_PAGE_SEL_DMA32:
        seg_index = VM_PAGE_SEG_DMA32;
        break;
    case VM_PAGE_SEL_DIRECTMAP:
        seg_index = VM_PAGE_SEG_DIRECTMAP;
        break;
    case VM_PAGE_SEL_HIGHMEM:
        seg_index = VM_PAGE_SEG_HIGHMEM;
        break;
    default:
        panic("vm_page: invalid selector");
    }

    return MIN(vm_page_segs_size - 1, seg_index);
}

static int __init
vm_page_boot_seg_loaded(const struct vm_page_boot_seg *seg)
{
    return (seg->end != 0);
}

static void __init
vm_page_check_boot_segs(void)
{
    unsigned int i;
    int expect_loaded;

    if (vm_page_segs_size == 0)
        panic("vm_page: no physical memory loaded");

    for (i = 0; i < ARRAY_SIZE(vm_page_boot_segs); i++) {
        expect_loaded = (i < vm_page_segs_size);

        if (vm_page_boot_seg_loaded(&vm_page_boot_segs[i]) == expect_loaded)
            continue;

        panic("vm_page: invalid boot segment table");
    }
}

static phys_addr_t __init
vm_page_boot_seg_size(struct vm_page_boot_seg *seg)
{
    return seg->end - seg->start;
}

static phys_addr_t __init
vm_page_boot_seg_avail_size(struct vm_page_boot_seg *seg)
{
    return seg->avail_end - seg->avail_start;
}

phys_addr_t __init
vm_page_bootalloc(size_t size)
{
    struct vm_page_boot_seg *seg;
    phys_addr_t pa;
    unsigned int i;

    for (i = vm_page_select_alloc_seg(VM_PAGE_SEL_DIRECTMAP);
         i < vm_page_segs_size;
         i--) {
        seg = &vm_page_boot_segs[i];

        if (size <= vm_page_boot_seg_avail_size(seg)) {
            pa = seg->avail_start;
            seg->avail_start += vm_page_round(size);
            return pa;
        }
    }

    panic("vm_page: no physical memory available");
}

void __init
vm_page_setup(void)
{
    struct vm_page_boot_seg *boot_seg;
    struct vm_page_seg *seg;
    struct vm_page *table, *page, *end;
    size_t nr_pages, table_size;
    unsigned long va;
    unsigned int i;
    phys_addr_t pa;

    vm_page_check_boot_segs();

    /*
     * Compute the page table size.
     */
    nr_pages = 0;

    for (i = 0; i < vm_page_segs_size; i++)
        nr_pages += vm_page_atop(vm_page_boot_seg_size(&vm_page_boot_segs[i]));

    table_size = vm_page_round(nr_pages * sizeof(struct vm_page));
    printf("vm_page: page table size: %lu entries (%luk)\n", nr_pages,
           table_size >> 10);
    table = (struct vm_page *)pmap_steal_memory(table_size);
    va = (unsigned long)table;

    /*
     * Initialize the segments, associating them to the page table. When
     * the segments are initialized, all their pages are set allocated.
     * Pages are then released, which populates the free lists.
     */
    for (i = 0; i < vm_page_segs_size; i++) {
        seg = &vm_page_segs[i];
        boot_seg = &vm_page_boot_segs[i];
        vm_page_seg_init(seg, boot_seg->start, boot_seg->end, table);
        page = seg->pages + vm_page_atop(boot_seg->avail_start
                                         - boot_seg->start);
        end = seg->pages + vm_page_atop(boot_seg->avail_end
                                        - boot_seg->start);

        while (page < end) {
            page->type = VM_PT_FREE;
            vm_page_seg_free_to_buddy(seg, page, 0);
            page++;
        }

        table += vm_page_atop(vm_page_seg_size(seg));
    }

    while (va < (unsigned long)table) {
        pa = pmap_extract(kernel_pmap, va);
        page = vm_page_lookup_pa(pa);
        assert((page != NULL) && (page->type == VM_PT_RESERVED));
        page->type = VM_PT_TABLE;
        va += PAGE_SIZE;
    }

    vm_page_is_ready = 1;
}

void __init
vm_page_manage(struct vm_page *page)
{
    assert(page->seg_index < ARRAY_SIZE(vm_page_segs));
    assert(page->type == VM_PT_RESERVED);

    vm_page_set_type(page, 0, VM_PT_FREE);
    vm_page_seg_free_to_buddy(&vm_page_segs[page->seg_index], page, 0);
}

struct vm_page *
vm_page_lookup_pa(phys_addr_t pa)
{
    struct vm_page_seg *seg;
    unsigned int i;

    for (i = 0; i < vm_page_segs_size; i++) {
        seg = &vm_page_segs[i];

        if ((pa >= seg->start) && (pa < seg->end))
            return &seg->pages[vm_page_atop(pa - seg->start)];
    }

    return NULL;
}

static struct vm_page_seg *
vm_page_lookup_seg(const struct vm_page *page)
{
    struct vm_page_seg *seg;
    unsigned int i;

    for (i = 0; i < vm_page_segs_size; i++) {
        seg = &vm_page_segs[i];

        if ((page->phys_addr >= seg->start) && (page->phys_addr < seg->end)) {
            return seg;
        }
    }

    return NULL;
}

void vm_page_check(const struct vm_page *page)
{
    if (page->fictitious) {
        if (page->private) {
            panic("vm_page: page both fictitious and private");
        }

        if (page->phys_addr != vm_page_fictitious_addr) {
            panic("vm_page: invalid fictitious page");
        }
    } else {
        struct vm_page_seg *seg;

        if (page->phys_addr == vm_page_fictitious_addr) {
            panic("vm_page: real page has fictitious address");
        }

        seg = vm_page_lookup_seg(page);

        if (seg == NULL) {
            if (!page->private) {
                panic("vm_page: page claims it's managed but not in any segment");
            }
        } else {
            if (page->private) {
                struct vm_page *real_page;

                if (vm_page_pageable(page)) {
                    panic("vm_page: private page is pageable");
                }

                real_page = vm_page_lookup_pa(page->phys_addr);

                if (vm_page_pageable(real_page)) {
                    panic("vm_page: page underlying private page is pageable");
                }

                if ((real_page->type == VM_PT_FREE)
                    || (real_page->order != VM_PAGE_ORDER_UNLISTED)) {
                    panic("vm_page: page underlying private pagei is free");
                }
            } else {
                unsigned int index;

                index = vm_page_seg_index(seg);

                if (index != page->seg_index) {
                    panic("vm_page: page segment mismatch");
                }
            }
        }
    }
}

struct vm_page *
vm_page_alloc_pa(unsigned int order, unsigned int selector, unsigned short type)
{
    struct vm_page *page;
    unsigned int i;

    for (i = vm_page_select_alloc_seg(selector); i < vm_page_segs_size; i--) {
        page = vm_page_seg_alloc(&vm_page_segs[i], order, type);

        if (page != NULL)
            return page;
    }

    if (!current_thread() || current_thread()->vm_privilege)
        panic("vm_page: privileged thread unable to allocate page");

    return NULL;
}

void
vm_page_free_pa(struct vm_page *page, unsigned int order)
{
    assert(page != NULL);
    assert(page->seg_index < ARRAY_SIZE(vm_page_segs));

    vm_page_seg_free(&vm_page_segs[page->seg_index], page, order);
}

const char *
vm_page_seg_name(unsigned int seg_index)
{
    /* Don't use a switch statement since segments can be aliased */
    if (seg_index == VM_PAGE_SEG_HIGHMEM)
        return "HIGHMEM";
    else if (seg_index == VM_PAGE_SEG_DIRECTMAP)
        return "DIRECTMAP";
    else if (seg_index == VM_PAGE_SEG_DMA32)
        return "DMA32";
    else if (seg_index == VM_PAGE_SEG_DMA)
        return "DMA";
    else
        panic("vm_page: invalid segment index");
}

void
vm_page_info_all(void)
{
    struct vm_page_seg *seg;
    unsigned long pages;
    unsigned int i;

    for (i = 0; i < vm_page_segs_size; i++) {
        seg = &vm_page_segs[i];
        pages = (unsigned long)(seg->pages_end - seg->pages);
        printf("vm_page: %s: pages: %lu (%luM), free: %lu (%luM)\n",
               vm_page_seg_name(i), pages, pages >> (20 - PAGE_SHIFT),
               seg->nr_free_pages, seg->nr_free_pages >> (20 - PAGE_SHIFT));
        printf("vm_page: %s: min:%lu low:%lu high:%lu\n",
               vm_page_seg_name(vm_page_seg_index(seg)),
               seg->min_free_pages, seg->low_free_pages, seg->high_free_pages);
    }
}

phys_addr_t
vm_page_seg_end(unsigned int selector)
{
    return vm_page_segs[vm_page_select_alloc_seg(selector)].end;
}

static unsigned long
vm_page_boot_table_size(void)
{
    unsigned long nr_pages;
    unsigned int i;

    nr_pages = 0;

    for (i = 0; i < vm_page_segs_size; i++) {
        nr_pages += vm_page_atop(vm_page_boot_seg_size(&vm_page_boot_segs[i]));
    }

    return nr_pages;
}

unsigned long
vm_page_table_size(void)
{
    unsigned long nr_pages;
    unsigned int i;

    if (!vm_page_is_ready) {
        return vm_page_boot_table_size();
    }

    nr_pages = 0;

    for (i = 0; i < vm_page_segs_size; i++) {
        nr_pages += vm_page_atop(vm_page_seg_size(&vm_page_segs[i]));
    }

    return nr_pages;
}

unsigned long
vm_page_table_index(phys_addr_t pa)
{
    struct vm_page_seg *seg;
    unsigned long index;
    unsigned int i;

    index = 0;

    for (i = 0; i < vm_page_segs_size; i++) {
        seg = &vm_page_segs[i];

        if ((pa >= seg->start) && (pa < seg->end)) {
            return index + vm_page_atop(pa - seg->start);
        }

        index += vm_page_atop(vm_page_seg_size(seg));
    }

    panic("vm_page: invalid physical address");
}

phys_addr_t
vm_page_mem_size(void)
{
    phys_addr_t total;
    unsigned int i;

    total = 0;

    for (i = 0; i < vm_page_segs_size; i++) {
        total += vm_page_seg_size(&vm_page_segs[i]);
    }

    return total;
}

unsigned long
vm_page_mem_free(void)
{
    unsigned long total;
    unsigned int i;

    total = 0;

    for (i = 0; i < vm_page_segs_size; i++) {
        total += vm_page_segs[i].nr_free_pages;
    }

    return total;
}

/*
 * Mark this page as wired down by yet another map, removing it
 * from paging queues as necessary.
 *
 * The page's object and the page queues must be locked.
 */
void
vm_page_wire(struct vm_page *page)
{
    VM_PAGE_CHECK(page);

    if (page->wire_count == 0) {
        vm_page_queues_remove(page);

        if (!page->private && !page->fictitious) {
            vm_page_wire_count++;
        }
    }

    page->wire_count++;
}

/*
 * Release one wiring of this page, potentially enabling it to be paged again.
 *
 * The page's object and the page queues must be locked.
 */
void
vm_page_unwire(struct vm_page *page)
{
    struct vm_page_seg *seg;

    VM_PAGE_CHECK(page);

    assert(page->wire_count != 0);
    page->wire_count--;

    if ((page->wire_count != 0)
        || page->fictitious
        || page->private) {
        return;
    }

    seg = vm_page_seg_get(page->seg_index);

    simple_lock(&seg->lock);
    vm_page_seg_add_active_page(seg, page);
    simple_unlock(&seg->lock);

    vm_page_wire_count--;
}

/*
 * Returns the given page to the inactive list, indicating that
 * no physical maps have access to this page.
 * [Used by the physical mapping system.]
 *
 * The page queues must be locked.
 */
void
vm_page_deactivate(struct vm_page *page)
{
    struct vm_page_seg *seg;

    VM_PAGE_CHECK(page);

    /*
     * This page is no longer very interesting.  If it was
     * interesting (active or inactive/referenced), then we
     * clear the reference bit and (re)enter it in the
     * inactive queue.  Note wired pages should not have
     * their reference bit cleared.
     */

    if (page->active || (page->inactive && page->reference)) {
        if (!page->fictitious && !page->private && !page->absent) {
            pmap_clear_reference(page->phys_addr);
        }

        page->reference = FALSE;
        vm_page_queues_remove(page);
    }

    if ((page->wire_count == 0) && !page->fictitious
        && !page->private && !page->inactive) {
        seg = vm_page_seg_get(page->seg_index);

        simple_lock(&seg->lock);
        vm_page_seg_add_inactive_page(seg, page);
        simple_unlock(&seg->lock);
    }
}

/*
 * Put the specified page on the active list (if appropriate).
 *
 * The page queues must be locked.
 */
void
vm_page_activate(struct vm_page *page)
{
    struct vm_page_seg *seg;

    VM_PAGE_CHECK(page);

    /*
     * Unconditionally remove so that, even if the page was already
     * active, it gets back to the end of the active queue.
     */
    vm_page_queues_remove(page);

    if ((page->wire_count == 0) && !page->fictitious && !page->private) {
        seg = vm_page_seg_get(page->seg_index);

        if (page->active)
            panic("vm_page_activate: already active");

        simple_lock(&seg->lock);
        vm_page_seg_add_active_page(seg, page);
        simple_unlock(&seg->lock);
    }
}

void
vm_page_queues_remove(struct vm_page *page)
{
    struct vm_page_seg *seg;

    assert(!page->active || !page->inactive);

    if (!page->active && !page->inactive) {
        return;
    }

    seg = vm_page_seg_get(page->seg_index);

    simple_lock(&seg->lock);

    if (page->active) {
        vm_page_seg_remove_active_page(seg, page);
    } else {
        vm_page_seg_remove_inactive_page(seg, page);
    }

    simple_unlock(&seg->lock);
}

/*
 * Check whether segments are all usable for unprivileged allocations.
 *
 * If all segments are usable, resume pending unprivileged allocations
 * and return TRUE.
 *
 * This function acquires vm_page_queue_free_lock, which is held on return.
 */
static boolean_t
vm_page_check_usable(void)
{
    struct vm_page_seg *seg;
    boolean_t usable;
    unsigned int i;

    simple_lock(&vm_page_queue_free_lock);

    for (i = 0; i < vm_page_segs_size; i++) {
        seg = vm_page_seg_get(i);

        simple_lock(&seg->lock);
        usable = vm_page_seg_usable(seg);
        simple_unlock(&seg->lock);

        if (!usable) {
            return FALSE;
        }
    }

    vm_page_external_laundry_count = -1;
    vm_page_alloc_paused = FALSE;
    thread_wakeup(&vm_page_alloc_paused);
    return TRUE;
}

static boolean_t
vm_page_may_balance(void)
{
    struct vm_page_seg *seg;
    boolean_t page_available;
    unsigned int i;

    for (i = 0; i < vm_page_segs_size; i++) {
        seg = vm_page_seg_get(i);

        simple_lock(&seg->lock);
        page_available = vm_page_seg_page_available(seg);
        simple_unlock(&seg->lock);

        if (page_available) {
            return TRUE;
        }
    }

    return FALSE;
}

static boolean_t
vm_page_balance_once(void)
{
    boolean_t balanced;
    unsigned int i;

    /*
     * It's important here that pages are moved from higher priority
     * segments first.
     */

    for (i = 0; i < vm_page_segs_size; i++) {
        balanced = vm_page_seg_balance(vm_page_seg_get(i));

        if (balanced) {
            return TRUE;
        }
    }

    return FALSE;
}

boolean_t
vm_page_balance(void)
{
    boolean_t balanced;

    while (vm_page_may_balance()) {
        balanced = vm_page_balance_once();

        if (!balanced) {
            break;
        }
    }

    return vm_page_check_usable();
}

static boolean_t
vm_page_evict_once(boolean_t external_only, boolean_t alloc_paused)
{
    boolean_t evicted;
    unsigned int i;

    /*
     * It's important here that pages are evicted from lower priority
     * segments first.
     */

    for (i = vm_page_segs_size - 1; i < vm_page_segs_size; i--) {
        evicted = vm_page_seg_evict(vm_page_seg_get(i),
                                    external_only, alloc_paused);

        if (evicted) {
            return TRUE;
        }
    }

    return FALSE;
}

#define VM_PAGE_MAX_LAUNDRY   5
#define VM_PAGE_MAX_EVICTIONS 5

boolean_t
vm_page_evict(boolean_t *should_wait)
{
    boolean_t pause, evicted, external_only, alloc_paused;
    unsigned int i;

    *should_wait = TRUE;
    external_only = TRUE;

    simple_lock(&vm_page_queue_free_lock);
    vm_page_external_laundry_count = 0;
    alloc_paused = vm_page_alloc_paused;
    simple_unlock(&vm_page_queue_free_lock);

again:
    vm_page_lock_queues();
    pause = (vm_page_laundry_count >= VM_PAGE_MAX_LAUNDRY);
    vm_page_unlock_queues();

    if (pause) {
        simple_lock(&vm_page_queue_free_lock);
        return FALSE;
    }

    for (i = 0; i < VM_PAGE_MAX_EVICTIONS; i++) {
        evicted = vm_page_evict_once(external_only, alloc_paused);

        if (!evicted) {
            break;
        }
    }

    simple_lock(&vm_page_queue_free_lock);

    /*
     * Keep in mind eviction may not cause pageouts, since non-precious
     * clean pages are simply released.
     */
    if ((vm_page_laundry_count == 0) && (vm_page_external_laundry_count == 0)) {
        /*
         * No pageout, but some clean pages were freed. Start a complete
         * scan again without waiting.
         */
        if (evicted) {
            *should_wait = FALSE;
            return FALSE;
        }

        /*
         * Eviction failed, consider pages from internal objects on the
         * next attempt.
         */
        if (external_only) {
            simple_unlock(&vm_page_queue_free_lock);
            external_only = FALSE;
            goto again;
        }

        /*
         * TODO Find out what could cause this and how to deal with it.
         * This will likely require an out-of-memory killer.
         */

        {
            static boolean_t warned = FALSE;

            if (!warned) {
                printf("vm_page warning: unable to recycle any page\n");
                warned = 1;
            }
        }
    }

    simple_unlock(&vm_page_queue_free_lock);

    return vm_page_check_usable();
}

void
vm_page_refill_inactive(void)
{
    unsigned int i;

    vm_page_lock_queues();

    for (i = 0; i < vm_page_segs_size; i++) {
        vm_page_seg_refill_inactive(vm_page_seg_get(i));
    }

    vm_page_unlock_queues();
}

void
vm_page_wait(void (*continuation)(void))
{
    assert(!current_thread()->vm_privilege);

    simple_lock(&vm_page_queue_free_lock);

    if (!vm_page_alloc_paused) {
        simple_unlock(&vm_page_queue_free_lock);
        return;
    }

    assert_wait(&vm_page_alloc_paused, FALSE);

    simple_unlock(&vm_page_queue_free_lock);

    if (continuation != 0) {
        counter(c_vm_page_wait_block_user++);
        thread_block(continuation);
    } else {
        counter(c_vm_page_wait_block_kernel++);
        thread_block((void (*)(void)) 0);
    }
}

#if MACH_KDB
#include <ddb/db_output.h>
#define PAGES_PER_MB ((1<<20) / PAGE_SIZE)
void db_show_vmstat(void)
{
	integer_t free_count = vm_page_mem_free();
	unsigned i;

	db_printf("%-20s %10uM\n", "size:",
		(free_count + vm_page_active_count +
		  vm_page_inactive_count + vm_page_wire_count)
		 / PAGES_PER_MB);

	db_printf("%-20s %10uM\n", "free:",
		free_count / PAGES_PER_MB);
	db_printf("%-20s %10uM\n", "active:",
		vm_page_active_count / PAGES_PER_MB);
	db_printf("%-20s %10uM\n", "inactive:",
		vm_page_inactive_count / PAGES_PER_MB);
	db_printf("%-20s %10uM\n", "wired:",
		vm_page_wire_count / PAGES_PER_MB);

	db_printf("%-20s %10uM\n", "zero filled:",
		vm_stat.zero_fill_count / PAGES_PER_MB);
	db_printf("%-20s %10uM\n", "reactivated:",
		vm_stat.reactivations / PAGES_PER_MB);
	db_printf("%-20s %10uM\n", "pageins:",
		vm_stat.pageins / PAGES_PER_MB);
	db_printf("%-20s %10uM\n", "pageouts:",
		vm_stat.pageouts / PAGES_PER_MB);
	db_printf("%-20s %10uM\n", "page faults:",
		vm_stat.faults / PAGES_PER_MB);
	db_printf("%-20s %10uM\n", "cow faults:",
		vm_stat.cow_faults / PAGES_PER_MB);
	db_printf("%-20s %10u%\n", "memobj hit ratio:",
		(vm_stat.hits * 100) / vm_stat.lookups);

	db_printf("%-20s %10u%\n", "cached_memobjs",
		vm_object_external_count);
	db_printf("%-20s %10uM\n", "cache",
		vm_object_external_pages / PAGES_PER_MB);

	for (i = 0; i < vm_page_segs_size; i++)
	{
		db_printf("\nSegment %s:\n", vm_page_seg_name(i));
		db_printf("%-20s %10uM\n", "size:",
			vm_page_seg_size(&vm_page_segs[i]) >> 20);
		db_printf("%-20s %10uM\n", "free:",
			vm_page_segs[i].nr_free_pages / PAGES_PER_MB);
		db_printf("%-20s %10uM\n", "min_free:",
			vm_page_segs[i].min_free_pages / PAGES_PER_MB);
		db_printf("%-20s %10uM\n", "low_free:",
			vm_page_segs[i].low_free_pages / PAGES_PER_MB);
		db_printf("%-20s %10uM\n", "high_free:",
			vm_page_segs[i].high_free_pages / PAGES_PER_MB);
		db_printf("%-20s %10uM\n", "active:",
			vm_page_segs[i].nr_active_pages / PAGES_PER_MB);
		db_printf("%-20s %10uM\n", "high active:",
			vm_page_segs[i].high_active_pages / PAGES_PER_MB);
		db_printf("%-20s %10uM\n", "inactive:",
			vm_page_segs[i].nr_inactive_pages / PAGES_PER_MB);
	}
}
#endif /* MACH_KDB */
