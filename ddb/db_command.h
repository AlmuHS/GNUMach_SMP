/*
 * Mach Operating System
 * Copyright (c) 1992,1991,1990 Carnegie Mellon University
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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#ifndef _DDB_DB_COMMAND_H_
#define _DDB_DB_COMMAND_H_

#if MACH_KDB

/*
 * Command loop declarations.
 */

#include <machine/db_machdep.h>
#include <machine/setjmp.h>

extern void		db_command_loop(void);
extern boolean_t	db_option(const char *, int) __attribute__ ((pure));

extern void		db_error(const char *) __attribute__ ((noreturn));	/* report error */

extern db_addr_t	db_dot;		/* current location */
extern db_addr_t	db_last_addr;	/* last explicit address typed */
extern db_addr_t	db_prev;	/* last address examined
					   or written */
extern db_addr_t	db_next;	/* next address to be examined
					   or written */
extern jmp_buf_t *	db_recover;	/* error recovery */

typedef void (*db_command_fun_t)(db_expr_t, boolean_t, db_expr_t, const char *);

/*
 * Command table
 */
struct db_command {
	char *	name;		/* command name */
	db_command_fun_t fcn;	/* function to call */
	int	flag;		/* extra info: */
#define	CS_OWN		0x1	    /* non-standard syntax */
#define	CS_MORE		0x2	    /* standard syntax, but may have other
				       words at end */
#define	CS_SET_DOT	0x100	    /* set dot after command */
	struct db_command *more;   /* another level of command */
};

extern boolean_t db_exec_cmd_nest(char *cmd, int size);

void db_fncall(void);

void db_help_cmd(void);

#endif /* MACH_KDB */

#endif /* _DDB_DB_COMMAND_H_ */
