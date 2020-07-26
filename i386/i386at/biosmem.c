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
 */

#include <string.h>
#include <i386/model_dep.h>
#include <i386at/biosmem.h>
#include <kern/assert.h>
#include <kern/debug.h>
#include <kern/macros.h>
#include <kern/printf.h>
#include <mach/vm_param.h>
#include <mach/xen.h>
#include <mach/machine/multiboot.h>
#include <sys/types.h>
#include <vm/vm_page.h>

#define DEBUG 0

#define __boot
#define __bootdata
#define __init

#define boot_memmove    memmove
#define boot_panic(s)   panic("%s", s)
#define boot_strlen     strlen

#define BOOT_CGAMEM     phystokv(0xb8000)
#define BOOT_CGACHARS   (80 * 25)
#define BOOT_CGACOLOR   0x7

#define BIOSMEM_MAX_BOOT_DATA 64

/*
 * Boot data descriptor.
 *
 * The start and end addresses must not be page-aligned, since there
 * could be more than one range inside a single page.
 */
struct biosmem_boot_data {
    phys_addr_t start;
    phys_addr_t end;
    boolean_t temporary;
};

/*
 * Sorted array of boot data descriptors.
 */
static struct biosmem_boot_data biosmem_boot_data_array[BIOSMEM_MAX_BOOT_DATA]
    __bootdata;
static unsigned int biosmem_nr_boot_data __bootdata;

/*
 * Maximum number of entries in the BIOS memory map.
 *
 * Because of adjustments of overlapping ranges, the memory map can grow
 * to twice this size.
 */
#define BIOSMEM_MAX_MAP_SIZE 128

/*
 * Memory range types.
 */
#define BIOSMEM_TYPE_AVAILABLE  1
#define BIOSMEM_TYPE_RESERVED   2
#define BIOSMEM_TYPE_ACPI       3
#define BIOSMEM_TYPE_NVS        4
#define BIOSMEM_TYPE_UNUSABLE   5
#define BIOSMEM_TYPE_DISABLED   6

/*
 * Memory map entry.
 */
struct biosmem_map_entry {
    uint64_t base_addr;
    uint64_t length;
    unsigned int type;
};

/*
 * Memory map built from the information passed by the boot loader.
 *
 * If the boot loader didn't pass a valid memory map, a simple map is built
 * based on the mem_lower and mem_upper multiboot fields.
 */
static struct biosmem_map_entry biosmem_map[BIOSMEM_MAX_MAP_SIZE * 2]
    __bootdata;
static unsigned int biosmem_map_size __bootdata;

/*
 * Contiguous block of physical memory.
 */
struct biosmem_segment {
    phys_addr_t start;
    phys_addr_t end;
};

/*
 * Physical segment boundaries.
 */
static struct biosmem_segment biosmem_segments[VM_PAGE_MAX_SEGS] __bootdata;

/*
 * Boundaries of the simple bootstrap heap.
 *
 * This heap is located above BIOS memory.
 */
static phys_addr_t biosmem_heap_start __bootdata;
static phys_addr_t biosmem_heap_bottom __bootdata;
static phys_addr_t biosmem_heap_top __bootdata;
static phys_addr_t biosmem_heap_end __bootdata;

/*
 * Boot allocation policy.
 *
 * Top-down allocations are normally preferred to avoid unnecessarily
 * filling the DMA segment.
 */
static boolean_t biosmem_heap_topdown __bootdata;

static char biosmem_panic_inval_boot_data[] __bootdata
    = "biosmem: invalid boot data";
static char biosmem_panic_too_many_boot_data[] __bootdata
    = "biosmem: too many boot data ranges";
static char biosmem_panic_too_big_msg[] __bootdata
    = "biosmem: too many memory map entries";
#ifndef MACH_HYP
static char biosmem_panic_setup_msg[] __bootdata
    = "biosmem: unable to set up the early memory allocator";
#endif /* MACH_HYP */
static char biosmem_panic_noseg_msg[] __bootdata
    = "biosmem: unable to find any memory segment";
