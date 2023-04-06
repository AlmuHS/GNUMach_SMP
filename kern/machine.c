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
 *	File:	kern/machine.c
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1987
 *
 *	Support for machine independent machine abstraction.
 */

#include <string.h>
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/machine.h>
#include <mach/host_info.h>
#include <kern/counters.h>
#include <kern/debug.h>
#include <kern/ipc_host.h>
#include <kern/host.h>
#include <kern/machine.h>
#include <kern/mach_host.server.h>
#include <kern/lock.h>
#include <kern/processor.h>
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/printf.h>
#include <machine/machspl.h>	/* for splsched */
#include <machine/model_dep.h>
#include <machine/pcb.h>
#include <sys/reboot.h>



/*
 *	Exported variables:
 */

struct machine_info	machine_info;
struct machine_slot	machine_slot[NCPUS];

queue_head_t	action_queue;	/* assign/shutdown queue */
def_simple_lock_data(,action_lock);

/*
 *	cpu_up:
 *
 *	Flag specified cpu as up and running.  Called when a processor comes
 *	online.
 */
void cpu_up(int cpu)
{
	struct machine_slot	*ms;
	processor_t		processor;
	spl_t 			s;

	processor = cpu_to_processor(cpu);
	pset_lock(&default_pset);
	s = splsched();
	processor_lock(processor);
#if	NCPUS > 1
	init_ast_check(processor);
#endif	/* NCPUS > 1 */
	ms = &machine_slot[cpu];
	ms->running = TRUE;
	machine_info.avail_cpus++;
	pset_add_processor(&default_pset, processor);
	processor->state = PROCESSOR_RUNNING;
	processor_unlock(processor);
	splx(s);
	pset_unlock(&default_pset);
}

kern_return_t
host_reboot(const host_t host, int options)
{
	if (host == HOST_NULL)
		return (KERN_INVALID_HOST);

	if (options & RB_DEBUGGER) {
		Debugger("Debugger");
	} else {
#ifdef parisc
/* XXX this could be made common */
		halt_all_cpus(options);
#else
		halt_all_cpus(!(options & RB_HALT));
#endif
	}
	return (KERN_SUCCESS);
}

#if	NCPUS > 1

/*
 *	cpu_down:
 *
 *	Flag specified cpu as down.  Called when a processor is about to
 *	go offline.
 */
static void cpu_down(int cpu)
{
	struct machine_slot	*ms;
	processor_t		processor;
	spl_t			s;

	s = splsched();
	processor = cpu_to_processor(cpu);
	processor_lock(processor);
	ms = &machine_slot[cpu];
	ms->running = FALSE;
	machine_info.avail_cpus--;
	/*
	 *	processor has already been removed from pset.
	 */
	processor->processor_set_next = PROCESSOR_SET_NULL;
	processor->state = PROCESSOR_OFF_LINE;
	processor_unlock(processor);
	splx(s);
}

/*
 *	processor_request_action - common internals of processor_assign
 *		and processor_shutdown.  If new_pset is null, this is
 *		a shutdown, else it's an assign and caller must donate
 *		a reference.
 */
static void
processor_request_action(
	processor_t	processor,
	processor_set_t	new_pset)
{
    processor_set_t pset;

    /*
     *	Processor must be in a processor set.  Must lock its idle lock to
     *	get at processor state.
     */
    pset = processor->processor_set;
    simple_lock(&pset->idle_lock);

    /*
     *	If the processor is dispatching, let it finish - it will set its
     *	state to running very soon.
     */
    while (*(volatile int *)&processor->state == PROCESSOR_DISPATCHING)
	cpu_pause();

    /*
     *	Now lock the action queue and do the dirty work.
     */
    simple_lock(&action_lock);

    switch (processor->state) {
	case PROCESSOR_IDLE:
	    /*
	     *	Remove from idle queue.
	     */
	    queue_remove(&pset->idle_queue, processor, 	processor_t,
		processor_queue);
	    pset->idle_count--;

	    /* fall through ... */
	case PROCESSOR_RUNNING:
	    /*
	     *	Put it on the action queue.
	     */
	    queue_enter(&action_queue, processor, processor_t,
		processor_queue);

	    /* fall through ... */
	case PROCESSOR_ASSIGN:
	    /*
	     * And ask the action_thread to do the work.
	     */

	    if (new_pset == PROCESSOR_SET_NULL) {
		processor->state = PROCESSOR_SHUTDOWN;
	    }
	    else {
		assert(processor->state != PROCESSOR_ASSIGN);
		processor->state = PROCESSOR_ASSIGN;
	        processor->processor_set_next = new_pset;
	    }
	    break;

	default:
	    printf("state: %d\n", processor->state);
	    panic("processor_request_action: bad state");
    }
    simple_unlock(&action_lock);
    simple_unlock(&pset->idle_lock);

    thread_wakeup((event_t)&action_queue);
}

