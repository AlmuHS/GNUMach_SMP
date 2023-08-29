/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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
 *	File:	vm/vm_resident.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Resident memory management module.
 */

#include <kern/printf.h>
#include <string.h>

#include <mach/vm_prot.h>
#include <kern/counters.h>
#include <kern/debug.h>
#include <kern/list.h>
#include <kern/sched_prim.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <mach/vm_statistics.h>
#include <machine/vm_param.h>
#include <kern/xpr.h>
#include <kern/slab.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern.h>
#include <vm/vm_resident.h>

#if	MACH_VM_DEBUG
#include <mach/kern_return.h>
#include <mach_debug/hash_info.h>
#include <vm/vm_user.h>
#endif

#if	MACH_KDB
#include <ddb/db_output.h>
#include <vm/vm_print.h>
#endif	/* MACH_KDB */


/*
 *	Associated with each page of user-allocatable memory is a
 *	page structure.
 */

/*
 *	These variables record the values returned by vm_page_bootstrap,
 *	for debugging purposes.  The implementation of pmap_steal_memory
 *	here also uses them internally.
 */

vm_offset_t virtual_space_start;
vm_offset_t virtual_space_end;

/*
 *	The vm_page_lookup() routine, which provides for fast
 *	(virtual memory object, offset) to page lookup, employs
 *	the following hash table.  The vm_page_{insert,remove}
 *	routines install and remove associations in the table.
 *	[This table is often called the virtual-to-physical,
 *	or VP, table.]
 */
typedef struct {
	decl_simple_lock_data(,lock)
	vm_page_t pages;
} vm_page_bucket_t;

vm_page_bucket_t *vm_page_buckets;		/* Array of buckets */
unsigned long	vm_page_bucket_count = 0;	/* How big is array? */
unsigned long	vm_page_hash_mask;		/* Mask for hash function */

static struct list	vm_page_queue_fictitious;
def_simple_lock_data(,vm_page_queue_free_lock)
int		vm_page_fictitious_count;
int		vm_object_external_count;
int		vm_object_external_pages;

/*
 *	Occasionally, the virtual memory system uses
 *	resident page structures that do not refer to
 *	real pages, for example to leave a page with
 *	important state information in the VP table.
 *
 *	These page structures are allocated the way
 *	most other kernel structures are.
 */
struct kmem_cache	vm_page_cache;

/*
 *	Fictitious pages don't have a physical address,
 *	but we must initialize phys_addr to something.
 *	For debugging, this should be a strange value
 *	that the pmap module can recognize in assertions.
 */
phys_addr_t vm_page_fictitious_addr = (phys_addr_t) -1;

/*
 *	Resident page structures are also chained on
 *	queues that are used by the page replacement
 *	system (pageout daemon).  These queues are
 *	defined here, but are shared by the pageout
 *	module.
 */
def_simple_lock_data(,vm_page_queue_lock)
int	vm_page_active_count;
int	vm_page_inactive_count;
int	vm_page_wire_count;

/*
 *	Several page replacement parameters are also
 *	shared with this module, so that page allocation
 *	(done here in vm_page_alloc) can trigger the
 *	pageout daemon.
 */
int	vm_page_laundry_count = 0;
int	vm_page_external_laundry_count = 0;


/*
 *	The VM system has a couple of heuristics for deciding
 *	that pages are "uninteresting" and should be placed
 *	on the inactive queue as likely candidates for replacement.
 *	These variables let the heuristics be controlled at run-time
 *	to make experimentation easier.
 */

boolean_t vm_page_deactivate_behind = TRUE;
boolean_t vm_page_deactivate_hint = TRUE;

/*
 *	vm_page_bootstrap:
 *
 *	Initializes the resident memory module.
 *
 *	Allocates memory for the page cells, and
 *	for the object/offset-to-page hash table headers.
 *	Each page cell is initialized and placed on the free list.
 *	Returns the range of available kernel virtual memory.
 */

