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
/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	10/90
 */

#if MACH_KDB

#ifndef	_DDB_DB_WATCH_
#define	_DDB_DB_WATCH_

#include <mach/machine/vm_types.h>
#include <kern/task.h>
#include <machine/db_machdep.h>

/*
 * Watchpoint.
 */

typedef struct db_watchpoint {
	task_t    task;			/* in this map */
	db_addr_t loaddr;		/* from this address */
	db_addr_t hiaddr;		/* to this address */
	struct db_watchpoint *link;	/* link in in-use or free chain */
} *db_watchpoint_t;

extern boolean_t db_find_watchpoint(vm_map_t map, db_addr_t addr,
				    db_regs_t *regs);
extern void db_set_watchpoints(void);
extern void db_clear_watchpoints(void);

extern void db_set_watchpoint(const task_t task, db_addr_t addr, vm_size_t size);
extern void db_delete_watchpoint(const task_t task, db_addr_t addr);
extern void db_list_watchpoints(void);

void db_listwatch_cmd(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	const char *	modif);

void db_deletewatch_cmd(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif);

void db_watchpoint_cmd(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif);

#endif	/* _DDB_DB_WATCH_ */

#endif /* MACH_KDB */
