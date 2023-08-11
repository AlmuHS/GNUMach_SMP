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
 *	File:	kern/lock.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Locking primitives implementation
 */

#include <string.h>

#include <machine/smp.h>

#include <kern/debug.h>
#include <kern/lock.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#if	MACH_KDB
#include <machine/db_machdep.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>
#endif


#if	NCPUS > 1

/*
 *	Module:		lock
 *	Function:
 *		Provide reader/writer sychronization.
 *	Implementation:
 *		Simple interlock on a bit.  Readers first interlock,
 *		increment the reader count, then let go.  Writers hold
 *		the interlock (thus preventing further readers), and
 *		wait for already-accepted readers to go away.
 */

/*
 *	The simple-lock routines are the primitives out of which
 *	the lock package is built.  The implementation is left
 *	to the machine-dependent code.
 */

#ifdef	notdef
/*
 *	A sample implementation of simple locks.
 *	assumes:
 *		boolean_t test_and_set(boolean_t *)
 *			indivisibly sets the boolean to TRUE
 *			and returns its old value
 *		and that setting a boolean to FALSE is indivisible.
 */
/*
 *	simple_lock_init initializes a simple lock.  A simple lock
 *	may only be used for exclusive locks.
 */

void simple_lock_init(simple_lock_t l)
{
	*(boolean_t *)l = FALSE;
}

void simple_lock(simple_lock_t l)
{
	while (test_and_set((boolean_t *)l))
		cpu_pause();
}

void simple_unlock(simple_lock_t l)
{
	*(boolean_t *)l = FALSE;
}

boolean_t simple_lock_try(simple_lock_t l)
{
    	return (!test_and_set((boolean_t *)l));
}
#endif	/* notdef */
#endif	/* NCPUS > 1 */

#if	NCPUS > 1
static int lock_wait_time = 100;
#else	/* NCPUS > 1 */

	/*
	 * 	It is silly to spin on a uni-processor as if we
	 *	thought something magical would happen to the
	 *	want_write bit while we are executing.
	 */
static int lock_wait_time = 0;
#endif	/* NCPUS > 1 */

#if	MACH_SLOCKS && NCPUS == 1
/*
 *	This code does not protect simple_locks_taken and simple_locks_info.
 *	It works despite the fact that interrupt code does use simple locks.
 *	This is because interrupts use locks in a stack-like manner.
 *	Each interrupt releases all the locks it acquires, so the data
 *	structures end up in the same state after the interrupt as before.
 *	The only precaution necessary is that simple_locks_taken be
 *	incremented first and decremented last, so that interrupt handlers
 *	don't over-write active slots in simple_locks_info.
 */

unsigned int simple_locks_taken = 0;

#define	NSLINFO	1000		/* maximum number of locks held */

struct simple_locks_info {
	simple_lock_t l;
	const char *expr;
	const char *loc;
} simple_locks_info[NSLINFO];

int do_check_simple_locks = 1;

void check_simple_locks(void)
{
	assert(! do_check_simple_locks || simple_locks_taken == 0);
}

void check_simple_locks_enable(void)
{
	do_check_simple_locks = 1;
}

void check_simple_locks_disable(void)
{
	do_check_simple_locks = 0;
}

/* Need simple lock sanity checking code if simple locks are being
   compiled in, and we are compiling for a uniprocessor. */

void simple_lock_init(
	simple_lock_t l)
{
	l->lock_data = 0;
}

void _simple_lock(
	simple_lock_t l,
	const char *expression,
	const char *location)
{
	struct simple_locks_info *info;

	assert(l->lock_data == 0);

	l->lock_data = 1;

	info = &simple_locks_info[simple_locks_taken++];
	barrier();
	info->l = l;
	info->expr = expression;
	info->loc = location;
}

boolean_t _simple_lock_try(
	simple_lock_t l,
	const char *expression,
	const char *location)
{
	struct simple_locks_info *info;

	if (l->lock_data != 0)
		return FALSE;

	l->lock_data = 1;

	info = &simple_locks_info[simple_locks_taken++];
	barrier();
	info->l = l;
	info->expr = expression;
	info->loc = location;

	return TRUE;
}

void _simple_unlock(
	simple_lock_t l)
{
	assert(l->lock_data != 0);

	l->lock_data = 0;

	if (simple_locks_info[simple_locks_taken-1].l != l) {
		unsigned int i = simple_locks_taken;

		/* out-of-order unlocking */

		do
			if (i == 0)
				panic("simple_unlock");
		while (simple_locks_info[--i].l != l);

		simple_locks_info[i] = simple_locks_info[simple_locks_taken-1];
	}
	barrier();
	simple_locks_taken--;
	simple_locks_info[simple_locks_taken] = (struct simple_locks_info) {0};
}

#endif	/* MACH_SLOCKS && NCPUS == 1 */

/*
 *	Routine:	lock_init
 *	Function:
 *		Initialize a lock; required before use.
 *		Note that clients declare the "struct lock"
 *		variables and then initialize them, rather
 *		than getting a new one from this module.
 */
