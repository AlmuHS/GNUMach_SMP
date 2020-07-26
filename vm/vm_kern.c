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
 *	File:	vm/vm_kern.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Kernel memory management.
 */

#include <string.h>

#include <mach/kern_return.h>
#include <machine/locore.h>
#include <machine/vm_param.h>
#include <kern/assert.h>
#include <kern/debug.h>
#include <kern/lock.h>
#include <kern/slab.h>
#include <kern/thread.h>
#include <kern/printf.h>
#include <vm/pmap.h>
#include <vm/vm_fault.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>



/*
 *	Variables exported by this module.
 */

static struct vm_map	kernel_map_store;
vm_map_t		kernel_map = &kernel_map_store;
vm_map_t	kernel_pageable_map;

/*
 *	projected_buffer_allocate
 *
 *	Allocate a wired-down buffer shared between kernel and user task.  
 *      Fresh, zero-filled memory is allocated.
 *      If persistence is false, this buffer can only be deallocated from
 *      user task using projected_buffer_deallocate, and deallocation 
 *      from user task also deallocates the buffer from the kernel map.
 *      projected_buffer_collect is called from vm_map_deallocate to
 *      automatically deallocate projected buffers on task_deallocate.
 *      Sharing with more than one user task is achieved by using 
 *      projected_buffer_map for the second and subsequent tasks.
 *      The user is precluded from manipulating the VM entry of this buffer
 *      (i.e. changing protection, inheritance or machine attributes).
 */

kern_return_t
projected_buffer_allocate(
	vm_map_t 	map,
	vm_size_t 	size,
       int 		persistence,
	vm_offset_t 	*kernel_p,
	vm_offset_t 	*user_p,
       vm_prot_t 	protection,
       vm_inherit_t 	inheritance)  /*Currently only VM_INHERIT_NONE supported*/
{
	vm_object_t object;
	vm_map_entry_t u_entry, k_entry;
	vm_offset_t addr;
	vm_size_t r_size;
	kern_return_t kr;

	if (map == VM_MAP_NULL || map == kernel_map)
	  return(KERN_INVALID_ARGUMENT);

	/*
	 *	Allocate a new object. 
	 */

	size = round_page(size);
	object = vm_object_allocate(size);

	vm_map_lock(kernel_map);
	kr = vm_map_find_entry(kernel_map, &addr, size, (vm_offset_t) 0,
			       VM_OBJECT_NULL, &k_entry);
	if (kr != KERN_SUCCESS) {
	  vm_map_unlock(kernel_map);
	  vm_object_deallocate(object);
	  return kr;
	}

	k_entry->object.vm_object = object;
	if (!persistence)
	  k_entry->projected_on = (vm_map_entry_t) -1;
              /*Mark entry so as to automatically deallocate it when
                last corresponding user entry is deallocated*/
	vm_map_unlock(kernel_map);
	*kernel_p = addr;

	vm_map_lock(map);
	kr = vm_map_find_entry(map, &addr, size, (vm_offset_t) 0,
			       VM_OBJECT_NULL, &u_entry);
	if (kr != KERN_SUCCESS) {
	  vm_map_unlock(map);
	  vm_map_lock(kernel_map);
	  vm_map_entry_delete(kernel_map, k_entry);
	  vm_map_unlock(kernel_map);
	  vm_object_deallocate(object);
	  return kr;
	}

	u_entry->object.vm_object = object;
	vm_object_reference(object);
	u_entry->projected_on = k_entry;
             /*Creates coupling with kernel mapping of the buffer, and
               also guarantees that user cannot directly manipulate
               buffer VM entry*/
	u_entry->protection = protection;
	u_entry->max_protection = protection;
	u_entry->inheritance = inheritance;
	vm_map_unlock(map);
       	*user_p = addr;

	/*
	 *	Allocate wired-down memory in the object,
	 *	and enter it in the kernel pmap.
	 */
	kmem_alloc_pages(object, 0,
			 *kernel_p, *kernel_p + size,
			 VM_PROT_READ | VM_PROT_WRITE);
	memset((void*) *kernel_p, 0, size);         /*Zero fill*/

	/* Set up physical mappings for user pmap */

	pmap_pageable(map->pmap, *user_p, *user_p + size, FALSE);
	for (r_size = 0; r_size < size; r_size += PAGE_SIZE) {
	  addr = pmap_extract(kernel_pmap, *kernel_p + r_size);
	  pmap_enter(map->pmap, *user_p + r_size, addr,
		     protection, TRUE);
	}

	return(KERN_SUCCESS);
}