static char biosmem_panic_inval_msg[] __bootdata
    = "biosmem: attempt to allocate 0 page";
static char biosmem_panic_nomem_msg[] __bootdata
    = "biosmem: unable to allocate memory";

void __boot
biosmem_register_boot_data(phys_addr_t start, phys_addr_t end,
                           boolean_t temporary)
{
    unsigned int i;

    if (start >= end) {
        boot_panic(biosmem_panic_inval_boot_data);
    }

    if (biosmem_nr_boot_data == ARRAY_SIZE(biosmem_boot_data_array)) {
        boot_panic(biosmem_panic_too_many_boot_data);
    }

    for (i = 0; i < biosmem_nr_boot_data; i++) {
        /* Check if the new range overlaps */
        if ((end > biosmem_boot_data_array[i].start)
             && (start < biosmem_boot_data_array[i].end)) {

            /*
             * If it does, check whether it's part of another range.
             * For example, this applies to debugging symbols directly
             * taken from the kernel image.
             */
            if ((start >= biosmem_boot_data_array[i].start)
                && (end <= biosmem_boot_data_array[i].end)) {

                /*
                 * If it's completely included, make sure that a permanent
                 * range remains permanent.
                 *
                 * XXX This means that if one big range is first registered
                 * as temporary, and a smaller range inside of it is
                 * registered as permanent, the bigger range becomes
                 * permanent. It's not easy nor useful in practice to do
                 * better than that.
                 */
                if (biosmem_boot_data_array[i].temporary != temporary) {
                    biosmem_boot_data_array[i].temporary = FALSE;
                }

                return;
            }

            boot_panic(biosmem_panic_inval_boot_data);
        }

        if (end <= biosmem_boot_data_array[i].start) {
            break;
        }
    }

    boot_memmove(&biosmem_boot_data_array[i + 1],
                 &biosmem_boot_data_array[i],
                 (biosmem_nr_boot_data - i) * sizeof(*biosmem_boot_data_array));

    biosmem_boot_data_array[i].start = start;
    biosmem_boot_data_array[i].end = end;
    biosmem_boot_data_array[i].temporary = temporary;
    biosmem_nr_boot_data++;
}

static void __init
biosmem_unregister_boot_data(phys_addr_t start, phys_addr_t end)
{
    unsigned int i;

    if (start >= end) {
        panic("%s", biosmem_panic_inval_boot_data);
    }

    assert(biosmem_nr_boot_data != 0);

    for (i = 0; biosmem_nr_boot_data; i++) {
        if ((start == biosmem_boot_data_array[i].start)
            && (end == biosmem_boot_data_array[i].end)) {
            break;
        }
    }

    if (i == biosmem_nr_boot_data) {
        return;
    }

#if DEBUG
    printf("biosmem: unregister boot data: %llx:%llx\n",
           (unsigned long long)biosmem_boot_data_array[i].start,
           (unsigned long long)biosmem_boot_data_array[i].end);
#endif /* DEBUG */

    biosmem_nr_boot_data--;

    boot_memmove(&biosmem_boot_data_array[i],
                 &biosmem_boot_data_array[i + 1],
                 (biosmem_nr_boot_data - i) * sizeof(*biosmem_boot_data_array));
}

#ifndef MACH_HYP

static void __boot
biosmem_map_build(const struct multiboot_raw_info *mbi)
{
    struct multiboot_raw_mmap_entry *mb_entry, *mb_end;
    struct biosmem_map_entry *start, *entry, *end;
    unsigned long addr;

    addr = phystokv(mbi->mmap_addr);
    mb_entry = (struct multiboot_raw_mmap_entry *)addr;
    mb_end = (struct multiboot_raw_mmap_entry *)(addr + mbi->mmap_length);
    start = biosmem_map;
    entry = start;
    end = entry + BIOSMEM_MAX_MAP_SIZE;

    while ((mb_entry < mb_end) && (entry < end)) {
        entry->base_addr = mb_entry->base_addr;
        entry->length = mb_entry->length;
        entry->type = mb_entry->type;

        mb_entry = (void *)mb_entry + sizeof(mb_entry->size) + mb_entry->size;
        entry++;
    }

    biosmem_map_size = entry - start;
}

