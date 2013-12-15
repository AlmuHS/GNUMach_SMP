/*
 * (c) Copyright 1992, 1993, 1994, 1995 OPEN SOFTWARE FOUNDATION, INC. 
 * ALL RIGHTS RESERVED 
 */
/*
 * OSF RI nmk19b2 5/2/95
 */

#ifndef	_DDB_DB_PRINT_H_
#define	_DDB_DB_PRINT_H_

#include <mach/boolean.h>
#include <machine/db_machdep.h>

/* Prototypes for functions exported by this module.
 */
void db_show_regs(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	char		*modif);

void db_show_one_task(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	const char *	modif);

void db_show_port_id(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	const char *	modif);

void db_show_one_thread(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif);

void db_show_all_threads(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif);

db_addr_t db_task_from_space(
	ipc_space_t	space,
	int		*task_id);

#endif	/* !_DDB_DB_PRINT_H_ */
