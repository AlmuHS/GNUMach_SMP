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
 *	File:	vm/vm_pageout.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	The proverbial page-out daemon.
 */

#include <device/net_io.h>
#include <mach/mach_types.h>
#include <mach/memory_object.h>
#include <vm/memory_object_default.user.h>
#include <vm/memory_object_user.user.h>
#include <mach/vm_param.h>
#include <mach/vm_statistics.h>
#include <kern/counters.h>
#include <kern/debug.h>
#include <kern/slab.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/printf.h>
#include <vm/memory_object.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <machine/locore.h>

#define DEBUG 0

/*
 * Maximum delay, in milliseconds, between two pageout scans.
 */
#define VM_PAGEOUT_TIMEOUT 50

/*
 * Event placeholder for pageout requests, synchronized with
 * the free page queue lock.
 */
static int vm_pageout_requested;

/*
 * Event placeholder for pageout throttling, synchronized with
 * the free page queue lock.
 */
static int vm_pageout_continue;

/*
 *	Routine:	vm_pageout_setup
 *	Purpose:
 *		Set up a page for pageout.
 *
 *		Move or copy the page to a new object, as part
 *		of which it will be sent to its memory manager
 *		in a memory_object_data_return or memory_object_initialize
 *		message.
 *
 *		The "paging_offset" argument specifies the offset
 *		of the page within its external memory object.
 *
 *		The "new_object" and "new_offset" arguments
 *		indicate where the page should be moved.
 *
 *		The "flush" argument specifies whether the page
 *		should be flushed from its object.  If not, a
 *		copy of the page is moved to the new object.
 *
 *	In/Out conditions:
 *		The page in question must not be on any pageout queues,
 *		and must be busy.  The object to which it belongs
 *		must be unlocked, and the caller must hold a paging
 *		reference to it.  The new_object must not be locked.
 *
 *		If the page is flushed from its original object,
 *		this routine returns a pointer to a place-holder page,
 *		inserted at the same offset, to block out-of-order
 *		requests for the page.  The place-holder page must
 *		be freed after the data_return or initialize message
 *		has been sent.  If the page is copied,
 *		the holding page is VM_PAGE_NULL.
 *
 *		The original page is put on a paging queue and marked
 *		not busy on exit.
 */