static void __boot
biosmem_map_build_simple(const struct multiboot_raw_info *mbi)
{
    struct biosmem_map_entry *entry;

    entry = biosmem_map;
    entry->base_addr = 0;
    entry->length = mbi->mem_lower << 10;
    entry->type = BIOSMEM_TYPE_AVAILABLE;

    entry++;
    entry->base_addr = BIOSMEM_END;
    entry->length = mbi->mem_upper << 10;
    entry->type = BIOSMEM_TYPE_AVAILABLE;

    biosmem_map_size = 2;
}

#endif /* MACH_HYP */

static int __boot
biosmem_map_entry_is_invalid(const struct biosmem_map_entry *entry)
{
    return (entry->base_addr + entry->length) <= entry->base_addr;
}

static void __boot
biosmem_map_filter(void)
{
    struct biosmem_map_entry *entry;
    unsigned int i;

    i = 0;

    while (i < biosmem_map_size) {
        entry = &biosmem_map[i];

        if (biosmem_map_entry_is_invalid(entry)) {
            biosmem_map_size--;
            boot_memmove(entry, entry + 1,
                         (biosmem_map_size - i) * sizeof(*entry));
            continue;
        }

        i++;
    }
}

static void __boot
biosmem_map_sort(void)
{
    struct biosmem_map_entry tmp;
    unsigned int i, j;

    /*
     * Simple insertion sort.
     */
    for (i = 1; i < biosmem_map_size; i++) {
        tmp = biosmem_map[i];

        for (j = i - 1; j < i; j--) {
            if (biosmem_map[j].base_addr < tmp.base_addr)
                break;

            biosmem_map[j + 1] = biosmem_map[j];
        }

        biosmem_map[j + 1] = tmp;
    }
}

static void __boot
biosmem_map_adjust(void)
{
    struct biosmem_map_entry tmp, *a, *b, *first, *second;
    uint64_t a_end, b_end, last_end;
    unsigned int i, j, last_type;

    biosmem_map_filter();

    /*
     * Resolve overlapping areas, giving priority to most restrictive
     * (i.e. numerically higher) types.
     */
    for (i = 0; i < biosmem_map_size; i++) {
        a = &biosmem_map[i];
        a_end = a->base_addr + a->length;

        j = i + 1;

        while (j < biosmem_map_size) {
            b = &biosmem_map[j];
            b_end = b->base_addr + b->length;

            if ((a->base_addr >= b_end) || (a_end <= b->base_addr)) {
                j++;
                continue;
            }

            if (a->base_addr < b->base_addr) {
                first = a;
                second = b;
            } else {
                first = b;
                second = a;
            }

            if (a_end > b_end) {
                last_end = a_end;
                last_type = a->type;
            } else {
                last_end = b_end;
                last_type = b->type;
            }

            tmp.base_addr = second->base_addr;
            tmp.length = MIN(a_end, b_end) - tmp.base_addr;
            tmp.type = MAX(a->type, b->type);
            first->length = tmp.base_addr - first->base_addr;
            second->base_addr += tmp.length;
            second->length = last_end - second->base_addr;
            second->type = last_type;

            /*
             * Filter out invalid entries.
             */
            if (biosmem_map_entry_is_invalid(a)
                && biosmem_map_entry_is_invalid(b)) {
                *a = tmp;
                biosmem_map_size--;
                memmove(b, b + 1, (biosmem_map_size - j) * sizeof(*b));
                continue;
            } else if (biosmem_map_entry_is_invalid(a)) {
                *a = tmp;
                j++;
                continue;
            } else if (biosmem_map_entry_is_invalid(b)) {
                *b = tmp;
                j++;
                continue;
            }

            if (tmp.type == a->type)
                first = a;
            else if (tmp.type == b->type)
                first = b;
            else {

                /*
                 * If the overlapping area can't be merged with one of its
                 * neighbors, it must be added as a new entry.
                 */

                if (biosmem_map_size >= ARRAY_SIZE(biosmem_map))
                    boot_panic(biosmem_panic_too_big_msg);

                biosmem_map[biosmem_map_size] = tmp;
                biosmem_map_size++;
                j++;
                continue;
            }

            if (first->base_addr > tmp.base_addr)
                first->base_addr = tmp.base_addr;

            first->length += tmp.length;
            j++;
        }
    }

    biosmem_map_sort();
}

