/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University.
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
 *	processor.h:	Processor and processor-set definitions.
 */

#ifndef	_KERN_PROCESSOR_H_
#define	_KERN_PROCESSOR_H_

/*
 *	Data structures for managing processors and sets of processors.
 */

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/processor_info.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/kern_types.h>
#include <kern/host.h>

#if	NCPUS > 1
#include <machine/ast_types.h>
#endif	/* NCPUS > 1 */

struct processor_set {
	struct run_queue	runq;		/* runq for this set */
	queue_head_t		idle_queue;	/* idle processors */
	int			idle_count;	/* how many ? */
	decl_simple_lock_data(,	idle_lock)	/* lock for above, shall be taken at splsched only */
	queue_head_t		processors;	/* all processors here */
	int			processor_count;	/* how many ? */
	boolean_t		empty;		/* true if no processors */
	queue_head_t		tasks;		/* tasks assigned */
	int			task_count;	/* how many */
	queue_head_t		threads;	/* threads in this set */
	int			thread_count;	/* how many */
	int			ref_count;	/* structure ref count */
	decl_simple_lock_data(,	ref_lock)	/* lock for ref count */
	queue_chain_t		all_psets;	/* link for all_psets */
	boolean_t		active;		/* is pset in use */
	decl_simple_lock_data(,	lock)		/* lock for everything else */
	struct ipc_port	*	pset_self;	/* port for operations */
	struct ipc_port *	pset_name_self;	/* port for information */
	int			max_priority;	/* maximum priority */
#if	MACH_FIXPRI
	int			policies;	/* bit vector for policies */
#endif	/* MACH_FIXPRI */
	int			set_quantum;	/* current default quantum */
#if	NCPUS > 1
	int			quantum_adj_index; /* runtime quantum adj. */
	decl_simple_lock_irq_data(, quantum_adj_lock)  /* lock for above */
	int			machine_quantum[NCPUS+1]; /* ditto */
#endif	/* NCPUS > 1 */
	long			mach_factor;	/* mach_factor */
	long			load_average;	/* load_average */
	long			sched_load;	/* load avg for scheduler */
};
extern struct processor_set	default_pset;
#if	MACH_HOST
extern struct processor_set	*slave_pset;
#endif

struct processor {
	struct run_queue runq;		/* local runq for this processor */
		/* XXX want to do this round robin eventually */
	queue_chain_t	processor_queue; /* idle/assign/shutdown queue link */
	int		state;		/* See below */
	struct thread	*next_thread;	/* next thread to run if dispatched */
	struct thread	*idle_thread;	/* this processor's idle thread. */
	int		quantum;	/* quantum for current thread */
	boolean_t	first_quantum;	/* first quantum in succession */
	int		last_quantum;	/* last quantum assigned */

	processor_set_t	processor_set;	/* processor set I belong to */
	processor_set_t processor_set_next;	/* set I will belong to */
	queue_chain_t	processors;	/* all processors in set */
	decl_simple_lock_data(,	lock)
	struct ipc_port *processor_self;	/* port for operations */
	int		slot_num;	/* machine-indep slot number */
#if	NCPUS > 1
	ast_check_t	ast_check_data;	/* for remote ast_check invocation */
#endif	/* NCPUS > 1 */
	/* punt id data temporarily */
};
typedef struct processor Processor;
extern struct processor	processor_array[NCPUS];

#include <kern/cpu_number.h>
#include <machine/percpu.h>

/*
 *	Chain of all processor sets.
 */
extern queue_head_t		all_psets;
extern int			all_psets_count;
decl_simple_lock_data(extern, all_psets_lock);

/*
 *	The lock ordering is:
 *
 *			all_psets_lock
 *			    |
 *			    |
 *			    V
 *			pset_lock
 *			    |
 *		+-----------+---------------+-------------------+
 *		|	    |		    |			|
 *		|	    |		    |			|
 *		|	    |		    V			V
 *		|	    |		task_lock	pset_self->ip_lock
 *		|	    |		    |			|
 *		|	    |	+-----------+---------------+	|
 *		|	    |	|			    |	|
 *		|	    V	V			    V	V
 *		|	thread_lock*			pset_ref_lock
 *		|	    |
 *		|   +-------+
 *		|   |	    |
 *		|   |	    V
 *		|   |	runq_lock*
 *		|   |
 *		V   V
 *	processor_lock*
 *		|
 *		|
 *		V
 *	pset_idle_lock*
 *		|
 *		|
 *		V
 *	action_lock*
 *
 *	Locks marked with "*" are taken at splsched.
 */

/*
 *	XXX need a pointer to the master processor structure
 */

extern processor_t	master_processor;

