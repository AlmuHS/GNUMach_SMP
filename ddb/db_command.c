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
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#if MACH_KDB

/*
 * Command dispatcher.
 */

#include <string.h>
#include <mach/boolean.h>
#include <machine/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_task_thread.h>
#include <ddb/db_macro.h>
#include <ddb/db_expr.h>
#include <ddb/db_examine.h>
#include <ddb/db_print.h>
#include <ddb/db_break.h>
#include <ddb/db_watch.h>
#include <ddb/db_variables.h>
#include <ddb/db_write_cmd.h>
#include <ddb/db_run.h>
#include <ddb/db_cond.h>

#include <machine/setjmp.h>
#include <machine/db_interface.h>
#include <kern/debug.h>
#include <kern/thread.h>
#include <kern/slab.h>
#include <ipc/ipc_pset.h> /* 4proto */
#include <ipc/ipc_port.h> /* 4proto */

#include <vm/vm_print.h>
#include <ipc/ipc_print.h>
#include <kern/lock.h>

/*
 * Exported global variables
 */
boolean_t	db_cmd_loop_done;
jmp_buf_t	*db_recover = 0;
db_addr_t	db_dot;
db_addr_t	db_last_addr;
db_addr_t	db_prev;
db_addr_t	db_next;

/*
 * if 'ed' style: 'dot' is set at start of last item printed,
 * and '+' points to next line.
 * Otherwise: 'dot' points to next item, '..' points to last.
 */
boolean_t	db_ed_style = TRUE;

/*
 * Results of command search.
 */
#define	CMD_UNIQUE	0
#define	CMD_FOUND	1
#define	CMD_NONE	2
#define	CMD_AMBIGUOUS	3
#define	CMD_HELP	4

/*
 * Search for command prefix.
 */
int
db_cmd_search(name, table, cmdp)
	const char *		name;
	const struct db_command	*table;
	const struct db_command	**cmdp;	/* out */
{
	const struct db_command	*cmd;
	int		result = CMD_NONE;

	for (cmd = table; cmd->name != 0; cmd++) {
	    const char *lp;
	    char *rp;
	    int  c;

	    lp = name;
	    rp = cmd->name;
	    while ((c = *lp) == *rp) {
		if (c == 0) {
		    /* complete match */
		    *cmdp = cmd;
		    return (CMD_UNIQUE);
		}
		lp++;
		rp++;
	    }
	    if (c == 0) {
		/* end of name, not end of command -
		   partial match */
		if (result == CMD_FOUND) {
		    result = CMD_AMBIGUOUS;
		    /* but keep looking for a full match -
		       this lets us match single letters */
		}
		else {
		    *cmdp = cmd;
		    result = CMD_FOUND;
		}
	    }
	}
	if (result == CMD_NONE) {
	    /* check for 'help' */
	    if (!strncmp(name, "help", strlen(name)))
		result = CMD_HELP;
	}
	return (result);
}

void
db_cmd_list(table)
	const struct db_command *table;
{
	const struct db_command *cmd;

	for (cmd = table; cmd->name != 0; cmd++) {
	    db_printf("%-12s", cmd->name);
	    db_end_line();
	}
}

void
db_command(
	struct db_command	**last_cmdp,	/* IN_OUT */
	struct db_command	*cmd_table)
{
	struct db_command	*cmd;
	int		t;
	char		modif[TOK_STRING_SIZE];
	db_expr_t	addr, count;
	boolean_t	have_addr = FALSE;
	int		result;