/*
 *	projected_buffer_map
 *
 *	Map an area of kernel memory onto a task's address space.
 *      No new memory is allocated; the area must previously exist in the
 *      kernel memory map.
 */

kern_return_t
projected_buffer_map(
	vm_map_t 	map,
	vm_offset_t 	kernel_addr,
	vm_size_t 	size,
	vm_offset_t 	*user_p,
       vm_prot_t 	protection,
       vm_inherit_t 	inheritance)  /*Currently only VM_INHERIT_NONE supported*/
{
	vm_map_entry_t u_entry, k_entry;
	vm_offset_t physical_addr, user_addr;
	vm_size_t r_size;
	kern_return_t kr;

	/*
	 *	Find entry in kernel map 
	 */

	size = round_page(size);
	if (map == VM_MAP_NULL || map == kernel_map ||
	    !vm_map_lookup_entry(kernel_map, kernel_addr, &k_entry) ||
	    kernel_addr + size > k_entry->vme_end)
	  return(KERN_INVALID_ARGUMENT);


	/*
         *     Create entry in user task
         */

	vm_map_lock(map);
	kr = vm_map_find_entry(map, &user_addr, size, (vm_offset_t) 0,
			       VM_OBJECT_NULL, &u_entry);
	if (kr != KERN_SUCCESS) {
	  vm_map_unlock(map);
	  return kr;
	}

	u_entry->object.vm_object = k_entry->object.vm_object;
	vm_object_reference(k_entry->object.vm_object);
	u_entry->offset = kernel_addr - k_entry->vme_start + k_entry->offset;
	u_entry->projected_on = k_entry;
             /*Creates coupling with kernel mapping of the buffer, and
               also guarantees that user cannot directly manipulate
               buffer VM entry*/
	u_entry->protection = protection;
	u_entry->max_protection = protection;
	u_entry->inheritance = inheritance;
	u_entry->wired_count = k_entry->wired_count;
	vm_map_unlock(map);
       	*user_p = user_addr;

	/* Set up physical mappings for user pmap */

	pmap_pageable(map->pmap, user_addr, user_addr + size,
		      !k_entry->wired_count);
	for (r_size = 0; r_size < size; r_size += PAGE_SIZE) {
	  physical_addr = pmap_extract(kernel_pmap, kernel_addr + r_size);
	  pmap_enter(map->pmap, user_addr + r_size, physical_addr,
		     protection, k_entry->wired_count);
	}

	return(KERN_SUCCESS);
}


/*
 *	projected_buffer_deallocate
 *
 *	Unmap projected buffer from task's address space.
 *      May also unmap buffer from kernel map, if buffer is not
 *      persistent and only the kernel reference remains.
 */

kern_return_t
projected_buffer_deallocate(
     vm_map_t 		map,
     vm_offset_t 	start, 
     vm_offset_t	end)
{
	vm_map_entry_t entry, k_entry;

	if (map == VM_MAP_NULL || map == kernel_map)
		return KERN_INVALID_ARGUMENT;

	vm_map_lock(map);
	if (!vm_map_lookup_entry(map, start, &entry) ||
	    end > entry->vme_end ||
            /*Check corresponding kernel entry*/
	    (k_entry = entry->projected_on) == 0) {
	  vm_map_unlock(map);
	  return(KERN_INVALID_ARGUMENT);
	}

	/*Prepare for deallocation*/
	if (entry->vme_start < start)
	  _vm_map_clip_start(&map->hdr, entry, start);
	if (entry->vme_end > end)
	  _vm_map_clip_end(&map->hdr, entry, end);
      	if (map->first_free == entry)   /*Adjust first_free hint*/
	  map->first_free = entry->vme_prev;
	entry->projected_on = 0;        /*Needed to allow deletion*/
	entry->wired_count = 0;         /*Avoid unwire fault*/
	vm_map_entry_delete(map, entry);
	vm_map_unlock(map);

	/*Check if the buffer is not persistent and only the 
          kernel mapping remains, and if so delete it*/
	vm_map_lock(kernel_map);
	if (k_entry->projected_on == (vm_map_entry_t) -1 &&
	    k_entry->object.vm_object->ref_count == 1) {
	  if (kernel_map->first_free == k_entry)
	    kernel_map->first_free = k_entry->vme_prev;
	  k_entry->projected_on = 0;    /*Allow unwire fault*/
	  vm_map_entry_delete(kernel_map, k_entry);
	}
	vm_map_unlock(kernel_map);
	return(KERN_SUCCESS);
}