#if	MACH_HOST
/*
 *	processor_assign() changes the processor set that a processor is
 *	assigned to.  Any previous assignment in progress is overridden.
 *	Synchronizes with assignment completion if wait is TRUE.
 */
kern_return_t
processor_assign(
	processor_t	processor,
	processor_set_t	new_pset,
	boolean_t	wait)
{
    spl_t		s;

    /*
     *	Check for null arguments.
     *  XXX Can't assign master processor.
     */
    if (processor == PROCESSOR_NULL || new_pset == PROCESSOR_SET_NULL ||
	processor == master_processor) {
	    return(KERN_INVALID_ARGUMENT);
    }

    /*
     *	Get pset reference to donate to processor_request_action.
     */
    pset_reference(new_pset);

    /*
     * Check processor status.
     * If shutdown or being shutdown, can`t reassign.
     * If being assigned, wait for assignment to finish.
     */
Retry:
    s = splsched();
    processor_lock(processor);
    if(processor->state == PROCESSOR_OFF_LINE ||
	processor->state == PROCESSOR_SHUTDOWN) {
	    /*
	     *	Already shutdown or being shutdown -- Can't reassign.
	     */
	    processor_unlock(processor);
	    (void) splx(s);
	    pset_deallocate(new_pset);
	    return(KERN_FAILURE);
    }

    if (processor->state == PROCESSOR_ASSIGN) {
	assert_wait((event_t) processor, TRUE);
	processor_unlock(processor);
	splx(s);
	thread_block(thread_no_continuation);
	goto Retry;
    }
	 
    /*
     *	Avoid work if processor is already in this processor set.
     */
    if (processor->processor_set == new_pset)  {
	processor_unlock(processor);
	(void) splx(s);
	/* clean up dangling ref */
	pset_deallocate(new_pset);
	return(KERN_SUCCESS);
    }

    /*
     * OK to start processor assignment.
     */
    processor_request_action(processor, new_pset);

    /*
     *	Synchronization with completion.
     */
    if (wait) {
	while (processor->state == PROCESSOR_ASSIGN ||
	    processor->state == PROCESSOR_SHUTDOWN) {
		assert_wait((event_t)processor, TRUE);
		processor_unlock(processor);
		splx(s);
		thread_block(thread_no_continuation);
		s = splsched();
		processor_lock(processor);
	}
    }
    processor_unlock(processor);
    splx(s);
    
    return(KERN_SUCCESS);
}

#else	/* MACH_HOST */

kern_return_t
processor_assign(
	processor_t	processor,
	processor_set_t	new_pset,
	boolean_t	wait)
{
	return KERN_FAILURE;
}

#endif	/* MACH_HOST */

/*
 *	processor_shutdown() queues a processor up for shutdown.
 *	Any assignment in progress is overriden.  It does not synchronize
 *	with the shutdown (can be called from interrupt level).
 */
kern_return_t
processor_shutdown(processor_t processor)
{
    spl_t		s;

    if (processor == PROCESSOR_NULL)
	return KERN_INVALID_ARGUMENT;

    s = splsched();
    processor_lock(processor);
    if(processor->state == PROCESSOR_OFF_LINE ||
	processor->state == PROCESSOR_SHUTDOWN) {
	    /*
	     *	Already shutdown or being shutdown -- nothing to do.
	     */
	    processor_unlock(processor);
	    splx(s);
	    return(KERN_SUCCESS);
    }

    processor_request_action(processor, PROCESSOR_SET_NULL);
    processor_unlock(processor);
    splx(s);

    return(KERN_SUCCESS);
}