void lock_init(
	lock_t		l,
	boolean_t	can_sleep)
{
	memset(l, 0, sizeof(lock_data_t));
	simple_lock_init(&l->interlock);
	l->want_write = FALSE;
	l->want_upgrade = FALSE;
	l->read_count = 0;
	l->can_sleep = can_sleep;
	l->thread = (struct thread *)-1;	/* XXX */
	l->recursion_depth = 0;
}

void lock_sleepable(
	lock_t		l,
	boolean_t	can_sleep)
{
	simple_lock(&l->interlock);
	l->can_sleep = can_sleep;
	simple_unlock(&l->interlock);
}


/*
 *	Sleep locks.  These use the same data structure and algorithm
 *	as the spin locks, but the process sleeps while it is waiting
 *	for the lock.  These work on uniprocessor systems.
 */

void lock_write(
	lock_t	l)
{
	int	i;

	check_simple_locks();
	simple_lock(&l->interlock);

	if (l->thread == current_thread()) {
		/*
		 *	Recursive lock.
		 */
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return;
	}

	/*
	 *	Try to acquire the want_write bit.
	 */
	while (l->want_write) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && l->want_write)
				cpu_pause();
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && l->want_write) {
			l->waiting = TRUE;
			thread_sleep(l,
				simple_lock_addr(l->interlock), FALSE);
			simple_lock(&l->interlock);
		}
	}
	l->want_write = TRUE;

	/* Wait for readers (and upgrades) to finish */

	while ((l->read_count != 0) || l->want_upgrade) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && (l->read_count != 0 ||
					l->want_upgrade))
				cpu_pause();
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && (l->read_count != 0 || l->want_upgrade)) {
			l->waiting = TRUE;
			thread_sleep(l,
				simple_lock_addr(l->interlock), FALSE);
			simple_lock(&l->interlock);
		}
	}
#if MACH_LDEBUG
	l->writer = current_thread();
#endif	/* MACH_LDEBUG */
	simple_unlock(&l->interlock);
}

void lock_done(
	lock_t	l)
{
	simple_lock(&l->interlock);

	if (l->read_count != 0)
		l->read_count--;
	else
	if (l->recursion_depth != 0)
		l->recursion_depth--;
	else
	if (l->want_upgrade) {
	 	l->want_upgrade = FALSE;
#if MACH_LDEBUG
		assert(l->writer == current_thread());
		l->writer = THREAD_NULL;
#endif	/* MACH_LDEBUG */
	} else {
	 	l->want_write = FALSE;
#if MACH_LDEBUG
		assert(l->writer == current_thread());
		l->writer = THREAD_NULL;
#endif	/* MACH_LDEBUG */
	}

	/*
	 *	There is no reason to wakeup a waiting thread
	 *	if the read-count is non-zero.  Consider:
	 *		we must be dropping a read lock
	 *		threads are waiting only if one wants a write lock
	 *		if there are still readers, they can't proceed
	 */

	if (l->waiting && (l->read_count == 0)) {
		l->waiting = FALSE;
		thread_wakeup(l);
	}

	simple_unlock(&l->interlock);
}

void lock_read(
	lock_t	l)
{
	int	i;

	check_simple_locks();
	simple_lock(&l->interlock);

	if (l->thread == current_thread()) {
		/*
		 *	Recursive lock.
		 */
		l->read_count++;
		simple_unlock(&l->interlock);
		return;
	}

	while (l->want_write || l->want_upgrade) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && (l->want_write || l->want_upgrade))
				cpu_pause();
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && (l->want_write || l->want_upgrade)) {
			l->waiting = TRUE;
			thread_sleep(l,
				simple_lock_addr(l->interlock), FALSE);
			simple_lock(&l->interlock);
		}
	}

	l->read_count++;
	simple_unlock(&l->interlock);
}

/*
 *	Routine:	lock_read_to_write
 *	Function:
 *		Improves a read-only lock to one with
 *		write permission.  If another reader has
 *		already requested an upgrade to a write lock,
 *		no lock is held upon return.
 *
 *		Returns TRUE if the upgrade *failed*.
 */
boolean_t lock_read_to_write(
	lock_t	l)
{
	int	i;

	check_simple_locks();
	simple_lock(&l->interlock);

	l->read_count--;

	if (l->thread == current_thread()) {
		/*
		 *	Recursive lock.
		 */
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return(FALSE);
	}

	if (l->want_upgrade) {
		/*
		 *	Someone else has requested upgrade.
		 *	Since we've released a read lock, wake
		 *	him up.
		 */
		if (l->waiting && (l->read_count == 0)) {
			l->waiting = FALSE;
			thread_wakeup(l);
		}

		simple_unlock(&l->interlock);
		return TRUE;
	}

	l->want_upgrade = TRUE;

	while (l->read_count != 0) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && l->read_count != 0)
				cpu_pause();
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && l->read_count != 0) {
			l->waiting = TRUE;
			thread_sleep(l,
				simple_lock_addr(l->interlock), FALSE);
			simple_lock(&l->interlock);
		}
	}

#if MACH_LDEBUG
	l->writer = current_thread();
