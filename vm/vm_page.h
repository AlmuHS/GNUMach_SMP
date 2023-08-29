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
#include <kern/list.h>
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
	struct list node;		/* page queues or free list (P) */
	void *priv;

	/*
	 * This member is used throughout the code and may only change for
	 * fictitious pages.
	 */
	phys_addr_t phys_addr;

	queue_chain_t	listq;		/* all pages in same object (O) */
	struct vm_page	*next;		/* VP bucket link (O) */

	/* We use an empty struct as the delimiter.  */
	struct {} vm_page_header;

	vm_object_t	object;		/* which object am I in (O,P) */
	vm_offset_t	offset;		/* offset into that object (O,P) */

	unsigned int	wire_count:15,	/* how many wired down maps use me?
					   (O&P) */
	/* boolean_t */	inactive:1,	/* page is in inactive list (P) */
			active:1,	/* page is in active list (P) */
			laundry:1,	/* page is being cleaned now (P)*/
			external_laundry:1,	/* same as laundry for external pagers (P)*/
			free:1,		/* page is on free list (P) */
			reference:1,	/* page has been used (P) */
			external:1,	/* page in external object (P) */
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

	vm_prot_t	page_lock:3;	/* Uses prohibited by data manager (O) */
	vm_prot_t	unlock_request:3;	/* Outstanding unlock request (O) */

	struct {} vm_page_footer;

	unsigned short type:2;
	unsigned short seg_index:2;
	unsigned short order:4;
};

#define VM_PAGE_BODY_SIZE					\
		(offsetof(struct vm_page, vm_page_footer)	\
		- offsetof(struct vm_page, vm_page_header))

/*
 *	For debugging, this macro can be defined to perform
 *	some useful check on a page structure.
 */

#define VM_PAGE_CHECK(mem) vm_page_check(mem)

void vm_page_check(const struct vm_page *page);

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

#define VM_PAGE_DMA		0x01
#if defined(VM_PAGE_DMA32_LIMIT) && VM_PAGE_DMA32_LIMIT > VM_PAGE_DIRECTMAP_LIMIT
#define VM_PAGE_DIRECTMAP      0x02
#define VM_PAGE_DMA32          0x04
#else
#define VM_PAGE_DMA32		0x02
#define VM_PAGE_DIRECTMAP	0x04
#endif
#define VM_PAGE_HIGHMEM		0x08

extern
int	vm_page_fictitious_count;/* How many fictitious pages are free? */
extern
int	vm_page_active_count;	/* How many pages are active? */
extern
int	vm_page_inactive_count;	/* How many pages are inactive? */
extern
int	vm_page_wire_count;	/* How many pages are wired? */
extern
int	vm_page_laundry_count;	/* How many pages being laundered? */
extern
int	vm_page_external_laundry_count;	/* How many external pages being paged out? */

decl_simple_lock_data(extern,vm_page_queue_lock)/* lock on active and inactive
						   page queues */
decl_simple_lock_data(extern,vm_page_queue_free_lock)
						/* lock on free page queue */

extern phys_addr_t	vm_page_fictitious_addr;
				/* (fake) phys_addr of fictitious pages */

extern void		vm_page_bootstrap(
	vm_offset_t	*startp,
	vm_offset_t	*endp);
extern void		vm_page_module_init(void);

extern vm_page_t	vm_page_lookup(
	vm_object_t	object,
	vm_offset_t	offset);
extern vm_page_t	vm_page_grab_fictitious(void);
extern boolean_t	vm_page_convert(vm_page_t *);
extern void		vm_page_more_fictitious(void);
extern vm_page_t	vm_page_grab(unsigned flags);
extern void		vm_page_release(vm_page_t, boolean_t, boolean_t);
extern phys_addr_t	vm_page_grab_phys_addr(void);
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

#define VM_PAGE_QUEUES_REMOVE(mem) vm_page_queues_remove(mem)

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
 * if 32bit PAE
 * DIRECTMAP    DMA32 DMA
 * DMA32        DMA32 DIRECTMAP DMA
 * HIGHMEM      HIGHMEM DMA32 DIRECTMAP DMA
 * else
 * DMA32        DMA32 DMA
 * DIRECTMAP    DIRECTMAP DMA32 DMA
 * HIGHMEM      HIGHMEM DIRECTMAP DMA32 DMA
 * endif
 */