/*
 *	processor_doaction actually does the shutdown.  The trick here
 *	is to schedule ourselves onto a cpu and then save our
 *	context back into the runqs before taking out the cpu.
 */
static void processor_doaction(processor_t processor)
{
	thread_t			this_thread;
	spl_t				s;
	processor_set_t			pset;
#if	MACH_HOST
	processor_set_t			new_pset;
	thread_t			thread;
	thread_t			prev_thread = THREAD_NULL;
	boolean_t			have_pset_ref = FALSE;
#endif	/* MACH_HOST */

	/*
	 *	Get onto the processor to shutdown
	 */
	this_thread = current_thread();
	thread_bind(this_thread, processor);
	thread_block(thread_no_continuation);

	pset = processor->processor_set;
#if	MACH_HOST
	/*
	 *	If this is the last processor in the processor_set,
	 *	stop all the threads first.
	 */
	pset_lock(pset);
	if (pset->processor_count == 1) {
		/*
		 *	First suspend all of them.
		 */
		queue_iterate(&pset->threads, thread, thread_t, pset_threads) {
			thread_hold(thread);
		}
		pset->empty = TRUE;
		/*
		 *	Now actually stop them.  Need a pset reference.
		 */
		pset->ref_count++;
		have_pset_ref = TRUE;

Restart_thread:
		prev_thread = THREAD_NULL;
		queue_iterate(&pset->threads, thread, thread_t, pset_threads) {
			thread_reference(thread);
			pset_unlock(pset);
			if (prev_thread != THREAD_NULL)
				thread_deallocate(prev_thread);

			/*
			 *	Only wait for threads still in the pset.
			 */
			thread_freeze(thread);
			if (thread->processor_set != pset) {
				/*
				 *	It got away - start over.
				 */
				thread_unfreeze(thread);
				thread_deallocate(thread);
				pset_lock(pset);
				goto Restart_thread;
			}

			(void) thread_dowait(thread, TRUE);
			prev_thread = thread;
			pset_lock(pset);
			thread_unfreeze(prev_thread);
		}
	}
	pset_unlock(pset);

	/*
	 *	At this point, it is ok to remove the processor from the pset.
	 *	We can use processor->processor_set_next without locking the
	 *	processor, since it cannot change while processor->state is
	 *	PROCESSOR_ASSIGN or PROCESSOR_SHUTDOWN.
	 */

	new_pset = processor->processor_set_next;

Restart_pset:
	if (new_pset) {
	    /*
	     *	Reassigning processor.
	     */

	    if ((integer_t) pset < (integer_t) new_pset) {
		pset_lock(pset);
		pset_lock(new_pset);
	    }
	    else {
		pset_lock(new_pset);
		pset_lock(pset);
	    }
	    if (!(new_pset->active)) {
		pset_unlock(new_pset);
		pset_unlock(pset);
		pset_deallocate(new_pset);
		new_pset = &default_pset;
		pset_reference(new_pset);
		goto Restart_pset;
	    }

	    /*
	     *  Handle remove last / assign first race.
	     *	Only happens if there is more than one action thread.
	     */
	    while (new_pset->empty && new_pset->processor_count > 0) {
		pset_unlock(new_pset);
		pset_unlock(pset);
		while (*(volatile boolean_t *)&new_pset->empty &&
		       *(volatile int *)&new_pset->processor_count > 0)
			/* spin */;
		goto Restart_pset;
	    }

	    /*
	     *	Lock the processor.  new_pset should not have changed.
	     */
	    s = splsched();
	    processor_lock(processor);
	    assert(processor->processor_set_next == new_pset);

	    /*
	     *	Shutdown may have been requested while this assignment
	     *	was in progress.
	     */
	    if (processor->state == PROCESSOR_SHUTDOWN) {
		processor->processor_set_next = PROCESSOR_SET_NULL;
		pset_unlock(new_pset);
		goto shutdown;	/* releases pset reference */
	    }

	    /*
	     *	Do assignment, then wakeup anyone waiting for it.
	     */
	    pset_remove_processor(pset, processor);
	    pset_unlock(pset);

	    pset_add_processor(new_pset, processor);
	    if (new_pset->empty) {
		/*
		 *	Set all the threads loose.
		 *
		 *	NOTE: this appears to violate the locking
		 *	order, since the processor lock should
		 *	be taken AFTER a thread lock.  However,
		 *	thread_setrun (called by thread_release)
		 *	only takes the processor lock if the
		 *	processor is idle.  The processor is
		 *	not idle here.
		 */
		queue_iterate(&new_pset->threads, thread, thread_t,
			      pset_threads) {
		    thread_release(thread);
		}
		new_pset->empty = FALSE;
	    }
	    processor->processor_set_next = PROCESSOR_SET_NULL;
	    processor->state = PROCESSOR_RUNNING;
	    thread_wakeup((event_t)processor);
	    processor_unlock(processor);
	    splx(s);
	    pset_unlock(new_pset);

	    /*
	     *	Clean up dangling references, and release our binding.
	     */
	    pset_deallocate(new_pset);
	    if (have_pset_ref)
		pset_deallocate(pset);
	    if (prev_thread != THREAD_NULL)
		thread_deallocate(prev_thread);
	    thread_bind(this_thread, PROCESSOR_NULL);

	    thread_block(thread_no_continuation);
	    return;
	}

#endif	/* MACH_HOST */
	
	/*
	 *	Do shutdown, make sure we live when processor dies.
	 */
	if (processor->state != PROCESSOR_SHUTDOWN) {
		printf("state: %d\n", processor->state);
	    	panic("action_thread -- bad processor state");
	}

	s = splsched();
	processor_lock(processor);

#if	MACH_HOST
    shutdown:
#endif	/* MACH_HOST */
	pset_remove_processor(pset, processor);
	processor_unlock(processor);
	pset_unlock(pset);
	splx(s);

	/*
	 *	Clean up dangling references, and release our binding.
	 */
#if	MACH_HOST
	if (new_pset != PROCESSOR_SET_NULL)
		pset_deallocate(new_pset);
	if (have_pset_ref)
		pset_deallocate(pset);
	if (prev_thread != THREAD_NULL)
		thread_deallocate(prev_thread);
#endif	/* MACH_HOST */

	thread_bind(this_thread, PROCESSOR_NULL);
	switch_to_shutdown_context(this_thread,
				   processor_doshutdown,
				   processor);

}