/*
 *	projected_buffer_collect
 *
 *	Unmap all projected buffers from task's address space.
 */

kern_return_t
projected_buffer_collect(vm_map_t map)
{
        vm_map_entry_t entry, next;

        if (map == VM_MAP_NULL || map == kernel_map)
	  return(KERN_INVALID_ARGUMENT);

	for (entry = vm_map_first_entry(map);
	     entry != vm_map_to_entry(map);
	     entry = next) {
	  next = entry->vme_next;
	  if (entry->projected_on != 0)
	    projected_buffer_deallocate(map, entry->vme_start, entry->vme_end);
	}
	return(KERN_SUCCESS);
}


/*
 *	projected_buffer_in_range
 *
 *	Verifies whether a projected buffer exists in the address range 
 *      given.
 */

boolean_t
projected_buffer_in_range(
       vm_map_t 	map,
       vm_offset_t 	start, 
	vm_offset_t	end)
{
        vm_map_entry_t entry;

        if (map == VM_MAP_NULL || map == kernel_map)
	  return(FALSE);

	/*Find first entry*/
	if (!vm_map_lookup_entry(map, start, &entry))
	  entry = entry->vme_next;

	while (entry != vm_map_to_entry(map) && entry->projected_on == 0 &&
	       entry->vme_start <= end) {
	  entry = entry->vme_next;
	}
	return(entry != vm_map_to_entry(map) && entry->vme_start <= end);
}


/*
 *	kmem_alloc:
 *
 *	Allocate wired-down memory in the kernel's address map
 *	or a submap.  The memory is not zero-filled.
 */

kern_return_t
kmem_alloc(
	vm_map_t 	map,
	vm_offset_t 	*addrp,
	vm_size_t 	size)
{
	vm_object_t object;
	vm_map_entry_t entry;
	vm_offset_t addr;
	unsigned int attempts;
	kern_return_t kr;

	/*
	 *	Allocate a new object.  We must do this before locking
	 *	the map, lest we risk deadlock with the default pager:
	 *		device_read_alloc uses kmem_alloc,
	 *		which tries to allocate an object,
	 *		which uses kmem_alloc_wired to get memory,
	 *		which blocks for pages.
	 *		then the default pager needs to read a block
	 *		to process a memory_object_data_write,
	 *		and device_read_alloc calls kmem_alloc
	 *		and deadlocks on the map lock.
	 */

	size = round_page(size);
	object = vm_object_allocate(size);

	attempts = 0;

retry:
	vm_map_lock(map);
	kr = vm_map_find_entry(map, &addr, size, (vm_offset_t) 0,
			       VM_OBJECT_NULL, &entry);
	if (kr != KERN_SUCCESS) {
		vm_map_unlock(map);

		if (attempts == 0) {
			attempts++;
			slab_collect();
			goto retry;
		}

		printf_once("no more room for kmem_alloc in %p (%s)\n",
			    map, map->name);
		vm_object_deallocate(object);
		return kr;
	}

	entry->object.vm_object = object;
	entry->offset = 0;

	/*
	 *	Since we have not given out this address yet,
	 *	it is safe to unlock the map.
	 */
	vm_map_unlock(map);

	/*
	 *	Allocate wired-down memory in the kernel_object,
	 *	for this entry, and enter it in the kernel pmap.
	 */
	kmem_alloc_pages(object, 0,
			 addr, addr + size,
			 VM_PROT_DEFAULT);

	/*
	 *	Return the memory, not zeroed.
	 */
	*addrp = addr;
	return KERN_SUCCESS;
}

