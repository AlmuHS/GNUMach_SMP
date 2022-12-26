/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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

#include <string.h>

#include <mach/error.h>
#include <mach/vm_param.h>
#include <kern/syscall_emulation.h>
#include <kern/task.h>
#include <kern/kalloc.h>
#include <vm/vm_kern.h>

/* XXX */
#define syscall_emulation_sync(task)



/*
 * WARNING:
 * This code knows that kalloc() allocates memory most efficiently
 * in sizes that are powers of 2, and asks for those sizes.
 */

/*
 * Go from number of entries to size of struct eml_dispatch and back.
 */
#define	base_size	(sizeof(struct eml_dispatch) - sizeof(eml_routine_t))
#define	count_to_size(count) \
	(base_size + sizeof(vm_offset_t) * (count))

#define	size_to_count(size) \
	( ((size) - base_size) / sizeof(vm_offset_t) )

/*
 *  eml_init:	initialize user space emulation code
 */
void eml_init(void)
{
}

/*
 * eml_task_reference() [Exported]
 *
 *	Bumps the reference count on the common emulation
 *	vector.
 */

void eml_task_reference(
	task_t	task, 
	task_t	parent)
{
	eml_dispatch_t	eml;

	if (parent == TASK_NULL)
	    eml = EML_DISPATCH_NULL;
	else
	    eml = parent->eml_dispatch;

	if (eml != EML_DISPATCH_NULL) {
	    simple_lock(&eml->lock);
	    eml->ref_count++;
	    simple_unlock(&eml->lock);
	}
	task->eml_dispatch = eml;
}


/*
 * eml_task_deallocate() [Exported]
 *
 *	Cleans up after the emulation code when a process exits.
 */
 
void eml_task_deallocate(task)
	const task_t task;
{
	eml_dispatch_t	eml;

	eml = task->eml_dispatch;
	if (eml != EML_DISPATCH_NULL) {
	    int count;

	    simple_lock(&eml->lock);
	    count = --eml->ref_count;
	    simple_unlock(&eml->lock);

	    if (count == 0)
		kfree((vm_offset_t)eml, count_to_size(eml->disp_count));
	}
}

/*
 *   task_set_emulation_vector:  [Server Entry]
 *   set a list of emulated system calls for this task.
 */
