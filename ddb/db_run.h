/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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

#ifndef _DDB_DB_RUN_H_
#define _DDB_DB_RUN_H_

#include <kern/task.h>
#include <machine/db_machdep.h>

extern int db_run_mode;

/* modes the system may be running in */

#define	STEP_NONE	0
#define	STEP_ONCE	1
#define	STEP_RETURN	2
#define	STEP_CALLT	3
#define	STEP_CONTINUE	4
#define STEP_INVISIBLE	5
#define	STEP_COUNT	6

extern void db_single_step(db_regs_t *regs, task_t task);

extern void db_single_step_cmd(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char	*modif);

void db_trace_until_call_cmd(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif);

void db_trace_until_matching_cmd(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif);

void db_continue_cmd(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif);

#ifndef db_set_single_step
void		db_set_task_single_step(db_regs_t *, task_t);
#else
#define	db_set_task_single_step(regs, task)	db_set_single_step(regs)
#endif
#ifndef db_clear_single_step
void		db_clear_task_single_step(const db_regs_t *, task_t);
#else
#define db_clear_task_single_step(regs, task)	db_clear_single_step(regs)
#endif

extern boolean_t db_in_single_step(void);

extern void
db_restart_at_pc(
	boolean_t watchpt,
	task_t	  task);

extern boolean_t
db_stop_at_pc(
	boolean_t	*is_breakpoint,
	task_t		task);

#endif /* _DDB_DB_RUN_H_ */