void vm_page_bootstrap(
	vm_offset_t *startp,
	vm_offset_t *endp)
{
	int i;

	/*
	 *	Initialize the page queues.
	 */

	simple_lock_init(&vm_page_queue_free_lock);
	simple_lock_init(&vm_page_queue_lock);

	list_init(&vm_page_queue_fictitious);

	/*
	 *	Allocate (and initialize) the virtual-to-physical
	 *	table hash buckets.
	 *
	 *	The number of buckets should be a power of two to
	 *	get a good hash function.  The following computation
	 *	chooses the first power of two that is greater
	 *	than the number of physical pages in the system.
	 */

	if (vm_page_bucket_count == 0) {
		unsigned long npages = vm_page_table_size();

		vm_page_bucket_count = 1;
		while (vm_page_bucket_count < npages)
			vm_page_bucket_count <<= 1;
	}

	vm_page_hash_mask = vm_page_bucket_count - 1;

	if (vm_page_hash_mask & vm_page_bucket_count)
		printf("vm_page_bootstrap: WARNING -- strange page hash\n");

	vm_page_buckets = (vm_page_bucket_t *)
		pmap_steal_memory(vm_page_bucket_count *
				  sizeof(vm_page_bucket_t));

	for (i = 0; i < vm_page_bucket_count; i++) {
		vm_page_bucket_t *bucket = &vm_page_buckets[i];

		bucket->pages = VM_PAGE_NULL;
		simple_lock_init(&bucket->lock);
	}

	vm_page_setup();

	virtual_space_start = round_page(virtual_space_start);
	virtual_space_end = trunc_page(virtual_space_end);

	*startp = virtual_space_start;
	*endp = virtual_space_end;
}

#ifndef	MACHINE_PAGES
/*
 *	We implement pmap_steal_memory with the help
 *	of two simpler functions, pmap_virtual_space and vm_page_bootalloc.
 */

vm_offset_t pmap_steal_memory(
	vm_size_t size)
{
	vm_offset_t addr, vaddr;
	phys_addr_t paddr;

	size = round_page(size);

	/*
	 *	If this is the first call to pmap_steal_memory,
	 *	we have to initialize ourself.
	 */

	if (virtual_space_start == virtual_space_end) {
		pmap_virtual_space(&virtual_space_start, &virtual_space_end);

		/*
		 *	The initial values must be aligned properly, and
		 *	we don't trust the pmap module to do it right.
		 */

		virtual_space_start = round_page(virtual_space_start);
		virtual_space_end = trunc_page(virtual_space_end);
	}

	/*
	 *	Allocate virtual memory for this request.
	 */

	addr = virtual_space_start;
	virtual_space_start += size;

	/*
	 *	Allocate and map physical pages to back new virtual pages.
	 */

	for (vaddr = round_page(addr);
	     vaddr < addr + size;
	     vaddr += PAGE_SIZE) {
		paddr = vm_page_bootalloc(PAGE_SIZE);

		/*
		 *	XXX Logically, these mappings should be wired,
		 *	but some pmap modules barf if they are.
		 */

		pmap_enter(kernel_pmap, vaddr, paddr,
			   VM_PROT_READ|VM_PROT_WRITE, FALSE);
	}

	return addr;
}
#endif	/* MACHINE_PAGES */

/*
 *	Routine:	vm_page_module_init
 *	Purpose:
 *		Second initialization pass, to be done after
 *		the basic VM system is ready.
 */
void		vm_page_module_init(void)
{
	kmem_cache_init(&vm_page_cache, "vm_page", sizeof(struct vm_page), 0,
			NULL, 0);
}

/*
 *	vm_page_hash:
 *
 *	Distributes the object/offset key pair among hash buckets.
 *
 *	NOTE:	To get a good hash function, the bucket count should
 *		be a power of two.
 */
#define vm_page_hash(object, offset) \
	(((unsigned int)(vm_offset_t)object + (unsigned int)atop(offset)) \
		& vm_page_hash_mask)

/*
 *	vm_page_insert:		[ internal use only ]
 *
 *	Inserts the given mem entry into the object/object-page
 *	table and object list.
 *
 *	The object and page must be locked.
 *	The free page queue must not be locked.
 */