#endif	/* MACH_LDEBUG */
	simple_unlock(&l->interlock);
	return FALSE;
}

void lock_write_to_read(
	lock_t	l)
{
	simple_lock(&l->interlock);
#if MACH_LDEBUG
	assert(l->writer == current_thread());
#endif	/* MACH_LDEBUG */

	l->read_count++;
	if (l->recursion_depth != 0)
		l->recursion_depth--;
	else
	if (l->want_upgrade)
		l->want_upgrade = FALSE;
	else
	 	l->want_write = FALSE;

	if (l->waiting) {
		l->waiting = FALSE;
		thread_wakeup(l);
	}

#if MACH_LDEBUG
	assert(l->writer == current_thread());
	l->writer = THREAD_NULL;
#endif	/* MACH_LDEBUG */
	simple_unlock(&l->interlock);
}


/*
 *	Routine:	lock_try_write
 *	Function:
 *		Tries to get a write lock.
 *
 *		Returns FALSE if the lock is not held on return.
 */

boolean_t lock_try_write(
	lock_t	l)
{
	simple_lock(&l->interlock);

	if (l->thread == current_thread()) {
		/*
		 *	Recursive lock
		 */
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return TRUE;
	}

	if (l->want_write || l->want_upgrade || l->read_count) {
		/*
		 *	Can't get lock.
		 */
		simple_unlock(&l->interlock);
		return FALSE;
	}

	/*
	 *	Have lock.
	 */

	l->want_write = TRUE;
#if MACH_LDEBUG
	l->writer = current_thread();
#endif	/* MACH_LDEBUG */
	simple_unlock(&l->interlock);
	return TRUE;
}

/*
 *	Routine:	lock_try_read
 *	Function:
 *		Tries to get a read lock.
 *
 *		Returns FALSE if the lock is not held on return.
 */

boolean_t lock_try_read(
	lock_t	l)
{
	simple_lock(&l->interlock);

	if (l->thread == current_thread()) {
		/*
		 *	Recursive lock
		 */
		l->read_count++;
		simple_unlock(&l->interlock);
		return TRUE;
	}

	if (l->want_write || l->want_upgrade) {
		simple_unlock(&l->interlock);
		return FALSE;
	}

	l->read_count++;
	simple_unlock(&l->interlock);
	return TRUE;
}

/*
 *	Routine:	lock_try_read_to_write
 *	Function:
 *		Improves a read-only lock to one with
 *		write permission.  If another reader has
 *		already requested an upgrade to a write lock,
 *		the read lock is still held upon return.
 *
 *		Returns FALSE if the upgrade *failed*.
 */
boolean_t lock_try_read_to_write(
	lock_t	l)
{
	check_simple_locks();
	simple_lock(&l->interlock);

	if (l->thread == current_thread()) {
		/*
		 *	Recursive lock
		 */
		l->read_count--;
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return TRUE;
	}

	if (l->want_upgrade) {
		simple_unlock(&l->interlock);
		return FALSE;
	}
	l->want_upgrade = TRUE;
	l->read_count--;

	while (l->read_count != 0) {
		l->waiting = TRUE;
		thread_sleep(l,
			simple_lock_addr(l->interlock), FALSE);
		simple_lock(&l->interlock);
	}

#if MACH_LDEBUG
	l->writer = current_thread();
#endif	/* MACH_LDEBUG */
	simple_unlock(&l->interlock);
	return TRUE;
}

/*
 *	Allow a process that has a lock for write to acquire it
 *	recursively (for read, write, or update).
 */
void lock_set_recursive(
	lock_t		l)
{
	simple_lock(&l->interlock);
#if MACH_LDEBUG
	assert(l->writer == current_thread());
#endif	/* MACH_LDEBUG */

	if (!l->want_write) {
		panic("lock_set_recursive: don't have write lock");
	}
	l->thread = current_thread();
	simple_unlock(&l->interlock);
}

/*
 *	Prevent a lock from being re-acquired.
 */
void lock_clear_recursive(
	lock_t		l)
{
	simple_lock(&l->interlock);
	if (l->thread != current_thread()) {
		panic("lock_clear_recursive: wrong thread");
	}
	if (l->recursion_depth == 0)
		l->thread = (struct thread *)-1;	/* XXX */
	simple_unlock(&l->interlock);
}

#if	MACH_KDB
#if	MACH_SLOCKS && NCPUS == 1
void db_show_all_slocks(void)
{
	int i;
	struct simple_locks_info *info;
	simple_lock_t l;

	for (i = 0; i < simple_locks_taken; i++) {
		info = &simple_locks_info[i];
		db_printf("%d: %s (", i, info->expr);
		db_printsym(info->l, DB_STGY_ANY);
		db_printf(") locked by %s\n", info->loc);
	}
}
#else	/* MACH_SLOCKS && NCPUS == 1 */
void db_show_all_slocks(void)
{
#if	MACH_LOCK_MON
	lip();
#else
	db_printf("simple lock info not available\n");
#endif
}
#endif	/* MACH_SLOCKS && NCPUS == 1 */
#endif	/* MACH_KDB */