/*
 *	kmem_alloc_wired:
 *
 *	Allocate wired-down memory in the kernel's address map
 *	or a submap.  The memory is not zero-filled.
 *
 *	The memory is allocated in the kernel_object.
 *	It may not be copied with vm_map_copy.
 */

kern_return_t
kmem_alloc_wired(
	vm_map_t 	map,
	vm_offset_t 	*addrp,
	vm_size_t 	size)
{
	vm_map_entry_t entry;
	vm_offset_t offset;
	vm_offset_t addr;
	unsigned int attempts;
	kern_return_t kr;

	/*
	 *	Use the kernel object for wired-down kernel pages.
	 *	Assume that no region of the kernel object is
	 *	referenced more than once.  We want vm_map_find_entry
	 *	to extend an existing entry if possible.
	 */

	size = round_page(size);
	attempts = 0;

retry:
	vm_map_lock(map);
	kr = vm_map_find_entry(map, &addr, size, (vm_offset_t) 0,
			       kernel_object, &entry);
	if (kr != KERN_SUCCESS) {
		vm_map_unlock(map);

		if (attempts == 0) {
			attempts++;
			slab_collect();
			goto retry;
		}

		printf_once("no more room for kmem_alloc_wired in %p (%s)\n",
			    map, map->name);
		return kr;
	}

	/*
	 *	Since we didn't know where the new region would
	 *	start, we couldn't supply the correct offset into
	 *	the kernel object.  We only initialize the entry
	 *	if we aren't extending an existing entry.
	 */

	offset = addr - VM_MIN_KERNEL_ADDRESS;

	if (entry->object.vm_object == VM_OBJECT_NULL) {
		vm_object_reference(kernel_object);

		entry->object.vm_object = kernel_object;
		entry->offset = offset;
	}

	/*
	 *	Since we have not given out this address yet,
	 *	it is safe to unlock the map.
	 */
	vm_map_unlock(map);

	/*
	 *	Allocate wired-down memory in the kernel_object,
	 *	for this entry, and enter it in the kernel pmap.
	 */
	kmem_alloc_pages(kernel_object, offset,
			 addr, addr + size,
			 VM_PROT_DEFAULT);

	/*
	 *	Return the memory, not zeroed.
	 */
	*addrp = addr;
	return KERN_SUCCESS;
}

/*
 *	kmem_alloc_aligned:
 *
 *	Like kmem_alloc_wired, except that the memory is aligned.
 *	The size should be a power-of-2.
 */

kern_return_t
kmem_alloc_aligned(
	vm_map_t 	map,
	vm_offset_t 	*addrp,
	vm_size_t 	size)
{
	vm_map_entry_t entry;
	vm_offset_t offset;
	vm_offset_t addr;
	unsigned int attempts;
	kern_return_t kr;

	if ((size & (size - 1)) != 0)
		panic("kmem_alloc_aligned");

	/*
	 *	Use the kernel object for wired-down kernel pages.
	 *	Assume that no region of the kernel object is
	 *	referenced more than once.  We want vm_map_find_entry
	 *	to extend an existing entry if possible.
	 */

	size = round_page(size);
	attempts = 0;

retry:
	vm_map_lock(map);
	kr = vm_map_find_entry(map, &addr, size, size - 1,
			       kernel_object, &entry);
	if (kr != KERN_SUCCESS) {
		vm_map_unlock(map);

		if (attempts == 0) {
			attempts++;
			slab_collect();
			goto retry;
		}

		printf_once("no more room for kmem_alloc_aligned in %p (%s)\n",
			    map, map->name);
		return kr;
	}

	/*
	 *	Since we didn't know where the new region would
	 *	start, we couldn't supply the correct offset into
	 *	the kernel object.  We only initialize the entry
	 *	if we aren't extending an existing entry.
	 */

	offset = addr - VM_MIN_KERNEL_ADDRESS;

	if (entry->object.vm_object == VM_OBJECT_NULL) {
		vm_object_reference(kernel_object);

		entry->object.vm_object = kernel_object;
		entry->offset = offset;
	}

	/*
	 *	Since we have not given out this address yet,
	 *	it is safe to unlock the map.
	 */
	vm_map_unlock(map);

	/*
	 *	Allocate wired-down memory in the kernel_object,
	 *	for this entry, and enter it in the kernel pmap.
	 */
	kmem_alloc_pages(kernel_object, offset,
			 addr, addr + size,
			 VM_PROT_DEFAULT);

	/*
	 *	Return the memory, not zeroed.
	 */
	*addrp = addr;
	return KERN_SUCCESS;
}