void vm_page_insert(
	vm_page_t	mem,
	vm_object_t	object,
	vm_offset_t	offset)
{
	vm_page_bucket_t *bucket;

	VM_PAGE_CHECK(mem);

	assert(!mem->active && !mem->inactive);
	assert(!mem->external);

	if (!object->internal) {
		mem->external = TRUE;
		vm_object_external_pages++;
	}

	if (mem->tabled)
		panic("vm_page_insert");

	/*
	 *	Record the object/offset pair in this page
	 */

	mem->object = object;
	mem->offset = offset;

	/*
	 *	Insert it into the object_object/offset hash table
	 */

	bucket = &vm_page_buckets[vm_page_hash(object, offset)];
	simple_lock(&bucket->lock);
	mem->next = bucket->pages;
	bucket->pages = mem;
	simple_unlock(&bucket->lock);

	/*
	 *	Now link into the object's list of backed pages.
	 */

	queue_enter(&object->memq, mem, vm_page_t, listq);
	mem->tabled = TRUE;

	/*
	 *	Show that the object has one more resident page.
	 */

	object->resident_page_count++;
	assert(object->resident_page_count != 0);

	/*
	 *	Detect sequential access and inactivate previous page.
	 *	We ignore busy pages.
	 */

	if (vm_page_deactivate_behind &&
	    (offset == object->last_alloc + PAGE_SIZE)) {
		vm_page_t	last_mem;

		last_mem = vm_page_lookup(object, object->last_alloc);
		if ((last_mem != VM_PAGE_NULL) && !last_mem->busy)
			vm_page_deactivate(last_mem);
	}
	object->last_alloc = offset;
}

/*
 *	vm_page_replace:
 *
 *	Exactly like vm_page_insert, except that we first
 *	remove any existing page at the given offset in object
 *	and we don't do deactivate-behind.
 *
 *	The object and page must be locked.
 *	The free page queue must not be locked.
 */

void vm_page_replace(
	vm_page_t	mem,
	vm_object_t	object,
	vm_offset_t	offset)
{
	vm_page_bucket_t *bucket;

	VM_PAGE_CHECK(mem);

	assert(!mem->active && !mem->inactive);
	assert(!mem->external);

	if (!object->internal) {
		mem->external = TRUE;
		vm_object_external_pages++;
	}

	if (mem->tabled)
		panic("vm_page_replace");

	/*
	 *	Record the object/offset pair in this page
	 */

	mem->object = object;
	mem->offset = offset;

	/*
	 *	Insert it into the object_object/offset hash table,
	 *	replacing any page that might have been there.
	 */

	bucket = &vm_page_buckets[vm_page_hash(object, offset)];
	simple_lock(&bucket->lock);
	if (bucket->pages) {
		vm_page_t *mp = &bucket->pages;
		vm_page_t m = *mp;
		do {
			if (m->object == object && m->offset == offset) {
				/*
				 * Remove page from bucket and from object,
				 * and return it to the free list.
				 */
				*mp = m->next;
				queue_remove(&object->memq, m, vm_page_t,
					     listq);
				m->tabled = FALSE;
				object->resident_page_count--;
				VM_PAGE_QUEUES_REMOVE(m);

				if (m->external) {
					m->external = FALSE;
					vm_object_external_pages--;
				}

				/*
				 * Return page to the free list.
				 * Note the page is not tabled now, so this
				 * won't self-deadlock on the bucket lock.
				 */

				vm_page_free(m);
				break;
			}
			mp = &m->next;
		} while ((m = *mp) != 0);
		mem->next = bucket->pages;
	} else {
		mem->next = VM_PAGE_NULL;
	}
	bucket->pages = mem;
	simple_unlock(&bucket->lock);

	/*
	 *	Now link into the object's list of backed pages.
	 */

	queue_enter(&object->memq, mem, vm_page_t, listq);
	mem->tabled = TRUE;

	/*
	 *	And show that the object has one more resident
	 *	page.
	 */

	object->resident_page_count++;
	assert(object->resident_page_count != 0);
}

/*
 *	vm_page_remove:		[ internal use only ]
 *
 *	Removes the given mem entry from the object/offset-page
 *	table, the object page list, and the page queues.
 *
 *	The object and page must be locked.
 *	The free page queue must not be locked.
 */