static kern_return_t
task_set_emulation_vector_internal(
	task_t 			task,
	int			vector_start,
	emulation_vector_t	emulation_vector,
	unsigned int		emulation_vector_count)
{
	eml_dispatch_t	cur_eml, new_eml, old_eml;
	vm_size_t	new_size;
	int		cur_start, cur_end;
	int		new_start = 0, new_end = 0;
	int		vector_end;

	if (task == TASK_NULL)
		return EML_BAD_TASK;

	vector_end = vector_start + emulation_vector_count;

	/*
	 * We try to re-use the existing emulation vector
	 * if possible.  We can reuse the vector if it
	 * is not shared with another task and if it is
	 * large enough to contain the entries we are
	 * supplying.
	 *
	 * We must grab the lock on the task to check whether
	 * there is an emulation vector.
	 * If the vector is shared or not large enough, we
	 * need to drop the lock and allocate a new emulation
	 * vector.
	 *
	 * While the lock is dropped, the emulation vector
	 * may be released by all other tasks (giving us
	 * exclusive use), or may be enlarged by another
	 * task_set_emulation_vector call.  Therefore,
	 * after allocating the new emulation vector, we
	 * must grab the lock again to check whether we
	 * really need the new vector we just allocated.
	 *
	 * Since an emulation vector cannot be altered
	 * if it is in use by more than one task, the
	 * task lock is sufficient to protect the vector`s
	 * start, count, and contents.  The lock in the
	 * vector protects only the reference count.
	 */

	old_eml = EML_DISPATCH_NULL;	/* vector to discard */
	new_eml = EML_DISPATCH_NULL;	/* new vector */

	for (;;) {
	    /*
	     * Find the current emulation vector.
	     * See whether we can overwrite it.
	     */
	    task_lock(task);
	    cur_eml = task->eml_dispatch;
	    if (cur_eml != EML_DISPATCH_NULL) {
		cur_start = cur_eml->disp_min;
		cur_end   = cur_eml->disp_count + cur_start;

		simple_lock(&cur_eml->lock);
		if (cur_eml->ref_count == 1 &&
		    cur_start <= vector_start &&
		    cur_end >= vector_end)
		{
		    /*
		     * Can use the existing emulation vector.
		     * Discard any new one we allocated.
		     */
		    simple_unlock(&cur_eml->lock);
		    old_eml = new_eml;
		    break;
		}

		if (new_eml != EML_DISPATCH_NULL &&
		    new_start <= cur_start &&
		    new_end >= cur_end)
		{
		    /*
		     * A new vector was allocated, and it is large enough
		     * to hold all the entries from the current vector.
		     * Copy the entries to the new emulation vector,
		     * deallocate the current one, and use the new one.
		     */
		    memcpy(&new_eml->disp_vector[cur_start-new_start],
			   &cur_eml->disp_vector[0],
			   cur_eml->disp_count * sizeof(vm_offset_t));

		    if (--cur_eml->ref_count == 0)
			old_eml = cur_eml;	/* discard old vector */
		    simple_unlock(&cur_eml->lock);

		    task->eml_dispatch = new_eml;
		    syscall_emulation_sync(task);
		    cur_eml = new_eml;
		    break;
		}
		simple_unlock(&cur_eml->lock);

		/*
		 * Need a new emulation vector.
		 * Ensure it will hold all the entries from
		 * both the old and new emulation vectors.
		 */
		new_start = vector_start;
		if (new_start > cur_start)
		    new_start = cur_start;
		new_end = vector_end;
		if (new_end < cur_end)
		    new_end = cur_end;
	    }
	    else {
		/*
		 * There is no current emulation vector.
		 * If a new one was allocated, use it.
		 */
		if (new_eml != EML_DISPATCH_NULL) {
		    task->eml_dispatch = new_eml;
		    cur_eml = new_eml;
		    break;
		}

		/*
		 * Compute the size needed for the new vector.
		 */
		new_start = vector_start;
		new_end = vector_end;
	    }

	    /*
	     * Have no vector (or one that is no longer large enough).
	     * Drop all the locks and allocate a new vector.
	     * Repeat the loop to check whether the old vector was
	     * changed while we didn`t hold the locks.
	     */

	    task_unlock(task);

	    if (new_eml != EML_DISPATCH_NULL)
		kfree((vm_offset_t)new_eml, count_to_size(new_eml->disp_count));

	    new_size = count_to_size(new_end - new_start);
	    new_eml = (eml_dispatch_t) kalloc(new_size);

	    memset(new_eml, 0, new_size);
	    simple_lock_init(&new_eml->lock);
	    new_eml->ref_count = 1;
	    new_eml->disp_min   = new_start;
	    new_eml->disp_count = new_end - new_start;

	    continue;
	}

	/*
	 * We have the emulation vector.
	 * Install the new emulation entries.
	 */
	memcpy(&cur_eml->disp_vector[vector_start - cur_eml->disp_min],
	       &emulation_vector[0],
	       emulation_vector_count * sizeof(vm_offset_t));

	task_unlock(task);

	/*
	 * Discard any old emulation vector we don`t need.
	 */
	if (old_eml)
	    kfree((vm_offset_t) old_eml, count_to_size(old_eml->disp_count));

	return KERN_SUCCESS;
}

/*
 *	task_set_emulation_vector:  [Server Entry]
 *
 *	Set the list of emulated system calls for this task.
 *	The list is out-of-line.
 */