/*
 *	kmem_alloc_pageable:
 *
 *	Allocate pageable memory in the kernel's address map.
 */

kern_return_t
kmem_alloc_pageable(
	vm_map_t 	map,
	vm_offset_t 	*addrp,
	vm_size_t 	size)
{
	vm_offset_t addr;
	kern_return_t kr;

	addr = vm_map_min(map);
	kr = vm_map_enter(map, &addr, round_page(size),
			  (vm_offset_t) 0, TRUE,
			  VM_OBJECT_NULL, (vm_offset_t) 0, FALSE,
			  VM_PROT_DEFAULT, VM_PROT_ALL, VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS) {
		printf_once("no more room for kmem_alloc_pageable in %p (%s)\n",
			    map, map->name);
		return kr;
	}

	*addrp = addr;
	return KERN_SUCCESS;
}

/*
 *	kmem_free:
 *
 *	Release a region of kernel virtual memory allocated
 *	with kmem_alloc, kmem_alloc_wired, or kmem_alloc_pageable,
 *	and return the physical pages associated with that region.
 */

void
kmem_free(
	vm_map_t 	map,
	vm_offset_t 	addr,
	vm_size_t 	size)
{
	kern_return_t kr;

	kr = vm_map_remove(map, trunc_page(addr), round_page(addr + size));
	if (kr != KERN_SUCCESS)
		panic("kmem_free");
}

/*
 *	Allocate new wired pages in an object.
 *	The object is assumed to be mapped into the kernel map or
 *	a submap.
 */
void
kmem_alloc_pages(
	vm_object_t	object,
	vm_offset_t	offset,
	vm_offset_t	start, 
	vm_offset_t	end,
	vm_prot_t	protection)
{
	/*
	 *	Mark the pmap region as not pageable.
	 */
	pmap_pageable(kernel_pmap, start, end, FALSE);

	while (start < end) {
	    vm_page_t	mem;

	    vm_object_lock(object);

	    /*
	     *	Allocate a page
	     */
	    while ((mem = vm_page_alloc(object, offset))
			 == VM_PAGE_NULL) {
		vm_object_unlock(object);
		VM_PAGE_WAIT((void (*)()) 0);
		vm_object_lock(object);
	    }

	    /*
	     *	Wire it down
	     */
	    vm_page_lock_queues();
	    vm_page_wire(mem);
	    vm_page_unlock_queues();
	    vm_object_unlock(object);

	    /*
	     *	Enter it in the kernel pmap
	     */
	    PMAP_ENTER(kernel_pmap, start, mem,
		       protection, TRUE);

	    vm_object_lock(object);
	    PAGE_WAKEUP_DONE(mem);
	    vm_object_unlock(object);

	    start += PAGE_SIZE;
	    offset += PAGE_SIZE;
	}
}

/*
 *	Remap wired pages in an object into a new region.
 *	The object is assumed to be mapped into the kernel map or
 *	a submap.
 */
void
kmem_remap_pages(
	vm_object_t	object,
	vm_offset_t	offset,
	vm_offset_t	start, 
	vm_offset_t	end,
	vm_prot_t	protection)
{
	/*
	 *	Mark the pmap region as not pageable.
	 */
	pmap_pageable(kernel_pmap, start, end, FALSE);

	while (start < end) {
	    vm_page_t	mem;

	    vm_object_lock(object);

	    /*
	     *	Find a page
	     */
	    if ((mem = vm_page_lookup(object, offset)) == VM_PAGE_NULL)
		panic("kmem_remap_pages");

	    /*
	     *	Wire it down (again)
	     */
	    vm_page_lock_queues();
	    vm_page_wire(mem);
	    vm_page_unlock_queues();
	    vm_object_unlock(object);

	    /*
	     *	Enter it in the kernel pmap.  The page isn't busy,
	     *	but this shouldn't be a problem because it is wired.
	     */
	    PMAP_ENTER(kernel_pmap, start, mem,
		       protection, TRUE);

	    start += PAGE_SIZE;
	    offset += PAGE_SIZE;
	}
}