	t = db_read_token();
	if (t == tEOL || t == tSEMI_COLON) {
	    /* empty line repeats last command, at 'next' */
	    cmd = *last_cmdp;
	    addr = (db_expr_t)db_next;
	    have_addr = FALSE;
	    count = 1;
	    modif[0]  = '\0';
	    if (t == tSEMI_COLON)
		db_unread_token(t);
	}
	else if (t == tEXCL) {
	    db_fncall();
	    return;
	}
	else if (t != tIDENT) {
	    db_printf("?\n");
	    db_flush_lex();
	    return;
	}
	else {
	    /*
	     * Search for command
	     */
	    while (cmd_table) {
		result = db_cmd_search(db_tok_string,
				       cmd_table,
				       &cmd);
		switch (result) {
		    case CMD_NONE:
			if (db_exec_macro(db_tok_string) == 0)
			    return;
			db_printf("No such command \"%s\"\n", db_tok_string);
			db_flush_lex();
			return;
		    case CMD_AMBIGUOUS:
			db_printf("Ambiguous\n");
			db_flush_lex();
			return;
		    case CMD_HELP:
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    default:
			break;
		}
		if ((cmd_table = cmd->more) != 0) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    }
		}
	    }

	    if ((cmd->flag & CS_OWN) == 0) {
		/*
		 * Standard syntax:
		 * command [/modifier] [addr] [,count]
		 */
		t = db_read_token();
		if (t == tSLASH) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_printf("Bad modifier \"/%s\"\n", db_tok_string);
			db_flush_lex();
			return;
		    }
		    db_strcpy(modif, db_tok_string);
		}
		else {
		    db_unread_token(t);
		    modif[0] = '\0';
		}

		if (db_expression(&addr)) {
		    db_dot = (db_addr_t) addr;
		    db_last_addr = db_dot;
		    have_addr = TRUE;
		}
		else {
		    addr = (db_expr_t) db_dot;
		    have_addr = FALSE;
		}
		t = db_read_token();
		if (t == tCOMMA) {
		    if (!db_expression(&count)) {
			db_printf("Count missing after ','\n");
			db_flush_lex();
			return;
		    }
		}
		else {
		    db_unread_token(t);
		    count = -1;
		}
	    }
	}
	*last_cmdp = cmd;
	if (cmd != 0) {
	    /*
	     * Execute the command.
	     */
	    (*cmd->fcn)(addr, have_addr, count, modif);

	    if (cmd->flag & CS_SET_DOT) {
		/*
		 * If command changes dot, set dot to
		 * previous address displayed (if 'ed' style).
		 */
		if (db_ed_style) {
		    db_dot = db_prev;
		}
		else {
		    db_dot = db_next;
		}
	    }
	    else {
		/*
		 * If command does not change dot,
		 * set 'next' location to be the same.
		 */
		db_next = db_dot;
	    }
	}
}

void
db_command_list(
	struct db_command	**last_cmdp,	/* IN_OUT */
	struct db_command	*cmd_table)
{
	do {
	    db_command(last_cmdp, cmd_table);
	    db_skip_to_eol();
	} while (db_read_token() == tSEMI_COLON && db_cmd_loop_done == FALSE);
}

struct db_command db_show_all_cmds[] = {
	{ "tasks",	db_show_all_tasks,	0,	0 },
	{ "threads",	db_show_all_threads,	0,	0 },
	{ "slocks",	db_show_all_slocks,	0,	0 },
	{ (char *)0 }
};

struct db_command db_show_cmds[] = {
	{ "all",	0,			0,	db_show_all_cmds },
	{ "registers",	db_show_regs,		0,	0 },
	{ "breaks",	db_listbreak_cmd, 	0,	0 },
	{ "watches",	db_listwatch_cmd, 	0,	0 },
	{ "thread",	db_show_one_thread,	0,	0 },
	{ "task",	db_show_one_task,	0,	0 },
	{ "macro",	db_show_macro,		CS_OWN, 0 },
	{ "map",	vm_map_print,		0,	0 },
	{ "object",	vm_object_print,	0,	0 },
	{ "page",	vm_page_print,		0,	0 },
	{ "copy",	vm_map_copy_print,	0,	0 },
	{ "port",	ipc_port_print,		0,	0 },
	{ "pset",	ipc_pset_print,		0,	0 },
	{ "kmsg",	ipc_kmsg_print,		0,	0 },
	{ "msg",	ipc_msg_print,		0,	0 },
	{ "ipc_port",	db_show_port_id,	0,	0 },
	{ "slabinfo",	db_show_slab_info,	0,	0 },
	{ (char *)0, }
};