void vm_page_remove(
	vm_page_t		mem)
{
	vm_page_bucket_t	*bucket;
	vm_page_t		this;

	assert(mem->tabled);
	VM_PAGE_CHECK(mem);

	/*
	 *	Remove from the object_object/offset hash table
	 */

	bucket = &vm_page_buckets[vm_page_hash(mem->object, mem->offset)];
	simple_lock(&bucket->lock);
	if ((this = bucket->pages) == mem) {
		/* optimize for common case */

		bucket->pages = mem->next;
	} else {
		vm_page_t	*prev;

		for (prev = &this->next;
		     (this = *prev) != mem;
		     prev = &this->next)
			continue;
		*prev = this->next;
	}
	simple_unlock(&bucket->lock);

	/*
	 *	Now remove from the object's list of backed pages.
	 */

	queue_remove(&mem->object->memq, mem, vm_page_t, listq);

	/*
	 *	And show that the object has one fewer resident
	 *	page.
	 */

	mem->object->resident_page_count--;

	mem->tabled = FALSE;

	VM_PAGE_QUEUES_REMOVE(mem);

	if (mem->external) {
		mem->external = FALSE;
		vm_object_external_pages--;
	}
}

/*
 *	vm_page_lookup:
 *
 *	Returns the page associated with the object/offset
 *	pair specified; if none is found, VM_PAGE_NULL is returned.
 *
 *	The object must be locked.  No side effects.
 */

vm_page_t vm_page_lookup(
	vm_object_t		object,
	vm_offset_t		offset)
{
	vm_page_t		mem;
	vm_page_bucket_t 	*bucket;

	/*
	 *	Search the hash table for this object/offset pair
	 */

	bucket = &vm_page_buckets[vm_page_hash(object, offset)];

	simple_lock(&bucket->lock);
	for (mem = bucket->pages; mem != VM_PAGE_NULL; mem = mem->next) {
		VM_PAGE_CHECK(mem);
		if ((mem->object == object) && (mem->offset == offset))
			break;
	}
	simple_unlock(&bucket->lock);
	return mem;
}

/*
 *	vm_page_rename:
 *
 *	Move the given memory entry from its
 *	current object to the specified target object/offset.
 *
 *	The object must be locked.
 */
void vm_page_rename(
	vm_page_t	mem,
	vm_object_t	new_object,
	vm_offset_t	new_offset)
{
	/*
	 *	Changes to mem->object require the page lock because
	 *	the pageout daemon uses that lock to get the object.
	 */

	vm_page_lock_queues();
    	vm_page_remove(mem);
	vm_page_insert(mem, new_object, new_offset);
	vm_page_unlock_queues();
}

static void vm_page_init_template(vm_page_t m)
{
	m->object = VM_OBJECT_NULL;	/* reset later */
	m->offset = 0;			/* reset later */
	m->wire_count = 0;

	m->inactive = FALSE;
	m->active = FALSE;
	m->laundry = FALSE;
	m->external_laundry = FALSE;
	m->free = FALSE;
	m->external = FALSE;

	m->busy = TRUE;
	m->wanted = FALSE;
	m->tabled = FALSE;
	m->fictitious = FALSE;
	m->private = FALSE;
	m->absent = FALSE;
	m->error = FALSE;
	m->dirty = FALSE;
	m->precious = FALSE;
	m->reference = FALSE;

	m->page_lock = VM_PROT_NONE;
	m->unlock_request = VM_PROT_NONE;
}

/*
 *	vm_page_init:
 *
 *	Initialize the fields in a new page.
 *	This takes a structure with random values and initializes it
 *	so that it can be given to vm_page_release or vm_page_insert.
 */
void vm_page_init(
	vm_page_t	mem)
{
	vm_page_init_template(mem);
}

/*
 *	vm_page_grab_fictitious:
 *
 *	Remove a fictitious page from the free list.
 *	Returns VM_PAGE_NULL if there are no free pages.
 */