/*
 * Find addresses of physical memory within a given range.
 *
 * This function considers the memory map with the [*phys_start, *phys_end]
 * range on entry, and returns the lowest address of physical memory
 * in *phys_start, and the highest address of unusable memory immediately
 * following physical memory in *phys_end.
 *
 * These addresses are normally used to establish the range of a segment.
 */
static int __boot
biosmem_map_find_avail(phys_addr_t *phys_start, phys_addr_t *phys_end)
{
    const struct biosmem_map_entry *entry, *map_end;
    phys_addr_t seg_start, seg_end;
    uint64_t start, end;

    seg_start = (phys_addr_t)-1;
    seg_end = (phys_addr_t)-1;
    map_end = biosmem_map + biosmem_map_size;

    for (entry = biosmem_map; entry < map_end; entry++) {
        if (entry->type != BIOSMEM_TYPE_AVAILABLE)
            continue;

        start = vm_page_round(entry->base_addr);

        if (start >= *phys_end)
            break;

        end = vm_page_trunc(entry->base_addr + entry->length);

        if ((start < end) && (start < *phys_end) && (end > *phys_start)) {
            if (seg_start == (phys_addr_t)-1)
                seg_start = start;

            seg_end = end;
        }
    }

    if ((seg_start == (phys_addr_t)-1) || (seg_end == (phys_addr_t)-1))
        return -1;

    if (seg_start > *phys_start)
        *phys_start = seg_start;

    if (seg_end < *phys_end)
        *phys_end = seg_end;

    return 0;
}

static void __boot
biosmem_set_segment(unsigned int seg_index, phys_addr_t start, phys_addr_t end)
{
    biosmem_segments[seg_index].start = start;
    biosmem_segments[seg_index].end = end;
}

static phys_addr_t __boot
biosmem_segment_end(unsigned int seg_index)
{
    return biosmem_segments[seg_index].end;
}

static phys_addr_t __boot
biosmem_segment_size(unsigned int seg_index)
{
    return biosmem_segments[seg_index].end - biosmem_segments[seg_index].start;
}

static int __boot
biosmem_find_avail_clip(phys_addr_t *avail_start, phys_addr_t *avail_end,
                        phys_addr_t data_start, phys_addr_t data_end)
{
    phys_addr_t orig_end;

    assert(data_start < data_end);

    orig_end = data_end;
    data_start = vm_page_trunc(data_start);
    data_end = vm_page_round(data_end);

    if (data_end < orig_end) {
        boot_panic(biosmem_panic_inval_boot_data);
    }

    if ((data_end <= *avail_start) || (data_start >= *avail_end)) {
        return 0;
    }

    if (data_start > *avail_start) {
        *avail_end = data_start;
    } else {
        if (data_end >= *avail_end) {
            return -1;
        }

        *avail_start = data_end;
    }

    return 0;
}

/*
 * Find available memory in the given range.
 *
 * The search starts at the given start address, up to the given end address.
 * If a range is found, it is stored through the avail_startp and avail_endp
 * pointers.
 *
 * The range boundaries are page-aligned on return.
 */
static int __boot
biosmem_find_avail(phys_addr_t start, phys_addr_t end,
                   phys_addr_t *avail_start, phys_addr_t *avail_end)
{
    phys_addr_t orig_start;
    unsigned int i;
    int error;

    assert(start <= end);

    orig_start = start;
    start = vm_page_round(start);
    end = vm_page_trunc(end);

    if ((start < orig_start) || (start >= end)) {
        return -1;
    }

    *avail_start = start;
    *avail_end = end;

    for (i = 0; i < biosmem_nr_boot_data; i++) {
        error = biosmem_find_avail_clip(avail_start, avail_end,
                                        biosmem_boot_data_array[i].start,
                                        biosmem_boot_data_array[i].end);

        if (error) {
            return -1;
        }
    }

    return 0;
}