/*
 *	action_thread() shuts down processors or changes their assignment.
 */
void __attribute__((noreturn)) action_thread_continue(void)
{
	processor_t	processor;
	spl_t		s;

	while (TRUE) {
		s = splsched();
		simple_lock(&action_lock);
		while ( !queue_empty(&action_queue)) {
			processor = (processor_t) queue_first(&action_queue);
			queue_remove(&action_queue, processor, processor_t,
				     processor_queue);
			simple_unlock(&action_lock);
			(void) splx(s);

			processor_doaction(processor);

			s = splsched();
			simple_lock(&action_lock);
		}

		assert_wait((event_t) &action_queue, FALSE);
		simple_unlock(&action_lock);
		(void) splx(s);
		counter(c_action_thread_block++);
		thread_block(action_thread_continue);
	}
}

void __attribute__((noreturn)) action_thread(void)
{
	action_thread_continue();
	/*NOTREACHED*/
}

/*
 *	Actually do the processor shutdown.  This is called at splsched,
 *	running on the processor's shutdown stack.
 */

void processor_doshutdown(processor_t processor)
{
	int		cpu = processor->slot_num;

	timer_switch(&kernel_timer[cpu]);

	/*
	 *	Ok, now exit this cpu.
	 */
	PMAP_DEACTIVATE_KERNEL(cpu);
#ifndef MIGRATING_THREADS
	active_threads[cpu] = THREAD_NULL;
#endif
	cpu_down(cpu);
	thread_wakeup((event_t)processor);
	halt_cpu();
	/*
	 *	The action thread returns to life after the call to
	 *	switch_to_shutdown_context above, on some other cpu.
	 */

	/*NOTREACHED*/
}
#else	/* NCPUS > 1 */

kern_return_t
processor_assign(
	processor_t	processor,
	processor_set_t	new_pset,
	boolean_t	wait)
{
	return(KERN_FAILURE);
}

#endif /* NCPUS > 1 */