vm_page_t vm_page_grab_fictitious(void)
{
	vm_page_t m;

	simple_lock(&vm_page_queue_free_lock);
	if (list_empty(&vm_page_queue_fictitious)) {
		m = VM_PAGE_NULL;
	} else {
		m = list_first_entry(&vm_page_queue_fictitious,
				     struct vm_page, node);
		assert(m->fictitious);
		list_remove(&m->node);
		m->free = FALSE;
		vm_page_fictitious_count--;
	}
	simple_unlock(&vm_page_queue_free_lock);

	return m;
}

/*
 *	vm_page_release_fictitious:
 *
 *	Release a fictitious page to the free list.
 */

static void vm_page_release_fictitious(
	vm_page_t m)
{
	simple_lock(&vm_page_queue_free_lock);
	if (m->free)
		panic("vm_page_release_fictitious");
	m->free = TRUE;
	list_insert_head(&vm_page_queue_fictitious, &m->node);
	vm_page_fictitious_count++;
	simple_unlock(&vm_page_queue_free_lock);
}

/*
 *	vm_page_more_fictitious:
 *
 *	Add more fictitious pages to the free list.
 *	Allowed to block.
 */

int vm_page_fictitious_quantum = 5;

void vm_page_more_fictitious(void)
{
	vm_page_t m;
	int i;

	for (i = 0; i < vm_page_fictitious_quantum; i++) {
		m = (vm_page_t) kmem_cache_alloc(&vm_page_cache);
		if (m == VM_PAGE_NULL)
			panic("vm_page_more_fictitious");

		vm_page_init(m);
		m->phys_addr = vm_page_fictitious_addr;
		m->fictitious = TRUE;
		vm_page_release_fictitious(m);
	}
}

/*
 *	vm_page_convert:
 *
 *	Attempt to convert a fictitious page into a real page.
 *
 *	The object referenced by *MP must be locked.
 */

boolean_t vm_page_convert(struct vm_page **mp)
{
	struct vm_page *real_m, *fict_m;
	vm_object_t object;
	vm_offset_t offset;

	fict_m = *mp;

	assert(fict_m->fictitious);
	assert(fict_m->phys_addr == vm_page_fictitious_addr);
	assert(!fict_m->active);
	assert(!fict_m->inactive);

	real_m = vm_page_grab(VM_PAGE_HIGHMEM);
	if (real_m == VM_PAGE_NULL)
		return FALSE;

	object = fict_m->object;
	offset = fict_m->offset;
	vm_page_remove(fict_m);

	memcpy(&real_m->vm_page_header,
	       &fict_m->vm_page_header,
	       VM_PAGE_BODY_SIZE);
	real_m->fictitious = FALSE;

	vm_page_insert(real_m, object, offset);

	assert(real_m->phys_addr != vm_page_fictitious_addr);
	assert(fict_m->fictitious);
	assert(fict_m->phys_addr == vm_page_fictitious_addr);

	vm_page_release_fictitious(fict_m);
	*mp = real_m;
	return TRUE;
}

/*
 *	vm_page_grab:
 *
 *	Remove a page from the free list.
 *	Returns VM_PAGE_NULL if the free list is too small.
 *
 *	FLAGS specify which constraint should be enforced for the allocated
 *	addresses.
 */

vm_page_t vm_page_grab(unsigned flags)
{
	unsigned selector;
	vm_page_t	mem;

	if (flags & VM_PAGE_HIGHMEM)
		selector = VM_PAGE_SEL_HIGHMEM;
#if defined(VM_PAGE_DMA32_LIMIT) && VM_PAGE_DMA32_LIMIT > VM_PAGE_DIRECTMAP_LIMIT
       else if (flags & VM_PAGE_DMA32)
               selector = VM_PAGE_SEL_DMA32;
#endif
	else if (flags & VM_PAGE_DIRECTMAP)
		selector = VM_PAGE_SEL_DIRECTMAP;
#if defined(VM_PAGE_DMA32_LIMIT) && VM_PAGE_DMA32_LIMIT <= VM_PAGE_DIRECTMAP_LIMIT
	else if (flags & VM_PAGE_DMA32)
		selector = VM_PAGE_SEL_DMA32;
#endif
	else
		selector = VM_PAGE_SEL_DMA;

	simple_lock(&vm_page_queue_free_lock);

	/*
	 * XXX Mach has many modules that merely assume memory is
	 * directly mapped in kernel space. Instead of updating all
	 * users, we assume those which need specific physical memory
	 * properties will wire down their pages, either because
	 * they can't be paged (not part of an object), or with
	 * explicit VM calls. The strategy is then to let memory
	 * pressure balance the physical segments with pageable pages.
	 */
	mem = vm_page_alloc_pa(0, selector, VM_PT_KERNEL);

	if (mem == NULL) {
		simple_unlock(&vm_page_queue_free_lock);
		return NULL;
	}

	mem->free = FALSE;
	simple_unlock(&vm_page_queue_free_lock);

	return mem;
}