vm_page_t
vm_pageout_setup(
	vm_page_t		m,
	vm_offset_t		paging_offset,
	vm_object_t		new_object,
	vm_offset_t		new_offset,
	boolean_t		flush)
{
	vm_object_t	old_object = m->object;
	vm_page_t	holding_page = 0; /*'=0'to quiet gcc warnings*/
	vm_page_t	new_m;

	assert(m->busy && !m->absent && !m->fictitious);

	/*
	 *	If we are not flushing the page, allocate a
	 *	page in the object.
	 */
	if (!flush) {
		for (;;) {
			vm_object_lock(new_object);
			new_m = vm_page_alloc(new_object, new_offset);
			vm_object_unlock(new_object);

			if (new_m != VM_PAGE_NULL) {
				break;
			}

			VM_PAGE_WAIT(NULL);
		}
	}

	if (flush) {
		/*
		 *	Create a place-holder page where the old one was,
		 *	to prevent anyone from attempting to page in this
		 *	page while we`re unlocked.
		 */
		while ((holding_page = vm_page_grab_fictitious())
							== VM_PAGE_NULL)
			vm_page_more_fictitious();

		vm_object_lock(old_object);
		vm_page_lock_queues();
		vm_page_remove(m);
		vm_page_unlock_queues();
		PAGE_WAKEUP_DONE(m);

		vm_page_lock_queues();
		vm_page_insert(holding_page, old_object, m->offset);
		vm_page_unlock_queues();

		/*
		 *	Record that this page has been written out
		 */
#if	MACH_PAGEMAP
		vm_external_state_set(old_object->existence_info,
					paging_offset,
					VM_EXTERNAL_STATE_EXISTS);
#endif	/* MACH_PAGEMAP */

		vm_object_unlock(old_object);

		vm_object_lock(new_object);

		/*
		 *	Move this page into the new object
		 */

		vm_page_lock_queues();
		vm_page_insert(m, new_object, new_offset);
		vm_page_unlock_queues();

		m->dirty = TRUE;
		m->precious = FALSE;
		m->page_lock = VM_PROT_NONE;
		m->unlock_request = VM_PROT_NONE;
	}
	else {
		/*
		 *	Copy the data into the new page,
		 *	and mark the new page as clean.
		 */
		vm_page_copy(m, new_m);

		vm_object_lock(old_object);
		m->dirty = FALSE;
		pmap_clear_modify(m->phys_addr);

		/*
		 *	Deactivate old page.
		 */
		vm_page_lock_queues();
		vm_page_deactivate(m);
		vm_page_unlock_queues();

		PAGE_WAKEUP_DONE(m);

		/*
		 *	Record that this page has been written out
		 */

#if	MACH_PAGEMAP
		vm_external_state_set(old_object->existence_info,
					paging_offset,
					VM_EXTERNAL_STATE_EXISTS);
#endif	/* MACH_PAGEMAP */

		vm_object_unlock(old_object);

		vm_object_lock(new_object);

		/*
		 *	Use the new page below.
		 */
		m = new_m;
		m->dirty = TRUE;
		assert(!m->precious);
		PAGE_WAKEUP_DONE(m);
	}

	/*
	 *	Make the old page eligible for replacement again; if a
	 *	user-supplied memory manager fails to release the page,
	 *	it will be paged out again to the default memory manager.
	 *
	 *	Note that pages written to the default memory manager
	 *	must be wired down -- in return, it guarantees to free
	 *	this page, rather than reusing it.
	 */

	vm_page_lock_queues();
	vm_stat.pageouts++;
	if (m->laundry) {

		/*
		 *	The caller is telling us that it is going to
		 *	immediately double page this page to the default
		 *	pager.
		 */

		assert(!old_object->internal);
		m->laundry = FALSE;
	} else if (old_object->internal ||
		   memory_manager_default_port(old_object->pager)) {
		m->laundry = TRUE;
		vm_page_laundry_count++;

		vm_page_wire(m);
	} else {
		m->external_laundry = TRUE;

		/*
		 *	If vm_page_external_laundry_count is negative,
		 *	the pageout daemon isn't expecting to be
		 *	notified.
		 */

		if (vm_page_external_laundry_count >= 0) {
			vm_page_external_laundry_count++;
		}

		vm_page_activate(m);
	}
	vm_page_unlock_queues();

	/*
	 *	Since IPC operations may block, we drop locks now.
	 *	[The placeholder page is busy, and we still have
	 *	paging_in_progress incremented.]
	 */

	vm_object_unlock(new_object);

	/*
	 *	Return the placeholder page to simplify cleanup.
	 */
	return (flush ? holding_page : VM_PAGE_NULL);
}

/*
 *	Routine:	vm_pageout_page
 *	Purpose:
 *		Causes the specified page to be written back to
 *		the appropriate memory object.
 *
 *		The "initial" argument specifies whether this
 *		data is an initialization only, and should use
 *		memory_object_data_initialize instead of
 *		memory_object_data_return.
 *
 *		The "flush" argument specifies whether the page
 *		should be flushed from the object.  If not, a
 *		copy of the data is sent to the memory object.
 *
 *	In/out conditions:
 *		The page in question must not be on any pageout queues.
 *		The object to which it belongs must be locked.
 *	Implementation:
 *		Move this page to a completely new object, if flushing;
 *		copy to a new page in a new object, if not.
 */
