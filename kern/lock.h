/* 
 * Mach Operating System
 * Copyright (c) 1993-1987 Carnegie Mellon University
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
 *	File:	kern/lock.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Locking primitives definitions
 */

#ifndef	_KERN_LOCK_H_
#define	_KERN_LOCK_H_

#include <mach/boolean.h>
#include <mach/machine/vm_types.h>
#include <machine/spl.h>

/*
 * Note: we cannot blindly use simple locks in interrupt handlers, otherwise one
 * may try to acquire a lock while already having the lock, thus a deadlock.
 *
 * When locks are needed in interrupt handlers, the _irq versions of the calls
 * should be used, which disable interrupts (by calling splhigh) before acquiring
 * the lock, thus preventing the deadlock. They need to be used this way:
 *
 * spl_t s = simple_lock_irq(&mylock);
 * [... critical section]
 * simple_unlock_irq(s, &mylock);
 *
 * To catch faulty code, when MACH_LDEBUG is set we check that non-_irq versions
 * are not called while handling an interrupt.
 *
 * In the following, the _nocheck versions don't check anything, the _irq
 * versions disable interrupts, and the pristine versions add a check when
 * MACH_LDEBUG is set.
 */

#if NCPUS > 1
#include <machine/lock.h>/*XXX*/
#if MACH_LOCK_MON == 0
#define simple_lock_nocheck	_simple_lock
#define simple_lock_try_nocheck	_simple_lock_try
#define simple_unlock_nocheck	_simple_unlock
#else
#define simple_lock_nocheck	simple_lock
#define simple_lock_try_nocheck	simple_lock_try
#define simple_unlock_nocheck	simple_unlock
#endif
#endif

#define MACH_SLOCKS	NCPUS > 1

/*
 *	A simple spin lock.
 */

struct slock {
	volatile natural_t lock_data;	/* in general 1 bit is sufficient */
	struct {} is_a_simple_lock;
};

/*
 *	Used by macros to assert that the given argument is a simple
 *	lock.
 */
#define simple_lock_assert(l)	(void) &(l)->is_a_simple_lock

typedef struct slock	simple_lock_data_t;
typedef struct slock	*simple_lock_t;

#if	MACH_SLOCKS
/*
 *	Use the locks.
 */

#define	decl_simple_lock_data(class,name) \
class	simple_lock_data_t	name;
#define	def_simple_lock_data(class,name) \
class	simple_lock_data_t	name = SIMPLE_LOCK_INITIALIZER(&name);
#define	def_simple_lock_irq_data(class,name) \
class	simple_lock_irq_data_t	name = { SIMPLE_LOCK_INITIALIZER(&name.lock) };

#define	simple_lock_addr(lock)	(simple_lock_assert(&(lock)),	\
				 &(lock))
#define	simple_lock_irq_addr(l)	(simple_lock_irq_assert(&(l)),	\
				&(l)->lock)

#if	(NCPUS > 1)

/*
 *	The single-CPU debugging routines are not valid
 *	on a multiprocessor.
 */
#define	simple_lock_taken(lock)		(simple_lock_assert(lock),	\
					 1)	/* always succeeds */
#define check_simple_locks()
#define check_simple_locks_enable()
#define check_simple_locks_disable()

#else	/* NCPUS > 1 */
/*
 *	Use our single-CPU locking test routines.
 */

extern void		simple_lock_init(simple_lock_t);
extern void		_simple_lock(simple_lock_t,
				     const char *, const char *);
extern void		_simple_unlock(simple_lock_t);
extern boolean_t	_simple_lock_try(simple_lock_t,
					 const char *, const char *);

/* We provide simple_lock and simple_lock_try so that we can save the
   location.  */
#define XSTR(x)		#x
#define STR(x)		XSTR(x)
#define LOCATION	__FILE__ ":" STR(__LINE__)

#define simple_lock_nocheck(lock)	_simple_lock((lock), #lock, LOCATION)
#define simple_lock_try_nocheck(lock)	_simple_lock_try((lock), #lock, LOCATION)
#define simple_unlock_nocheck(lock)	_simple_unlock((lock))

#define simple_lock_pause()
#define simple_lock_taken(lock)		(simple_lock_assert(lock),	\
					 (lock)->lock_data)

extern void		check_simple_locks(void);
extern void		check_simple_locks_enable(void);
extern void		check_simple_locks_disable(void);

#endif	/* NCPUS > 1 */

#else	/* MACH_SLOCKS */
/*
 * Do not allocate storage for locks if not needed.
 */
struct simple_lock_data_empty { struct {} is_a_simple_lock; };
struct simple_lock_irq_data_empty { struct simple_lock_data_empty slock; };
#define	decl_simple_lock_data(class,name)	\
class struct simple_lock_data_empty name;
#define	def_simple_lock_data(class,name)	\
class struct simple_lock_data_empty name;
#define	def_simple_lock_irq_data(class,name)	\
class struct simple_lock_irq_data_empty name;
#define	simple_lock_addr(lock)		(simple_lock_assert(&(lock)),	\
					 (simple_lock_t)0)
#define	simple_lock_irq_addr(lock)	(simple_lock_irq_assert(&(lock)),	\
					 (simple_lock_t)0)

/*
 *	No multiprocessor locking is necessary.
 */
