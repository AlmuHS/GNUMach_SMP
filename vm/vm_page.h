/* 
 * Mach Operating System
 * Copyright (c) 1993-1988 Carnegie Mellon University
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
/*
 *	File:	vm/vm_page.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Resident memory system definitions.
 */

#ifndef	_VM_VM_PAGE_H_
#define _VM_VM_PAGE_H_

#include <mach/boolean.h>
#include <mach/vm_prot.h>
#include <machine/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_types.h>
#include <kern/queue.h>
#include <kern/lock.h>
#include <kern/log2.h>

#include <kern/macros.h>
#include <kern/sched_prim.h>	/* definitions of wait/wakeup */

#if	MACH_VM_DEBUG
#include <mach_debug/hash_info.h>
#endif

/*
 *	Management of resident (logical) pages.
 *
 *	A small structure is kept for each resident
 *	page, indexed by page number.  Each structure
 *	is an element of several lists:
 *
 *		A hash table bucket used to quickly
 *		perform object/offset lookups
 *
 *		A list of all pages for a given object,
 *		so they can be quickly deactivated at
 *		time of deallocation.
 *
 *		An ordered list of pages due for pageout.
 *
 *	In addition, the structure contains the object
 *	and offset to which this page belongs (for pageout),
 *	and sundry status bits.
 *
 *	Fields in this structure are locked either by the lock on the
 *	object that the page belongs to (O) or by the lock on the page
 *	queues (P).  [Some fields require that both locks be held to
 *	change that field; holding either lock is sufficient to read.]
 */

struct vm_page {
	/* Members used in the vm_page module only */
	struct list node;
	unsigned short type;
	unsigned short seg_index;
	unsigned short order;

	/*
	 * This member is used throughout the code and may only change for
	 * fictitious pages.
	 */
	phys_addr_t phys_addr;

	/* We use an empty struct as the delimiter.  */
	struct {} vm_page_header;
#define VM_PAGE_HEADER_SIZE	offsetof(struct vm_page, vm_page_header)

	queue_chain_t	pageq;		/* queue info for FIFO
					 * queue or free list (P) */
	queue_chain_t	listq;		/* all pages in same object (O) */
	struct vm_page	*next;		/* VP bucket link (O) */

	vm_object_t	object;		/* which object am I in (O,P) */
	vm_offset_t	offset;		/* offset into that object (O,P) */

	unsigned int	wire_count:15,	/* how many wired down maps use me?
					   (O&P) */
	/* boolean_t */	inactive:1,	/* page is in inactive list (P) */
			active:1,	/* page is in active list (P) */
			laundry:1,	/* page is being cleaned now (P)*/
			free:1,		/* page is on free list (P) */
			reference:1,	/* page has been used (P) */
			external:1,	/* page considered external (P) */
			extcounted:1,   /* page counted in ext counts (P) */
			busy:1,		/* page is in transit (O) */
			wanted:1,	/* someone is waiting for page (O) */
			tabled:1,	/* page is in VP table (O) */
			fictitious:1,	/* Physical page doesn't exist (O) */
			private:1,	/* Page should not be returned to
					 *  the free list (O) */
			absent:1,	/* Data has been requested, but is
					 *  not yet available (O) */
			error:1,	/* Data manager was unable to provide
					 *  data due to error (O) */
			dirty:1,	/* Page must be cleaned (O) */
			precious:1,	/* Page is precious; data must be
					 *  returned even if clean (O) */
			overwriting:1;	/* Request to unlock has been made
					 * without having data. (O)
					 * [See vm_object_overwrite] */

	vm_prot_t	page_lock;	/* Uses prohibited by data manager (O) */
	vm_prot_t	unlock_request;	/* Outstanding unlock request (O) */
};

/*
 *	For debugging, this macro can be defined to perform
 *	some useful check on a page structure.
 */

#define VM_PAGE_CHECK(mem)