kern_return_t
task_set_emulation_vector(
	task_t 			task,
	int			vector_start,
	emulation_vector_t	emulation_vector,
	unsigned int		emulation_vector_count)
{
	kern_return_t		kr;
	vm_offset_t		emul_vector_addr;

	if (task == TASK_NULL)
	    return EML_BAD_TASK;	/* XXX sb KERN_INVALID_ARGUMENT */

	/*
	 *	The emulation vector is really a vm_map_copy_t.
	 */
	kr = vm_map_copyout(ipc_kernel_map, &emul_vector_addr,
			(vm_map_copy_t) emulation_vector);
	if (kr != KERN_SUCCESS)
	    return kr;

	/*
	 *	Do the work.
	 */
	kr = task_set_emulation_vector_internal(
			task,
			vector_start,
			(emulation_vector_t) emul_vector_addr,
			emulation_vector_count);

	/*
	 *	Discard the memory
	 */
	(void) kmem_free(ipc_kernel_map,
			 emul_vector_addr,
			 emulation_vector_count * sizeof(eml_dispatch_t));

	return kr;
}

/*
 *	task_get_emulation_vector: [Server Entry]
 *
 *	Get the list of emulated system calls for this task.
 *	List is returned out-of-line.
 */
kern_return_t
task_get_emulation_vector(
	task_t			task,
	int			*vector_start,			/* out */
	emulation_vector_t	*emulation_vector,		/* out */
	unsigned int		*emulation_vector_count)	/* out */
{
	eml_dispatch_t		eml;
	vm_size_t		vector_size, size;
	vm_offset_t		addr;

	if (task == TASK_NULL)
	    return EML_BAD_TASK;

	addr = 0;
	size = 0;

	for(;;) {
	    vm_size_t	size_needed;

	    task_lock(task);
	    eml = task->eml_dispatch;
	    if (eml == EML_DISPATCH_NULL) {
		task_unlock(task);
		if (addr)
		    (void) kmem_free(ipc_kernel_map, addr, size);
		*vector_start = 0;
		*emulation_vector = 0;
		*emulation_vector_count = 0;
		return KERN_SUCCESS;
	    }

	    /*
	     * Do we have the memory we need?
	     */
	    vector_size = eml->disp_count * sizeof(vm_offset_t);

	    size_needed = round_page(vector_size);
	    if (size_needed <= size)
		break;

	    /*
	     * If not, unlock the task and allocate more memory.
	     */
	    task_unlock(task);

	    if (size != 0)
		kmem_free(ipc_kernel_map, addr, size);

	    size = size_needed;
	    if (kmem_alloc(ipc_kernel_map, &addr, size) != KERN_SUCCESS)
		return KERN_RESOURCE_SHORTAGE;
	}

	/*
	 * Copy out the dispatch addresses
	 */
	*vector_start = eml->disp_min;
	*emulation_vector_count = eml->disp_count;
	memcpy((void *)addr,
	       eml->disp_vector,
	       vector_size);

	/*
	 * Unlock the task and free any memory we did not need
	 */
	task_unlock(task);

    {
	vm_size_t	size_used, size_left;
	vm_map_copy_t	memory;

	/*
	 * Free any unused memory beyond the end of the last page used
	 */
	size_used = round_page(vector_size);
	if (size_used != size)
	    (void) kmem_free(ipc_kernel_map,
			     addr + size_used,
			     size - size_used);

	/*
	 * Zero the remainder of the page being returned.
	 */
	size_left = size_used - vector_size;
	if (size_left > 0)
	    memset((char *)addr + vector_size, 0, size_left);

	/*
	 * Make memory into copyin form - this unwires it.
	 */
	(void) vm_map_copyin(ipc_kernel_map, addr, vector_size, TRUE, &memory);

	*emulation_vector = (emulation_vector_t) memory;
    }

	return KERN_SUCCESS;
}

/*
 *   task_set_emulation:  [Server Entry]
 *   set up for user space emulation of syscalls within this task.
 */
kern_return_t task_set_emulation(
	task_t		task,
	vm_offset_t 	routine_entry_pt,
	int		routine_number)
{
	return task_set_emulation_vector_internal(task, routine_number,
					 &routine_entry_pt, 1);
}
