/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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

#ifndef _DDB_DB_TASK_THREAD_H_
#define _DDB_DB_TASK_THREAD_H_

#include <ddb/db_variables.h>

#include <kern/task.h>
#include <kern/thread.h>

#define db_current_task()						\
		((current_thread())? current_thread()->task: TASK_NULL)
#define db_target_space(thread, user_space)				\
		((!(user_space))? TASK_NULL:				\
		(thread)? (thread)->task: db_current_task())
#define db_is_current_task(task) 					\
		((task) == TASK_NULL || (task) == db_current_task())

extern task_t	db_default_task;		/* default target task */
extern thread_t	db_default_thread;		/* default target thread */

extern int		db_lookup_task(const task_t);
extern int		db_lookup_thread(const thread_t);
extern int		db_lookup_task_thread(const task_t, const thread_t);
extern boolean_t	db_check_thread_address_valid(const thread_t);
extern boolean_t	db_get_next_thread(thread_t *, int);
extern void		db_init_default_thread(void);

extern void
db_set_default_thread(
	struct db_variable 	*vp,
	db_expr_t		*valuep,
	int			flag,
	db_var_aux_param_t	ap);

extern void
db_get_task_thread(
	struct db_variable	*vp,
	db_expr_t		*valuep,
	int			flag,
	db_var_aux_param_t	ap);

extern void
db_get_map(struct db_variable *vp,
	   db_expr_t *valuep,
	   int flag,
	   db_var_aux_param_t ap);

#endif  /* _DDB_DB_TASK_THREAD_H_ */