void
db_debug_all_traps_cmd(db_expr_t addr,
		int have_addr,
		db_expr_t count,
		const char *modif);
void
db_debug_port_references_cmd(db_expr_t addr,
			 int have_addr,
			 db_expr_t count,
			 const char *modif);

struct db_command db_debug_cmds[] = {
	{ "traps",		db_debug_all_traps_cmd,		0,	0 },
	{ "references",		db_debug_port_references_cmd,	0,	0 },
	{ (char *)0, }
};

struct db_command db_command_table[] = {
#ifdef DB_MACHINE_COMMANDS
  /* this must be the first entry, if it exists */
	{ "machine",    0,                      0,     		0},
#endif
	{ "print",	db_print_cmd,		CS_OWN,		0 },
	{ "examine",	db_examine_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "x",		db_examine_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "xf",		db_examine_forward,	CS_SET_DOT,	0 },
	{ "xb",		db_examine_backward,	CS_SET_DOT,	0 },
	{ "search",	db_search_cmd,		CS_OWN|CS_SET_DOT, 0 },
	{ "set",	db_set_cmd,		CS_OWN,		0 },
	{ "write",	db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "w",		db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "delete",	db_delete_cmd,		CS_OWN,		0 },
	{ "d",		db_delete_cmd,		CS_OWN,		0 },
	{ "break",	db_breakpoint_cmd,	CS_MORE,	0 },
	{ "dwatch",	db_deletewatch_cmd,	CS_MORE,	0 },
	{ "watch",	db_watchpoint_cmd,	CS_MORE,	0 },
	{ "step",	db_single_step_cmd,	0,		0 },
	{ "s",		db_single_step_cmd,	0,		0 },
	{ "continue",	db_continue_cmd,	0,		0 },
	{ "c",		db_continue_cmd,	0,		0 },
	{ "until",	db_trace_until_call_cmd,0,		0 },
	{ "next",	db_trace_until_matching_cmd,0,		0 },
	{ "match",	db_trace_until_matching_cmd,0,		0 },
	{ "trace",	db_stack_trace_cmd,	0,		0 },
	{ "cond",	db_cond_cmd,		CS_OWN,	 	0 },
	{ "call",	db_fncall,		CS_OWN,		0 },
	{ "macro",	db_def_macro_cmd,	CS_OWN,	 	0 },
	{ "dmacro",	db_del_macro_cmd,	CS_OWN,		0 },
	{ "show",	0,			0,	db_show_cmds },
	{ "debug",	0,			0,	db_debug_cmds },
	{ "reset",	db_reset_cpu,		0,		0 },
	{ "reboot",	db_reset_cpu,		0,		0 },
	{ "halt",	db_halt_cpu,		0,		0 },
	{ (char *)0, }
};

#ifdef DB_MACHINE_COMMANDS

/* this function should be called to install the machine dependent
   commands. It should be called before the debugger is enabled  */
void db_machine_commands_install(struct db_command *ptr)
{
  db_command_table[0].more = ptr;
  return;
}

#endif /* DB_MACHINE_COMMANDS */


struct db_command	*db_last_command = 0;

void
db_help_cmd(void)
{
	struct db_command *cmd = db_command_table;

	while (cmd->name != 0) {
	    db_printf("%-12s", cmd->name);
	    db_end_line();
	    cmd++;
	}
}