/*
 *	NOTE: The processor->processor_set link is needed in one of the
 *	scheduler's critical paths.  [Figure out where to look for another
 *	thread to run on this processor.]  It is accessed without locking.
 *	The following access protocol controls this field.
 *
 *	Read from own processor - just read.
 *	Read from another processor - lock processor structure during read.
 *	Write from own processor - lock processor structure during write.
 *	Write from another processor - NOT PERMITTED.
 *
 */

/*
 *	Processor state locking:
 *
 *	Values for the processor state are defined below.  If the processor
 *	is off-line or being shutdown, then it is only necessary to lock
 *	the processor to change its state.  Otherwise it is only necessary
 *	to lock its processor set's idle_lock.  Scheduler code will
 *	typically lock only the idle_lock, but processor manipulation code
 *	will often lock both.
 */

#define PROCESSOR_OFF_LINE	0	/* Not in system */
#define	PROCESSOR_RUNNING	1	/* Running normally */
#define	PROCESSOR_IDLE		2	/* idle */
#define PROCESSOR_DISPATCHING	3	/* dispatching (idle -> running) */
#define	PROCESSOR_ASSIGN	4	/* Assignment is changing */
#define PROCESSOR_SHUTDOWN	5	/* Being shutdown */

#define processor_ptr(i)	(&percpu_array[i].processor)
#define cpu_to_processor	processor_ptr

#define current_processor()	(percpu_ptr(struct processor, processor))
#define current_processor_set()	(current_processor()->processor_set)

/* Compatibility -- will go away */

#define cpu_state(slot_num)	(processor_ptr(slot_num)->state)
#define cpu_idle(slot_num)	(cpu_state(slot_num) == PROCESSOR_IDLE)

/* Useful lock macros */

#define	pset_lock(pset)		simple_lock(&(pset)->lock)
#define pset_unlock(pset)	simple_unlock(&(pset)->lock)
#define	pset_ref_lock(pset)	simple_lock(&(pset)->ref_lock)
#define	pset_ref_unlock(pset)	simple_unlock(&(pset)->ref_lock)

/* Shall be taken at splsched only */
#define processor_lock(pr)	simple_lock(&(pr)->lock)
#define processor_unlock(pr)	simple_unlock(&(pr)->lock)

typedef mach_port_t	*processor_array_t;
typedef mach_port_t	*processor_set_array_t;
typedef mach_port_t	*processor_set_name_array_t;


/*
 *	Exported functions
 */

/* Initialization */

#ifdef KERNEL
#if	MACH_HOST
extern void	pset_sys_init(void);
#endif	/* MACH_HOST */

/* Pset internal functions */

extern void	pset_sys_bootstrap(void);
extern void	pset_reference(processor_set_t);
extern void	pset_deallocate(processor_set_t);
extern void	pset_remove_processor(processor_set_t, processor_t);
extern void	pset_add_processor(processor_set_t, processor_t);
extern void	pset_remove_task(processor_set_t, struct task *);
extern void	pset_add_task(processor_set_t, struct task *);
extern void	pset_remove_thread(processor_set_t, struct thread *);
extern void	pset_add_thread(processor_set_t, struct thread *);
extern void	thread_change_psets(struct thread *,
				processor_set_t, processor_set_t);

/* Processor interface */

extern kern_return_t processor_get_assignment(
		processor_t	processor,
		processor_set_t *processor_set);

extern kern_return_t processor_info(
		processor_t	processor,
		int		flavor,
		host_t *	host,
		processor_info_t info,
		natural_t *	count);

extern kern_return_t processor_start(
		processor_t	processor);

extern kern_return_t processor_exit(
		processor_t	processor);

extern kern_return_t processor_control(
		processor_t	processor,
		processor_info_t info,
		natural_t 	count);

/* Pset interface */

extern kern_return_t processor_set_create(
		host_t		host,
		processor_set_t *new_set,
		processor_set_t *new_name);

extern kern_return_t processor_set_destroy(
		processor_set_t	pset);

extern kern_return_t processor_set_info(
		processor_set_t	pset,
		int		flavor,
		host_t		*host,
		processor_set_info_t info,
		natural_t	*count);

extern kern_return_t processor_set_max_priority(
		processor_set_t	pset,
		int		max_priority,
		boolean_t	change_threads);

extern kern_return_t processor_set_policy_enable(
		processor_set_t	pset,
		int		policy);

extern kern_return_t processor_set_policy_disable(
		processor_set_t	pset,
		int		policy,
		boolean_t	change_threads);

extern kern_return_t processor_set_tasks(
		processor_set_t	pset,
		task_array_t	*task_list,
		natural_t	*count);

extern kern_return_t processor_set_threads(
		processor_set_t	pset,
		thread_array_t	*thread_list,
		natural_t	*count);
#endif

void processor_doshutdown(processor_t processor);
void quantum_set(processor_set_t pset);
void pset_init(processor_set_t pset);
void processor_init(processor_t pr, int slot_num);

#endif	/* _KERN_PROCESSOR_H_ */