#ifndef MACH_HYP

static void __boot
biosmem_setup_allocator(const struct multiboot_raw_info *mbi)
{
    phys_addr_t heap_start, heap_end, max_heap_start, max_heap_end;
    phys_addr_t start, end;
    int error;

    /*
     * Find some memory for the heap. Look for the largest unused area in
     * upper memory, carefully avoiding all boot data.
     */
    end = vm_page_trunc((mbi->mem_upper + 1024) << 10);

#ifndef __LP64__
    if (end > VM_PAGE_DIRECTMAP_LIMIT)
        end = VM_PAGE_DIRECTMAP_LIMIT;
#endif /* __LP64__ */

    max_heap_start = 0;
    max_heap_end = 0;
    start = BIOSMEM_END;

    for (;;) {
        error = biosmem_find_avail(start, end, &heap_start, &heap_end);

        if (error) {
            break;
        }

        if ((heap_end - heap_start) > (max_heap_end - max_heap_start)) {
            max_heap_start = heap_start;
            max_heap_end = heap_end;
        }

        start = heap_end;
    }

    if (max_heap_start >= max_heap_end)
        boot_panic(biosmem_panic_setup_msg);

    biosmem_heap_start = max_heap_start;
    biosmem_heap_end = max_heap_end;
    biosmem_heap_bottom = biosmem_heap_start;
    biosmem_heap_top = biosmem_heap_end;
    biosmem_heap_topdown = TRUE;

    /* Prevent biosmem_free_usable() from releasing the heap */
    biosmem_register_boot_data(biosmem_heap_start, biosmem_heap_end, FALSE);
}

#endif /* MACH_HYP */

static void __boot
biosmem_bootstrap_common(void)
{
    phys_addr_t phys_start, phys_end;
    int error;

    biosmem_map_adjust();

    phys_start = BIOSMEM_BASE;
    phys_end = VM_PAGE_DMA_LIMIT;
    error = biosmem_map_find_avail(&phys_start, &phys_end);

    if (error)
        boot_panic(biosmem_panic_noseg_msg);

    biosmem_set_segment(VM_PAGE_SEG_DMA, phys_start, phys_end);

    phys_start = VM_PAGE_DMA_LIMIT;
#ifdef VM_PAGE_DMA32_LIMIT
    phys_end = VM_PAGE_DMA32_LIMIT;
    error = biosmem_map_find_avail(&phys_start, &phys_end);

    if (error)
        return;

    biosmem_set_segment(VM_PAGE_SEG_DMA32, phys_start, phys_end);

    phys_start = VM_PAGE_DMA32_LIMIT;
#endif /* VM_PAGE_DMA32_LIMIT */
    phys_end = VM_PAGE_DIRECTMAP_LIMIT;
    error = biosmem_map_find_avail(&phys_start, &phys_end);

    if (error)
        return;

    biosmem_set_segment(VM_PAGE_SEG_DIRECTMAP, phys_start, phys_end);

    phys_start = VM_PAGE_DIRECTMAP_LIMIT;
    phys_end = VM_PAGE_HIGHMEM_LIMIT;
    error = biosmem_map_find_avail(&phys_start, &phys_end);

    if (error)
        return;

    biosmem_set_segment(VM_PAGE_SEG_HIGHMEM, phys_start, phys_end);
}

#ifdef MACH_HYP