/*
 *	Each pageable resident page falls into one of three lists:
 *
 *	free	
 *		Available for allocation now.
 *	inactive
 *		Not referenced in any map, but still has an
 *		object/offset-page mapping, and may be dirty.
 *		This is the list of pages that should be
 *		paged out next.
 *	active
 *		A list of pages which have been placed in
 *		at least one physical map.  This list is
 *		ordered, in LRU-like fashion.
 */

extern
vm_page_t	vm_page_queue_fictitious;	/* fictitious free queue */
extern
queue_head_t	vm_page_queue_active;	/* active memory queue */
extern
queue_head_t	vm_page_queue_inactive;	/* inactive memory queue */

extern
int	vm_page_free_count;	/* How many pages are free? */
extern
int	vm_page_fictitious_count;/* How many fictitious pages are free? */
extern
int	vm_page_active_count;	/* How many pages are active? */
extern
int	vm_page_inactive_count;	/* How many pages are inactive? */
extern
int	vm_page_wire_count;	/* How many pages are wired? */
extern
int	vm_page_free_target;	/* How many do we want free? */
extern
int	vm_page_free_min;	/* When to wakeup pageout */
extern
int	vm_page_inactive_target;/* How many do we want inactive? */
extern
int	vm_page_free_reserved;	/* How many pages reserved to do pageout */
extern
int	vm_page_laundry_count;	/* How many pages being laundered? */
extern
int	vm_page_external_limit;	/* Max number of pages for external objects  */

/* Only objects marked with the extcounted bit are included in this total.
   Pages which we scan for possible pageout, but which are not actually
   dirty, don't get considered against the external page limits any more
   in this way.  */
extern
int	vm_page_external_count;	/* How many pages for external objects? */



decl_simple_lock_data(extern,vm_page_queue_lock)/* lock on active and inactive
						   page queues */
decl_simple_lock_data(extern,vm_page_queue_free_lock)
						/* lock on free page queue */

extern unsigned int	vm_page_free_wanted;
				/* how many threads are waiting for memory */

extern vm_offset_t	vm_page_fictitious_addr;
				/* (fake) phys_addr of fictitious pages */

extern void		vm_page_bootstrap(
	vm_offset_t	*startp,
	vm_offset_t	*endp);
extern void		vm_page_module_init(void);

extern vm_page_t	vm_page_lookup(
	vm_object_t	object,
	vm_offset_t	offset);
extern vm_page_t	vm_page_grab_fictitious(void);
extern boolean_t	vm_page_convert(vm_page_t *, boolean_t);
extern void		vm_page_more_fictitious(void);
extern vm_page_t	vm_page_grab(boolean_t);
extern vm_page_t	vm_page_grab_contig(vm_size_t, unsigned int);
extern void		vm_page_free_contig(vm_page_t, vm_size_t);
extern void		vm_page_wait(void (*)(void));
extern vm_page_t	vm_page_alloc(
	vm_object_t	object,
	vm_offset_t	offset);
extern void		vm_page_init(
	vm_page_t	mem);
extern void		vm_page_free(vm_page_t);
extern void		vm_page_activate(vm_page_t);
extern void		vm_page_deactivate(vm_page_t);
extern void		vm_page_rename(
	vm_page_t	mem,
	vm_object_t	new_object,
	vm_offset_t	new_offset);
extern void		vm_page_insert(
	vm_page_t	mem,
	vm_object_t	object,
	vm_offset_t	offset);
extern void		vm_page_remove(
	vm_page_t	mem);

extern void		vm_page_zero_fill(vm_page_t);
extern void		vm_page_copy(vm_page_t src_m, vm_page_t dest_m);

extern void		vm_page_wire(vm_page_t);
extern void		vm_page_unwire(vm_page_t);

#if	MACH_VM_DEBUG
extern unsigned int	vm_page_info(
	hash_info_bucket_t	*info,
	unsigned int		count);