#define VM_PAGE_SEL_DMA         0
#if defined(VM_PAGE_DMA32_LIMIT) && VM_PAGE_DMA32_LIMIT > VM_PAGE_DIRECTMAP_LIMIT
#define VM_PAGE_SEL_DIRECTMAP   1
#define VM_PAGE_SEL_DMA32       2
#else
#define VM_PAGE_SEL_DMA32       1
#define VM_PAGE_SEL_DIRECTMAP   2
#endif
#define VM_PAGE_SEL_HIGHMEM     3

/*
 * Page usage types.
 */
#define VM_PT_FREE          0   /* Page unused */
#define VM_PT_RESERVED      1   /* Page reserved at boot time */
#define VM_PT_TABLE         2   /* Page is part of the page table */
#define VM_PT_KERNEL        3   /* Type for generic kernel allocations */

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

/*
 * Associate private data with a page.
 */
static inline void
vm_page_set_priv(struct vm_page *page, void *priv)
{
    page->priv = priv;
}

static inline void *
vm_page_get_priv(const struct vm_page *page)
{
    return page->priv;
}

/*
 * Load physical memory into the vm_page module at boot time.
 *
 * All addresses must be page-aligned. Segments can be loaded in any order.
 */
void vm_page_load(unsigned int seg_index, phys_addr_t start, phys_addr_t end);

/*
 * Load available physical memory into the vm_page module at boot time.
 *
 * The segment referred to must have been loaded with vm_page_load
 * before loading its heap.
 */
void vm_page_load_heap(unsigned int seg_index, phys_addr_t start,
                       phys_addr_t end);

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
phys_addr_t vm_page_bootalloc(size_t size);

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
 *
 * This function should only be used by the vm_resident module.
 */
struct vm_page * vm_page_alloc_pa(unsigned int order, unsigned int selector,
                                  unsigned short type);

/*
 * Release a block of 2^order physical pages.
 *
 * This function should only be used by the vm_resident module.
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
 * Return the maximum physical address for a given segment selector.
 */
phys_addr_t vm_page_seg_end(unsigned int selector);

/*
 * Return the total number of physical pages.
 */
unsigned long vm_page_table_size(void);

/*
 * Return the index of a page in the page table.
 */
unsigned long vm_page_table_index(phys_addr_t pa);

/*
 * Return the total amount of physical memory.
 */
phys_addr_t vm_page_mem_size(void);

/*
 * Return the amount of free (unused) pages.
 *
 * XXX This currently relies on the kernel being non preemptible and
 * uniprocessor.
 */
unsigned long vm_page_mem_free(void);

/*
 * Remove the given page from any page queue it might be in.
 */
void vm_page_queues_remove(struct vm_page *page);

/*
 * Balance physical pages among segments.
 *
 * This function should be called first by the pageout daemon
 * on memory pressure, since it may be unnecessary to perform any
 * other operation, let alone shrink caches, if balancing is
 * enough to make enough free pages.
 *
 * Return TRUE if balancing made enough free pages for unprivileged
 * allocations to succeed, in which case pending allocations are resumed.
 *
 * This function acquires vm_page_queue_free_lock, which is held on return.
 */
boolean_t vm_page_balance(void);

/*
 * Evict physical pages.
 *
 * This function should be called by the pageout daemon after balancing
 * the segments and shrinking kernel caches.
 *
 * Return TRUE if eviction made enough free pages for unprivileged
 * allocations to succeed, in which case pending allocations are resumed.
 *
 * Otherwise, report whether the pageout daemon should wait (some pages
 * have been paged out) or not (only clean pages have been released).
 *
 * This function acquires vm_page_queue_free_lock, which is held on return.
 */
boolean_t vm_page_evict(boolean_t *should_wait);

/*
 * Turn active pages into inactive ones for second-chance LRU
 * approximation.
 *
 * This function should be called by the pageout daemon on memory pressure,
 * i.e. right before evicting pages.
 *
 * XXX This is probably not the best strategy, compared to keeping the
 * active/inactive ratio in check at all times, but this means less
 * frequent refills.
 */
void vm_page_refill_inactive(void);

/*
 * Print vmstat information
 */
void db_show_vmstat(void);

#endif	/* _VM_VM_PAGE_H_ */