void
biosmem_xen_bootstrap(void)
{
    struct biosmem_map_entry *entry;

    entry = biosmem_map;
    entry->base_addr = 0;
    entry->length = boot_info.nr_pages << PAGE_SHIFT;
    entry->type = BIOSMEM_TYPE_AVAILABLE;

    biosmem_map_size = 1;

    biosmem_bootstrap_common();

    biosmem_heap_start = _kvtophys(boot_info.pt_base)
                         + (boot_info.nr_pt_frames + 3) * 0x1000;
    biosmem_heap_end = boot_info.nr_pages << PAGE_SHIFT;

#ifndef __LP64__
    if (biosmem_heap_end > VM_PAGE_DIRECTMAP_LIMIT)
        biosmem_heap_end = VM_PAGE_DIRECTMAP_LIMIT;
#endif /* __LP64__ */

    biosmem_heap_bottom = biosmem_heap_start;
    biosmem_heap_top = biosmem_heap_end;

    /*
     * XXX Allocation on Xen are initially bottom-up :
     * At the "start of day", only 512k are available after the boot
     * data. The pmap module then creates a 4g mapping so all physical
     * memory is available, but it uses this allocator to do so.
     * Therefore, it must return pages from this small 512k regions
     * first.
     */
    biosmem_heap_topdown = FALSE;

    /*
     * Prevent biosmem_free_usable() from releasing the Xen boot information
     * and the heap.
     */
    biosmem_register_boot_data(0, biosmem_heap_end, FALSE);
}

#else /* MACH_HYP */

void __boot
biosmem_bootstrap(const struct multiboot_raw_info *mbi)
{
    if (mbi->flags & MULTIBOOT_LOADER_MMAP)
        biosmem_map_build(mbi);
    else
        biosmem_map_build_simple(mbi);

    biosmem_bootstrap_common();
    biosmem_setup_allocator(mbi);
}

#endif /* MACH_HYP */

unsigned long __boot
biosmem_bootalloc(unsigned int nr_pages)
{
    unsigned long addr, size;

    size = vm_page_ptoa(nr_pages);

    if (size == 0)
        boot_panic(biosmem_panic_inval_msg);

    if (biosmem_heap_topdown) {
        addr = biosmem_heap_top - size;

        if ((addr < biosmem_heap_start) || (addr > biosmem_heap_top)) {
            boot_panic(biosmem_panic_nomem_msg);
        }

        biosmem_heap_top = addr;
    } else {
        unsigned long end;

        addr = biosmem_heap_bottom;
        end = addr + size;

        if ((end > biosmem_heap_end) || (end < biosmem_heap_bottom)) {
            boot_panic(biosmem_panic_nomem_msg);
        }

        biosmem_heap_bottom = end;
    }

    return addr;
}

phys_addr_t __boot
biosmem_directmap_end(void)
{
    if (biosmem_segment_size(VM_PAGE_SEG_DIRECTMAP) != 0)
        return biosmem_segment_end(VM_PAGE_SEG_DIRECTMAP);
    else if (biosmem_segment_size(VM_PAGE_SEG_DMA32) != 0)
        return biosmem_segment_end(VM_PAGE_SEG_DMA32);
    else
        return biosmem_segment_end(VM_PAGE_SEG_DMA);
}

static const char * __init
biosmem_type_desc(unsigned int type)
{
    switch (type) {
    case BIOSMEM_TYPE_AVAILABLE:
        return "available";
    case BIOSMEM_TYPE_RESERVED:
        return "reserved";
    case BIOSMEM_TYPE_ACPI:
        return "ACPI";
    case BIOSMEM_TYPE_NVS:
        return "ACPI NVS";
    case BIOSMEM_TYPE_UNUSABLE:
        return "unusable";
    default:
        return "unknown (reserved)";
    }
}

static void __init
biosmem_map_show(void)
{
    const struct biosmem_map_entry *entry, *end;

    printf("biosmem: physical memory map:\n");

    for (entry = biosmem_map, end = entry + biosmem_map_size;
         entry < end;
         entry++)
        printf("biosmem: %018llx:%018llx, %s\n", entry->base_addr,
               entry->base_addr + entry->length,
               biosmem_type_desc(entry->type));

#if DEBUG
    printf("biosmem: heap: %llx:%llx\n",
           (unsigned long long)biosmem_heap_start,
           (unsigned long long)biosmem_heap_end);
#endif
}