#endif

/*
 *	Functions implemented as macros
 */

#define PAGE_ASSERT_WAIT(m, interruptible)			\
		MACRO_BEGIN					\
		(m)->wanted = TRUE;				\
		assert_wait((event_t) (m), (interruptible));	\
		MACRO_END

#define PAGE_WAKEUP_DONE(m)					\
		MACRO_BEGIN					\
		(m)->busy = FALSE;				\
		if ((m)->wanted) {				\
			(m)->wanted = FALSE;			\
			thread_wakeup(((event_t) m));		\
		}						\
		MACRO_END

#define PAGE_WAKEUP(m)						\
		MACRO_BEGIN					\
		if ((m)->wanted) {				\
			(m)->wanted = FALSE;			\
			thread_wakeup((event_t) (m));		\
		}						\
		MACRO_END

#define VM_PAGE_FREE(p) 			\
		MACRO_BEGIN			\
		vm_page_lock_queues();		\
		vm_page_free(p);		\
		vm_page_unlock_queues();	\
		MACRO_END

/*
 *	Macro to be used in place of pmap_enter()
 */

#define PMAP_ENTER(pmap, virtual_address, page, protection, wired) \
		MACRO_BEGIN					\
		pmap_enter(					\
			(pmap),					\
			(virtual_address),			\
			(page)->phys_addr,			\
			(protection) & ~(page)->page_lock,	\
			(wired)					\
		 );						\
		MACRO_END

#define	VM_PAGE_WAIT(continuation)	vm_page_wait(continuation)

#define vm_page_lock_queues()	simple_lock(&vm_page_queue_lock)
#define vm_page_unlock_queues()	simple_unlock(&vm_page_queue_lock)

#define VM_PAGE_QUEUES_REMOVE(mem)				\
	MACRO_BEGIN						\
	if (mem->active) {					\
		queue_remove(&vm_page_queue_active,		\
			mem, vm_page_t, pageq);			\
		mem->active = FALSE;				\
		vm_page_active_count--;				\
	}							\
								\
	if (mem->inactive) {					\
		queue_remove(&vm_page_queue_inactive,		\
			mem, vm_page_t, pageq);			\
		mem->inactive = FALSE;				\
		vm_page_inactive_count--;			\
	}							\
	MACRO_END

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
 * Physical page management.
 */

/*
 * Address/page conversion and rounding macros (not inline functions to
 * be easily usable on both virtual and physical addresses, which may not
 * have the same type size).
 */
#define vm_page_atop(addr)      ((addr) >> PAGE_SHIFT)
#define vm_page_ptoa(page)      ((page) << PAGE_SHIFT)
#define vm_page_trunc(addr)     P2ALIGN(addr, PAGE_SIZE)
#define vm_page_round(addr)     P2ROUND(addr, PAGE_SIZE)
#define vm_page_aligned(addr)   P2ALIGNED(addr, PAGE_SIZE)

/*
 * Segment selectors.
 *
 * Selector-to-segment-list translation table :
 * DMA          DMA
 * DMA32        DMA32 DMA
 * DIRECTMAP    DIRECTMAP DMA32 DMA
 * HIGHMEM      HIGHMEM DIRECTMAP DMA32 DMA
 */
#define VM_PAGE_SEL_DMA         0
#define VM_PAGE_SEL_DMA32       1
#define VM_PAGE_SEL_DIRECTMAP   2
#define VM_PAGE_SEL_HIGHMEM     3

/*
 * Page usage types.
 *
 * Failing to allocate pmap pages will cause a kernel panic.
 * TODO Obviously, this needs to be addressed, e.g. with a reserved pool of
 * pages.
 */