phys_addr_t vm_page_grab_phys_addr(void)
{
	vm_page_t p = vm_page_grab(VM_PAGE_DIRECTMAP);
	if (p == VM_PAGE_NULL)
		return -1;
	else
		return p->phys_addr;
}

/*
 *	vm_page_release:
 *
 *	Return a page to the free list.
 */

void vm_page_release(
	vm_page_t	mem,
	boolean_t 	laundry,
	boolean_t 	external_laundry)
{
	simple_lock(&vm_page_queue_free_lock);
	if (mem->free)
		panic("vm_page_release");
	mem->free = TRUE;
	vm_page_free_pa(mem, 0);
	if (laundry) {
		vm_page_laundry_count--;

		if (vm_page_laundry_count == 0) {
			vm_pageout_resume();
		}
	}
	if (external_laundry) {

		/*
		 *	If vm_page_external_laundry_count is negative,
		 *	the pageout daemon isn't expecting to be
		 *	notified.
		 */

		if (vm_page_external_laundry_count > 0) {
			vm_page_external_laundry_count--;

			if (vm_page_external_laundry_count == 0) {
				vm_pageout_resume();
			}
		}
	}

	simple_unlock(&vm_page_queue_free_lock);
}

/*
 *	vm_page_grab_contig:
 *
 *	Remove a block of contiguous pages from the free list.
 *	Returns VM_PAGE_NULL if the request fails.
 */

vm_page_t vm_page_grab_contig(
	vm_size_t size,
	unsigned int selector)
{
	unsigned int i, order, nr_pages;
	vm_page_t mem;

	order = vm_page_order(size);
	nr_pages = 1 << order;

	simple_lock(&vm_page_queue_free_lock);

	/* TODO Allow caller to pass type */
	mem = vm_page_alloc_pa(order, selector, VM_PT_KERNEL);

	if (mem == NULL) {
		simple_unlock(&vm_page_queue_free_lock);
		return NULL;
	}

	for (i = 0; i < nr_pages; i++) {
		mem[i].free = FALSE;
	}

	simple_unlock(&vm_page_queue_free_lock);

	return mem;
}

/*
 *	vm_page_free_contig:
 *
 *	Return a block of contiguous pages to the free list.
 */

void vm_page_free_contig(vm_page_t mem, vm_size_t size)
{
	unsigned int i, order, nr_pages;

	order = vm_page_order(size);
	nr_pages = 1 << order;

	simple_lock(&vm_page_queue_free_lock);

	for (i = 0; i < nr_pages; i++) {
		if (mem[i].free)
			panic("vm_page_free_contig");

		mem[i].free = TRUE;
	}

	vm_page_free_pa(mem, order);

	simple_unlock(&vm_page_queue_free_lock);
}

/*
 *	vm_page_alloc:
 *
 *	Allocate and return a memory cell associated
 *	with this VM object/offset pair.
 *
 *	Object must be locked.
 */

vm_page_t vm_page_alloc(
	vm_object_t	object,
	vm_offset_t	offset)
{
	vm_page_t	mem;

	mem = vm_page_grab(VM_PAGE_HIGHMEM);
	if (mem == VM_PAGE_NULL)
		return VM_PAGE_NULL;

	vm_page_lock_queues();
	vm_page_insert(mem, object, offset);
	vm_page_unlock_queues();

	return mem;
}

/*
 *	vm_page_free:
 *
 *	Returns the given page to the free list,
 *	disassociating it with any VM object.
 *
 *	Object and page queues must be locked prior to entry.
 */