static void __init
biosmem_load_segment(struct biosmem_segment *seg, uint64_t max_phys_end)
{
    phys_addr_t phys_start, phys_end, avail_start, avail_end;
    unsigned int seg_index;

    phys_start = seg->start;
    phys_end = seg->end;
    seg_index = seg - biosmem_segments;

    if (phys_end > max_phys_end) {
        if (max_phys_end <= phys_start) {
            printf("biosmem: warning: segment %s physically unreachable, "
                   "not loaded\n", vm_page_seg_name(seg_index));
            return;
        }

        printf("biosmem: warning: segment %s truncated to %#llx\n",
               vm_page_seg_name(seg_index), max_phys_end);
        phys_end = max_phys_end;
    }

    vm_page_load(seg_index, phys_start, phys_end);

    /*
     * Clip the remaining available heap to fit it into the loaded
     * segment if possible.
     */

    if ((biosmem_heap_top > phys_start) && (biosmem_heap_bottom < phys_end)) {
        if (biosmem_heap_bottom >= phys_start) {
            avail_start = biosmem_heap_bottom;
        } else {
            avail_start = phys_start;
        }

        if (biosmem_heap_top <= phys_end) {
            avail_end = biosmem_heap_top;
        } else {
            avail_end = phys_end;
        }

        vm_page_load_heap(seg_index, avail_start, avail_end);
    }
}

void __init
biosmem_setup(void)
{
    struct biosmem_segment *seg;
    unsigned int i;

    biosmem_map_show();

    for (i = 0; i < ARRAY_SIZE(biosmem_segments); i++) {
        if (biosmem_segment_size(i) == 0)
            break;

        seg = &biosmem_segments[i];
        biosmem_load_segment(seg, VM_PAGE_HIGHMEM_LIMIT);
    }
}

static void __init
biosmem_unregister_temporary_boot_data(void)
{
    struct biosmem_boot_data *data;
    unsigned int i;

    for (i = 0; i < biosmem_nr_boot_data; i++) {
        data = &biosmem_boot_data_array[i];

        if (!data->temporary) {
            continue;
        }

        biosmem_unregister_boot_data(data->start, data->end);
        i = (unsigned int)-1;
    }
}

static void __init
biosmem_free_usable_range(phys_addr_t start, phys_addr_t end)
{
    struct vm_page *page;

#if DEBUG
    printf("biosmem: release to vm_page: %llx:%llx (%lluk)\n",
           (unsigned long long)start, (unsigned long long)end,
           (unsigned long long)((end - start) >> 10));
#endif

    while (start < end) {
        page = vm_page_lookup_pa(start);
        assert(page != NULL);
        vm_page_manage(page);
        start += PAGE_SIZE;
    }
}

static void __init
biosmem_free_usable_entry(phys_addr_t start, phys_addr_t end)
{
    phys_addr_t avail_start, avail_end;
    int error;

    for (;;) {
        error = biosmem_find_avail(start, end, &avail_start, &avail_end);

        if (error) {
            break;
        }

        biosmem_free_usable_range(avail_start, avail_end);
        start = avail_end;
    }
}

void __init
biosmem_free_usable(void)
{
    struct biosmem_map_entry *entry;
    uint64_t start, end;
    unsigned int i;

    biosmem_unregister_temporary_boot_data();

    for (i = 0; i < biosmem_map_size; i++) {
        entry = &biosmem_map[i];

        if (entry->type != BIOSMEM_TYPE_AVAILABLE)
            continue;

        start = vm_page_round(entry->base_addr);

        if (start >= VM_PAGE_HIGHMEM_LIMIT)
            break;

        end = vm_page_trunc(entry->base_addr + entry->length);

        if (end > VM_PAGE_HIGHMEM_LIMIT) {
            end = VM_PAGE_HIGHMEM_LIMIT;
        }

        if (start < BIOSMEM_BASE)
            start = BIOSMEM_BASE;

        if (start >= end) {
            continue;
        }

        biosmem_free_usable_entry(start, end);
    }
}