#define VM_PT_FREE          0   /* Page unused */
#define VM_PT_RESERVED      1   /* Page reserved at boot time */
#define VM_PT_TABLE         2   /* Page is part of the page table */
#define VM_PT_PMAP          3   /* Page stores pmap-specific data */
#define VM_PT_KMEM          4   /* Page is part of a kmem slab */
#define VM_PT_STACK         5   /* Type for generic kernel allocations */
#define VM_PT_KERNEL        6   /* Type for generic kernel allocations */

static inline unsigned short
vm_page_type(const struct vm_page *page)
{
    return page->type;
}

void vm_page_set_type(struct vm_page *page, unsigned int order,
                      unsigned short type);

static inline unsigned int
vm_page_order(size_t size)
{
    return iorder2(vm_page_atop(vm_page_round(size)));
}

static inline phys_addr_t
vm_page_to_pa(const struct vm_page *page)
{
    return page->phys_addr;
}

#if 0
static inline unsigned long
vm_page_direct_va(phys_addr_t pa)
{
    assert(pa < VM_PAGE_DIRECTMAP_LIMIT);
    return ((unsigned long)pa + VM_MIN_DIRECTMAP_ADDRESS);
}

static inline phys_addr_t
vm_page_direct_pa(unsigned long va)
{
    assert(va >= VM_MIN_DIRECTMAP_ADDRESS);
    assert(va < VM_MAX_DIRECTMAP_ADDRESS);
    return (va - VM_MIN_DIRECTMAP_ADDRESS);
}

static inline void *
vm_page_direct_ptr(const struct vm_page *page)
{
    return (void *)vm_page_direct_va(vm_page_to_pa(page));
}
#endif

/*
 * Load physical memory into the vm_page module at boot time.
 *
 * The avail_start and avail_end parameters are used to maintain a simple
 * heap for bootstrap allocations.
 *
 * All addresses must be page-aligned. Segments can be loaded in any order.
 */
void vm_page_load(unsigned int seg_index, phys_addr_t start, phys_addr_t end,
                  phys_addr_t avail_start, phys_addr_t avail_end);

/*
 * Return true if the vm_page module is completely initialized, false
 * otherwise, in which case only vm_page_bootalloc() can be used for
 * allocations.
 */
int vm_page_ready(void);

/*
 * Early allocation function.
 *
 * This function is used by the vm_resident module to implement
 * pmap_steal_memory. It can be used after physical segments have been loaded
 * and before the vm_page module is initialized.
 */
unsigned long vm_page_bootalloc(size_t size);

/*
 * Set up the vm_page module.
 *
 * Architecture-specific code must have loaded segments before calling this
 * function. Segments must comply with the selector-to-segment-list table,
 * e.g. HIGHMEM is loaded if and only if DIRECTMAP, DMA32 and DMA are loaded,
 * notwithstanding segment aliasing.
 *
 * Once this function returns, the vm_page module is ready, and normal
 * allocation functions can be used.
 */
void vm_page_setup(void);

/*
 * Make the given page managed by the vm_page module.
 *
 * If additional memory can be made usable after the VM system is initialized,
 * it should be reported through this function.
 */
void vm_page_manage(struct vm_page *page);

/*
 * Return the page descriptor for the given physical address.
 */
struct vm_page * vm_page_lookup_pa(phys_addr_t pa);

/*
 * Allocate a block of 2^order physical pages.
 *
 * The selector is used to determine the segments from which allocation can
 * be attempted.
 */
struct vm_page * vm_page_alloc_pa(unsigned int order, unsigned int selector,
                                  unsigned short type);

/*
 * Release a block of 2^order physical pages.
 */
void vm_page_free_pa(struct vm_page *page, unsigned int order);

/*
 * Return the name of the given segment.
 */
const char * vm_page_seg_name(unsigned int seg_index);

/*
 * Display internal information about the module.
 */
void vm_page_info_all(void);

/*
 * Return the total amount of physical memory.
 */
phys_addr_t vm_page_mem_size(void);

#endif	/* _VM_VM_PAGE_H_ */