void vm_page_free(
	vm_page_t	mem)
{
	if (mem->free)
		panic("vm_page_free");

	if (mem->tabled) {
		vm_page_remove(mem);
	}

	assert(!mem->active && !mem->inactive);

	if (mem->wire_count != 0) {
		if (!mem->private && !mem->fictitious)
			vm_page_wire_count--;
		mem->wire_count = 0;
	}

	PAGE_WAKEUP_DONE(mem);

	if (mem->absent)
		vm_object_absent_release(mem->object);

	/*
	 *	XXX The calls to vm_page_init here are
	 *	really overkill.
	 */

	if (mem->private || mem->fictitious) {
		vm_page_init(mem);
		mem->phys_addr = vm_page_fictitious_addr;
		mem->fictitious = TRUE;
		vm_page_release_fictitious(mem);
	} else {
		boolean_t laundry = mem->laundry;
		boolean_t external_laundry = mem->external_laundry;
		vm_page_init(mem);
		vm_page_release(mem, laundry, external_laundry);
	}
}

/*
 *	vm_page_zero_fill:
 *
 *	Zero-fill the specified page.
 */
void vm_page_zero_fill(
	vm_page_t	m)
{
	VM_PAGE_CHECK(m);

	pmap_zero_page(m->phys_addr);
}

/*
 *	vm_page_copy:
 *
 *	Copy one page to another
 */

void vm_page_copy(
	vm_page_t	src_m,
	vm_page_t	dest_m)
{
	VM_PAGE_CHECK(src_m);
	VM_PAGE_CHECK(dest_m);

	pmap_copy_page(src_m->phys_addr, dest_m->phys_addr);
}

#if	MACH_VM_DEBUG
/*
 *	Routine:	vm_page_info
 *	Purpose:
 *		Return information about the global VP table.
 *		Fills the buffer with as much information as possible
 *		and returns the desired size of the buffer.
 *	Conditions:
 *		Nothing locked.  The caller should provide
 *		possibly-pageable memory.
 */

unsigned int
vm_page_info(
	hash_info_bucket_t *info,
	unsigned int	count)
{
	int i;

	if (vm_page_bucket_count < count)
		count = vm_page_bucket_count;

	for (i = 0; i < count; i++) {
		vm_page_bucket_t *bucket = &vm_page_buckets[i];
		unsigned int bucket_count = 0;
		vm_page_t m;

		simple_lock(&bucket->lock);
		for (m = bucket->pages; m != VM_PAGE_NULL; m = m->next)
			bucket_count++;
		simple_unlock(&bucket->lock);

		/* don't touch pageable memory while holding locks */
		info[i].hib_count = bucket_count;
	}

	return vm_page_bucket_count;
}
#endif	/* MACH_VM_DEBUG */


#if	MACH_KDB
#define	printf	kdbprintf

/*
 *	Routine:	vm_page_print [exported]
 */
void		vm_page_print(const vm_page_t	p)
{
	iprintf("Page 0x%X: object 0x%X,", (vm_offset_t) p, (vm_offset_t) p->object);
	 printf(" offset 0x%X", p->offset);
	 printf("wire_count %d,", p->wire_count);
	 printf(" %s",
		(p->active ? "active" : (p->inactive ? "inactive" : "loose")));
	 printf("%s",
		(p->free ? " free" : ""));
	 printf("%s ",
		(p->laundry ? " laundry" : ""));
	 printf("%s",
		(p->dirty ? "dirty" : "clean"));
	 printf("%s",
	 	(p->busy ? " busy" : ""));
	 printf("%s",
	 	(p->absent ? " absent" : ""));
	 printf("%s",
	 	(p->error ? " error" : ""));
	 printf("%s",
		(p->fictitious ? " fictitious" : ""));
	 printf("%s",
		(p->private ? " private" : ""));
	 printf("%s",
		(p->wanted ? " wanted" : ""));
	 printf("%s,",
		(p->tabled ? "" : "not_tabled"));
	 printf("phys_addr = 0x%X, lock = 0x%X, unlock_request = 0x%X\n",
		p->phys_addr,
		(vm_offset_t) p->page_lock,
		(vm_offset_t) p->unlock_request);
}
#endif	/* MACH_KDB */