void
db_command_loop(void)
{
	jmp_buf_t db_jmpbuf;
	jmp_buf_t *prev = db_recover;
	extern int db_output_line;
	extern int db_macro_level;

	/*
	 * Initialize 'prev' and 'next' to dot.
	 */
	db_prev = db_dot;
	db_next = db_dot;

	db_cmd_loop_done = FALSE;
	while (!db_cmd_loop_done) {
	    (void) _setjmp(db_recover = &db_jmpbuf);
	    db_macro_level = 0;
	    if (db_print_position() != 0)
		db_printf("\n");
	    db_output_line = 0;
	    db_printf("db%s", (db_default_thread)? "t": "");
#if	NCPUS > 1
	    db_printf("{%d}", cpu_number());
#endif
	    db_printf("> ");

	    (void) db_read_line("!!");
	    db_command_list(&db_last_command, db_command_table);
	}

	db_recover = prev;
}

boolean_t
db_exec_cmd_nest(
	char *cmd,
	int  size)
{
	struct db_lex_context lex_context;

	db_cmd_loop_done = FALSE;
	if (cmd) {
	    db_save_lex_context(&lex_context);
	    db_switch_input(cmd, size /**OLD, &lex_context OLD**/);
	}
	db_command_list(&db_last_command, db_command_table);
	if (cmd)
	    db_restore_lex_context(&lex_context);
	return(db_cmd_loop_done == FALSE);
}

void db_error(s)
	const char *s;
{
	extern int db_macro_level;

	db_macro_level = 0;
	if (db_recover) {
	    if (s)
		db_printf(s);
	    db_flush_lex();
	    _longjmp(db_recover, 1);
	}
	else
	{
	    if (s)
	        db_printf(s);
	    panic("db_error");
	}
}

/*
 * Call random function:
 * !expr(arg,arg,arg)
 */
void
db_fncall(void)
{
	db_expr_t	fn_addr;
#define	MAXARGS		11
	db_expr_t	args[MAXARGS];
	int		nargs = 0;
	db_expr_t	retval;
	db_expr_t	(*func)();
	int		t;

	if (!db_expression(&fn_addr)) {
	    db_printf("Bad function \"%s\"\n", db_tok_string);
	    db_flush_lex();
	    return;
	}
	func = (db_expr_t (*) ()) fn_addr;

	t = db_read_token();
	if (t == tLPAREN) {
	    if (db_expression(&args[0])) {
		nargs++;
		while ((t = db_read_token()) == tCOMMA) {
		    if (nargs == MAXARGS) {
			db_printf("Too many arguments\n");
			db_flush_lex();
			return;
		    }
		    if (!db_expression(&args[nargs])) {
			db_printf("Argument missing\n");
			db_flush_lex();
			return;
		    }
		    nargs++;
		}
		db_unread_token(t);
	    }
	    if (db_read_token() != tRPAREN) {
		db_printf("?\n");
		db_flush_lex();
		return;
	    }
	}
	while (nargs < MAXARGS) {
	    args[nargs++] = 0;
	}

	retval = (*func)(args[0], args[1], args[2], args[3], args[4],
			 args[5], args[6], args[7], args[8], args[9] );
	db_printf(" %#N\n", retval);
}

boolean_t __attribute__ ((pure))
db_option(modif, option)
	const char	*modif;
	int		option;
{
	const char *p;

	for (p = modif; *p; p++)
	    if (*p == option)
		return(TRUE);
	return(FALSE);
}

void
db_debug_all_traps_cmd(db_expr_t addr,
		       int have_addr,
		       db_expr_t count,
		       const char *modif)
{
  if (strcmp (modif, "on") == 0)
    db_debug_all_traps (TRUE);
  else if (strcmp (modif, "off") == 0)
    db_debug_all_traps (FALSE);
  else
    db_error ("debug traps /on|/off\n");
}

void
db_debug_port_references_cmd(db_expr_t addr,
			     int have_addr,
			     db_expr_t count,
			     const char *modif)
{
  if (strcmp (modif, "on") == 0)
    db_debug_port_references (TRUE);
  else if (strcmp (modif, "off") == 0)
    db_debug_port_references (FALSE);
  else
    db_error ("debug references /on|/off\n");
}

#endif /* MACH_KDB */