/*
 *	kmem_submap:
 *
 *	Initializes a map to manage a subrange
 *	of the kernel virtual address space.
 *
 *	Arguments are as follows:
 *
 *	map		Map to initialize
 *	parent		Map to take range from
 *	size		Size of range to find
 *	min, max	Returned endpoints of map
 *	pageable	Can the region be paged
 */

void
kmem_submap(
	vm_map_t 	map, 
	vm_map_t 	parent,
	vm_offset_t 	*min, 
	vm_offset_t 	*max,
	vm_size_t 	size)
{
	vm_offset_t addr;
	kern_return_t kr;

	size = round_page(size);

	/*
	 *	Need reference on submap object because it is internal
	 *	to the vm_system.  vm_object_enter will never be called
	 *	on it (usual source of reference for vm_map_enter).
	 */
	vm_object_reference(vm_submap_object);

	addr = vm_map_min(parent);
	kr = vm_map_enter(parent, &addr, size,
			  (vm_offset_t) 0, TRUE,
			  vm_submap_object, (vm_offset_t) 0, FALSE,
			  VM_PROT_DEFAULT, VM_PROT_ALL, VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS)
		panic("kmem_submap");

	pmap_reference(vm_map_pmap(parent));
	vm_map_setup(map, vm_map_pmap(parent), addr, addr + size);
	kr = vm_map_submap(parent, addr, addr + size, map);
	if (kr != KERN_SUCCESS)
		panic("kmem_submap");

	*min = addr;
	*max = addr + size;
}

/*
 *	kmem_init:
 *
 *	Initialize the kernel's virtual memory map, taking
 *	into account all memory allocated up to this time.
 */
void kmem_init(
	vm_offset_t	start,
	vm_offset_t	end)
{
	vm_map_setup(kernel_map, pmap_kernel(), VM_MIN_KERNEL_ADDRESS, end);

	/*
	 *	Reserve virtual memory allocated up to this time.
	 */
	if (start != VM_MIN_KERNEL_ADDRESS) {
		kern_return_t rc;
		vm_offset_t addr = VM_MIN_KERNEL_ADDRESS;
		rc = vm_map_enter(kernel_map,
				  &addr, start - VM_MIN_KERNEL_ADDRESS,
				  (vm_offset_t) 0, TRUE,
				  VM_OBJECT_NULL, (vm_offset_t) 0, FALSE,
				  VM_PROT_DEFAULT, VM_PROT_ALL,
				  VM_INHERIT_DEFAULT);
		if (rc)
			panic("vm_map_enter failed (%d)\n", rc);
	}
}

/*
 *	New and improved IO wiring support.
 */

/*
 *	kmem_io_map_copyout:
 *
 *	Establish temporary mapping in designated map for the memory
 *	passed in.  Memory format must be a page_list vm_map_copy.
 *	Mapping is READ-ONLY.
 */