void
vm_pageout_page(
	vm_page_t		m,
	boolean_t		initial,
	boolean_t		flush)
{
	vm_map_copy_t		copy;
	vm_object_t		old_object;
	vm_object_t		new_object;
	vm_page_t		holding_page;
	vm_offset_t		paging_offset;
	kern_return_t		rc;
	boolean_t		precious_clean;

	assert(m->busy);

	/*
	 *	Cleaning but not flushing a clean precious page is a
	 *	no-op.  Remember whether page is clean and precious now
	 *	because vm_pageout_setup will mark it dirty and not precious.
	 *
	 * XXX Check if precious_clean && !flush can really happen.
	 */
	precious_clean = (!m->dirty) && m->precious;
	if (precious_clean && !flush) {
		PAGE_WAKEUP_DONE(m);
		return;
	}

	/*
	 *	Verify that we really want to clean this page.
	 */
	if (m->absent || m->error || (!m->dirty && !m->precious)) {
		VM_PAGE_FREE(m);
		return;
	}

	/*
	 *	Create a paging reference to let us play with the object.
	 */
	old_object = m->object;
	paging_offset = m->offset + old_object->paging_offset;
	vm_object_paging_begin(old_object);
	vm_object_unlock(old_object);

	/*
	 *	Allocate a new object into which we can put the page.
	 */
	new_object = vm_object_allocate(PAGE_SIZE);
	new_object->used_for_pageout = TRUE;

	/*
	 *	Move the page into the new object.
	 */
	holding_page = vm_pageout_setup(m,
				paging_offset,
				new_object,
				0,		/* new offset */
				flush);		/* flush */

	rc = vm_map_copyin_object(new_object, 0, PAGE_SIZE, &copy);
	assert(rc == KERN_SUCCESS);

	if (initial) {
		rc = memory_object_data_initialize(
			 old_object->pager,
			 old_object->pager_request,
			 paging_offset, (pointer_t) copy, PAGE_SIZE);
	}
	else {
		rc = memory_object_data_return(
			 old_object->pager,
			 old_object->pager_request,
			 paging_offset, (pointer_t) copy, PAGE_SIZE,
			 !precious_clean, !flush);
	}

	if (rc != KERN_SUCCESS)
		vm_map_copy_discard(copy);

	/*
	 *	Clean up.
	 */
	vm_object_lock(old_object);
	if (holding_page != VM_PAGE_NULL)
	    VM_PAGE_FREE(holding_page);
	vm_object_paging_end(old_object);
}

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 *
 *	Return TRUE if the pageout daemon is done for now, FALSE otherwise,
 *	in which case should_wait indicates whether the pageout daemon
 *	should wait to allow pagers to keep up.
 *
 *	It returns with vm_page_queue_free_lock held.
 */

static boolean_t vm_pageout_scan(boolean_t *should_wait)
{
	boolean_t done;

	/*
	 *	Try balancing pages among segments first, since this
	 *	may be enough to resume unprivileged allocations.
	 */

	/* This function returns with vm_page_queue_free_lock held */
	done = vm_page_balance();

	if (done) {
		return TRUE;
	}

	simple_unlock(&vm_page_queue_free_lock);

	/*
	 *	Balancing is not enough. Shrink caches and scan pages
	 *	for eviction.
	 */

	stack_collect();
	net_kmsg_collect();
	consider_task_collect();
	if (0)	/* XXX: pcb_collect doesn't do anything yet, so it is
		   pointless to call consider_thread_collect.  */
	consider_thread_collect();

	/*
	 *	slab_collect should be last, because the other operations
	 *	might return memory to caches.
	 */
	slab_collect();

	vm_page_refill_inactive();

	/* This function returns with vm_page_queue_free_lock held */
	return vm_page_evict(should_wait);
}

void vm_pageout(void)
{
	boolean_t done, should_wait;

	current_thread()->vm_privilege = 1;
	stack_privilege(current_thread());
	thread_set_own_priority(0);

	for (;;) {
		done = vm_pageout_scan(&should_wait);
		/* we hold vm_page_queue_free_lock now */

		if (done) {
			thread_sleep(&vm_pageout_requested,
				     simple_lock_addr(vm_page_queue_free_lock),
				     FALSE);
		} else if (should_wait) {
			assert_wait(&vm_pageout_continue, FALSE);
			thread_set_timeout(VM_PAGEOUT_TIMEOUT * hz / 1000);
			simple_unlock(&vm_page_queue_free_lock);
			thread_block(NULL);

#if DEBUG
			if (current_thread()->wait_result != THREAD_AWAKENED) {
				printf("vm_pageout: timeout,"
				       " vm_page_laundry_count:%d"
				       " vm_page_external_laundry_count:%d\n",
				       vm_page_laundry_count,
				       vm_page_external_laundry_count);
			}
#endif
		} else {
			simple_unlock(&vm_page_queue_free_lock);
		}
	}
}

/*
 *	Start pageout
 *
 *	The free page queue lock must be held before calling this function.
 */
void vm_pageout_start(void)
{
	if (!current_thread())
		return;

	thread_wakeup_one(&vm_pageout_requested);
}

/*
 *	Resume pageout
 *
 *	The free page queue lock must be held before calling this function.
 */
void vm_pageout_resume(void)
{
	thread_wakeup_one(&vm_pageout_continue);
}