#define simple_lock_init(l)	simple_lock_assert(l)
#define simple_lock_nocheck(l)		simple_lock_assert(l)
#define simple_unlock_nocheck(l)	simple_lock_assert(l)
#define simple_lock_try_nocheck(l)	(simple_lock_assert(l),		\
				 TRUE)	/* always succeeds */
#define simple_lock_taken(l)	(simple_lock_assert(l),		\
				 1)	/* always succeeds */
#define check_simple_locks()
#define check_simple_locks_enable()
#define check_simple_locks_disable()
#define simple_lock_pause()

#endif	/* MACH_SLOCKS */


#define decl_mutex_data(class,name)	decl_simple_lock_data(class,name)
#define def_mutex_data(class,name)	def_simple_lock_data(class,name)
#define	mutex_try(l)			simple_lock_try(l)
#define	mutex_lock(l)			simple_lock(l)
#define	mutex_unlock(l)			simple_unlock(l)
#define	mutex_init(l)			simple_lock_init(l)


/*
 *	The general lock structure.  Provides for multiple readers,
 *	upgrading from read to write, and sleeping until the lock
 *	can be gained.
 *
 *	On some architectures, assembly language code in the 'inline'
 *	program fiddles the lock structures.  It must be changed in
 *	concert with the structure layout.
 *
 *	Only the "interlock" field is used for hardware exclusion;
 *	other fields are modified with normal instructions after
 *	acquiring the interlock bit.
 */
struct lock {
	struct thread	*thread;	/* Thread that has lock, if
					   recursive locking allowed */
	unsigned int	read_count:16,	/* Number of accepted readers */
	/* boolean_t */	want_upgrade:1,	/* Read-to-write upgrade waiting */
	/* boolean_t */	want_write:1,	/* Writer is waiting, or
					   locked for write */
	/* boolean_t */	waiting:1,	/* Someone is sleeping on lock */
	/* boolean_t */	can_sleep:1,	/* Can attempts to lock go to sleep? */
			recursion_depth:12, /* Depth of recursion */
			:0; 
#if MACH_LDEBUG
	struct thread	*writer;
#endif	/* MACH_LDEBUG */
	decl_simple_lock_data(,interlock)
					/* Hardware interlock field.
					   Last in the structure so that
					   field offsets are the same whether
					   or not it is present. */
};

typedef struct lock	lock_data_t;
typedef struct lock	*lock_t;

/* Sleep locks must work even if no multiprocessing */

extern void		lock_init(lock_t, boolean_t);
extern void		lock_sleepable(lock_t, boolean_t);
extern void		lock_write(lock_t);
extern void		lock_read(lock_t);
extern void		lock_done(lock_t);
extern boolean_t	lock_read_to_write(lock_t);
extern void		lock_write_to_read(lock_t);
extern boolean_t	lock_try_write(lock_t);
extern boolean_t	lock_try_read(lock_t);
extern boolean_t	lock_try_read_to_write(lock_t);

#define	lock_read_done(l)	lock_done(l)
#define	lock_write_done(l)	lock_done(l)

extern void		lock_set_recursive(lock_t);
extern void		lock_clear_recursive(lock_t);

/* Lock debugging support.  */
#if	! MACH_LDEBUG
#define have_read_lock(l)	1
#define have_write_lock(l)	1
#define lock_check_no_interrupts()
#else	/* MACH_LDEBUG */
/* XXX: We don't keep track of readers, so this is an approximation.  */
#define have_read_lock(l)	((l)->read_count > 0)
#define have_write_lock(l)	((l)->writer == current_thread())
extern unsigned long in_interrupt[NCPUS];
#define lock_check_no_interrupts()	assert(!in_interrupt[cpu_number()])
#endif	/* MACH_LDEBUG */
#define have_lock(l)		(have_read_lock(l) || have_write_lock(l))

/* These are defined elsewhere with lock monitoring */
#if MACH_LOCK_MON == 0
#define simple_lock(l)		do { \
	lock_check_no_interrupts(); \
	simple_lock_nocheck(l); \
} while (0)
#define simple_lock_try(l)	({ \
	lock_check_no_interrupts(); \
	simple_lock_try_nocheck(l); \
})
#define simple_unlock(l)	do { \
	lock_check_no_interrupts(); \
	simple_unlock_nocheck(l); \
} while (0)
#endif

/* _irq variants */

struct slock_irq {
	struct slock slock;
};

#define simple_lock_irq_assert(l)	simple_lock_assert(&(l)->slock)

typedef struct slock_irq	simple_lock_irq_data_t;
typedef struct slock_irq	*simple_lock_irq_t;

#define	decl_simple_lock_irq_data(class,name) \
class	simple_lock_irq_data_t	name;

#define simple_lock_init_irq(l) simple_lock_init(&(l)->slock)

#define simple_lock_irq(l)	({ \
	spl_t __s = splhigh(); \
	simple_lock_nocheck(&(l)->slock); \
	__s; \
})
#define simple_unlock_irq(s, l)	do { \
	simple_unlock_nocheck(&(l)->slock); \
	splx(s); \
} while (0)

#if	MACH_KDB
extern void db_show_all_slocks(void);
#endif	/* MACH_KDB */

extern void lip(void);

#endif	/* _KERN_LOCK_H_ */