kern_return_t
kmem_io_map_copyout(
     vm_map_t 		map,
     vm_offset_t	*addr,  	/* actual addr of data */
     vm_offset_t	*alloc_addr,	/* page aligned addr */
     vm_size_t		*alloc_size,	/* size allocated */
     vm_map_copy_t	copy,
     vm_size_t		min_size)	/* Do at least this much */
{
	vm_offset_t	myaddr, offset;
	vm_size_t	mysize, copy_size;
	kern_return_t	ret;
	vm_page_t	*page_list;
	vm_map_copy_t	new_copy;
	int		i;

	assert(copy->type == VM_MAP_COPY_PAGE_LIST);
	assert(min_size != 0);

	/*
	 *	Figure out the size in vm pages.
	 */
	min_size += copy->offset - trunc_page(copy->offset);
	min_size = round_page(min_size);
	mysize = round_page(copy->offset + copy->size) -
		trunc_page(copy->offset);

	/*
	 *	If total size is larger than one page list and
	 *	we don't have to do more than one page list, then
	 *	only do one page list.  
	 *
	 * XXX	Could be much smarter about this ... like trimming length
	 * XXX	if we need more than one page list but not all of them.
	 */

	copy_size = ptoa(copy->cpy_npages);
	if (mysize > copy_size && copy_size > min_size)
		mysize = copy_size;

	/*
	 *	Allocate some address space in the map (must be kernel
	 *	space).
	 */
	myaddr = vm_map_min(map);
	ret = vm_map_enter(map, &myaddr, mysize,
			  (vm_offset_t) 0, TRUE,
			  VM_OBJECT_NULL, (vm_offset_t) 0, FALSE,
			  VM_PROT_DEFAULT, VM_PROT_ALL, VM_INHERIT_DEFAULT);

	if (ret != KERN_SUCCESS)
		return(ret);

	/*
	 *	Tell the pmap module that this will be wired, and
	 *	enter the mappings.
	 */
	pmap_pageable(vm_map_pmap(map), myaddr, myaddr + mysize, TRUE);

	*addr = myaddr + (copy->offset - trunc_page(copy->offset));
	*alloc_addr = myaddr;
	*alloc_size = mysize;

	offset = myaddr;
	page_list = &copy->cpy_page_list[0];
	while (TRUE) {
		for ( i = 0; i < copy->cpy_npages; i++, offset += PAGE_SIZE) {
			PMAP_ENTER(vm_map_pmap(map), offset, *page_list,
				   VM_PROT_READ, TRUE);
			page_list++;
		}

		if (offset == (myaddr + mysize))
			break;

		/*
		 *	Onward to the next page_list.  The extend_cont
		 *	leaves the current page list's pages alone; 
		 *	they'll be cleaned up at discard.  Reset this
		 *	copy's continuation to discard the next one.
		 */
		vm_map_copy_invoke_extend_cont(copy, &new_copy, &ret);

		if (ret != KERN_SUCCESS) {
			kmem_io_map_deallocate(map, myaddr, mysize);
			return(ret);
		}
		copy->cpy_cont = vm_map_copy_discard_cont;
		copy->cpy_cont_args = (char *) new_copy;
		copy = new_copy;
		page_list = &copy->cpy_page_list[0];
	}

	return(ret);
}

/*
 *	kmem_io_map_deallocate:
 *
 *	Get rid of the mapping established by kmem_io_map_copyout.
 *	Assumes that addr and size have been rounded to page boundaries.
 *	(e.g., the alloc_addr and alloc_size returned by kmem_io_map_copyout)
 */

void
kmem_io_map_deallocate(
	vm_map_t	map,
	vm_offset_t	addr,
	vm_size_t	size)
{
	/*
	 *	Remove the mappings.  The pmap_remove is needed.
	 */
	
	pmap_remove(vm_map_pmap(map), addr, addr + size);
	vm_map_remove(map, addr, addr + size);
}

/*
 *	Routine:	copyinmap
 *	Purpose:
 *		Like copyin, except that fromaddr is an address
 *		in the specified VM map.  This implementation
 *		is incomplete; it handles the current user map
 *		and the kernel map/submaps.
 */

int copyinmap(
	vm_map_t 	map,
	char 		*fromaddr, 
	char		*toaddr,
	int 		length)
{
	if (vm_map_pmap(map) == kernel_pmap) {
		/* assume a correct copy */
		memcpy(toaddr, fromaddr, length);
		return 0;
	}

	if (current_map() == map)
		return copyin( fromaddr, toaddr, length);

	return 1;
}

/*
 *	Routine:	copyoutmap
 *	Purpose:
 *		Like copyout, except that toaddr is an address
 *		in the specified VM map.  This implementation
 *		is incomplete; it handles the current user map
 *		and the kernel map/submaps.
 */

int copyoutmap(
	vm_map_t map,
	char 	*fromaddr, 
	char	*toaddr,
	int 	length)
{
	if (vm_map_pmap(map) == kernel_pmap) {
		/* assume a correct copy */
		memcpy(toaddr, fromaddr, length);
		return 0;
	}

	if (current_map() == map)
		return copyout(fromaddr, toaddr, length);

	return 1;
}
