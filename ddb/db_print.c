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
 *	Date:	7/90
 */

#if MACH_KDB

/*
 * Miscellaneous printing.
 */
#include <string.h>
#include <mach/policy.h>
#include <mach/port.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/processor.h>
#include <kern/smp.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <machine/db_interface.h>
#include <machine/db_machdep.h>
#include <machine/thread.h>

#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_lex.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <ddb/db_task_thread.h>
#include <ddb/db_print.h>

extern unsigned long	db_maxoff;

/* ARGSUSED */
void
db_show_regs(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	char		*modif)
{
	struct 		db_variable *regp;
	db_expr_t	value;
	db_addr_t	offset;
	char *		name;
	int 		i;
	struct db_var_aux_param aux_param;
	task_t		task = TASK_NULL;

	aux_param.modif = modif;
	aux_param.thread = THREAD_NULL;
	if (db_option(modif, 't')) {
	    if (have_addr) {
		if (!db_check_thread_address_valid((thread_t)addr))
		    return;
		aux_param.thread = (thread_t)addr;
	    } else
	        aux_param.thread = db_default_thread;
	    if (aux_param.thread != THREAD_NULL)
		task = aux_param.thread->task;
	}
	for (regp = db_regs; regp < db_eregs; regp++) {
	    if (regp->max_level > 1) {
		db_printf("bad multi-suffixed register %s\n", regp->name);
		continue;
	    }
	    aux_param.level = regp->max_level;
	    for (i = regp->low; i <= regp->high; i++) {
		aux_param.suffix[0] = i;
	        db_read_write_variable(regp, &value, DB_VAR_GET, &aux_param);
		if (regp->max_level > 0)
		    db_printf("%s%d%*s", regp->name, i,
				12-strlen(regp->name)-((i<10)?1:2), "");
		else
		    db_printf("%-12s", regp->name);
		db_printf("%#*N", 2+2*sizeof(vm_offset_t), value);
		db_find_xtrn_task_sym_and_offset((db_addr_t)value, &name,
							&offset, task);
		if (name != 0 && offset <= db_maxoff && offset != value) {
		    db_printf("\t%s", name);
		    if (offset != 0)
			db_printf("+%#r", offset);
	    	}
		db_printf("\n");
	    }
	}
}

#define OPTION_LONG		0x001		/* long print option */
#define OPTION_USER		0x002		/* print ps-like stuff */
#define OPTION_SCHED		0x004		/* print scheduling info */
#define OPTION_INDENT		0x100		/* print with indent */
#define OPTION_THREAD_TITLE	0x200		/* print thread title */
#define OPTION_TASK_TITLE	0x400		/* print thread title */

#ifndef	DB_TASK_NAME
#define DB_TASK_NAME(task)			/* no task name */
#define DB_TASK_NAME_TITLE	""		/* no task name */
#endif	/* DB_TASK_NAME */

#ifndef	db_thread_fp_used
#define db_thread_fp_used(thread)	FALSE
#endif

static char *
db_thread_stat(
	const thread_t 	thread,
	char	 	*status)
{
	char *p = status;

	*p++ = (thread->state & TH_RUN)  ? 'R' : '.';
	*p++ = (thread->state & TH_WAIT) ? 'W' : '.';
	*p++ = (thread->state & TH_SUSP) ? 'S' : '.';
	*p++ = (thread->state & TH_SWAPPED) ? 'O' : '.';
	*p++ = (thread->state & TH_UNINT) ? 'N' : '.';
	/* show if the FPU has been used */
	*p++ = db_thread_fp_used(thread) ? 'F' : '.';
	*p++ = 0;
	return(status);
}

void
db_print_thread(
	thread_t thread,
	int	 thread_id,
	int	 flag)
{
	if (flag & OPTION_USER) {
	    char status[8];
	    char *indent = "";
	    if (flag & OPTION_INDENT)
	      indent = "    ";

	    if (flag & OPTION_LONG) {
		if (flag & OPTION_THREAD_TITLE) {
		    db_printf("%s ID: THREAD   STAT   STACK    PCB", indent);
		    db_printf("      SUS PRI CONTINUE,WAIT_FUNC\n");
		}
		db_printf("%s%3d%c %0*X %s %0*X %0*X %3d %3d ",
		    indent, thread_id,
		    (thread == current_thread())? '#': ':',
		    2*sizeof(vm_offset_t), thread,
		    db_thread_stat(thread, status),
		    2*sizeof(vm_offset_t), thread->kernel_stack,
		    2*sizeof(vm_offset_t), thread->pcb,
		    thread->suspend_count, thread->sched_pri);
		if ((thread->state & TH_SWAPPED) && thread->swap_func) {
		    db_task_printsym((db_addr_t)thread->swap_func,
				     DB_STGY_ANY, kernel_task);
		    db_printf(", ");
		}
		if (thread->state & TH_WAIT)
		    db_task_printsym((db_addr_t)thread->wait_event,
				     DB_STGY_ANY, kernel_task);
		db_printf("\n");
	    } else if (flag & OPTION_SCHED) {
		if (flag & OPTION_THREAD_TITLE) {
		    db_printf("%s     "
			      "STAT    PRIORITY            POLICY   USAGE                 LAST\n",
			      indent);
		    db_printf("%s ID: "
			      "RWSONF  SET  MAX COMP DEPR  P DATA   CPU        SCHED      UPDATED\n",
			      indent);
		    db_printf(" \n");
		}
		db_printf("%s%3d%c %s %4d %4d %4d %4d  %c %4d  %10d %10d %10d\n",
			  indent, thread_id,
			  (thread == current_thread())? '#': ':',
			  db_thread_stat(thread, status),
			  thread->priority,
			  thread->max_priority,
			  thread->sched_pri,
			  thread->depress_priority,
#if	MACH_FIXPRI
			  thread->policy == POLICY_TIMESHARE ? 'T' : 'F',
			  thread->sched_data,
#else	/* MACH_FIXPRI */
			  'T', 0,
#endif	/* MACH_FIXPRI */
			  thread->cpu_usage,
			  thread->sched_usage,
			  thread->sched_stamp);
	    } else {
		if (thread_id % 3 == 0) {
		    if (flag & OPTION_INDENT)
			db_printf("\n    ");
		} else
		    db_printf(" ");
		db_printf("%3d%c(%0*X,%s)", thread_id,
		    (thread == current_thread())? '#': ':',
		    2*sizeof(vm_offset_t), thread,
		    db_thread_stat(thread, status));
	    }
	} else {
	    if (flag & OPTION_INDENT)
		db_printf("            %3d ", thread_id);
	    if (thread->name[0] &&
		strncmp (thread->name, thread->task->name, THREAD_NAME_SIZE))
		db_printf("%s ", thread->name);
	    db_printf("(%0*X) ", 2*sizeof(vm_offset_t), thread);
	    char status[8];
	    db_printf("%s", db_thread_stat(thread, status));
	    if (thread->state & TH_SWAPPED) {
		if (thread->swap_func) {
		    db_printf("(");
		    db_task_printsym((db_addr_t)thread->swap_func,
				     DB_STGY_ANY, kernel_task);
		    db_printf(")");
		} else {
		    db_printf("(swapped)");
		}
	    }
	    if (thread->state & TH_WAIT) {
		db_printf(" ");
		db_task_printsym((db_addr_t)thread->wait_event,
			    DB_STGY_ANY, kernel_task);
	    }
	    db_printf("\n");
	}
}

static void
db_print_task(
	task_t	task,
	int	task_id,
	int	flag)
{
	thread_t thread;
	int thread_id;

	if (flag & OPTION_USER) {
	    if (flag & OPTION_TASK_TITLE) {
		db_printf(" ID: TASK     MAP      THD SUS PR %s",
			  DB_TASK_NAME_TITLE);
		if ((flag & (OPTION_LONG|OPTION_SCHED)) == 0)
		    db_printf("  THREADS");
		db_printf("\n");
	    }
	    db_printf("%3d: %0*X %0*X %3d %3d %2d ",
			    task_id, 2*sizeof(vm_offset_t), task,
			    2*sizeof(vm_offset_t), task->map, task->thread_count,
			    task->suspend_count, task->priority);
	    DB_TASK_NAME(task);
	    if (flag & (OPTION_LONG|OPTION_SCHED)) {
		if (flag & OPTION_TASK_TITLE)
		    flag |= OPTION_THREAD_TITLE;
		db_printf("\n");
	    } else if (task->thread_count <= 1)
		flag &= ~OPTION_INDENT;
	    thread_id = 0;
	    queue_iterate(&task->thread_list, thread, thread_t, thread_list) {
		db_print_thread(thread, thread_id, flag);
		flag &= ~OPTION_THREAD_TITLE;
		thread_id++;
	    }
	    if ((flag & (OPTION_LONG|OPTION_SCHED)) == 0)
		db_printf("\n");
	} else {
	    if (flag & OPTION_TASK_TITLE)
		db_printf("    TASK        THREADS\n");
	    if (task->name[0])
		db_printf("%3d %s (%0*X): ", task_id, task->name,
			  2*sizeof(vm_offset_t), task);
	    else
		db_printf("%3d (%0*X): ", task_id,
			  2*sizeof(vm_offset_t), task);
	    if (task->thread_count == 0) {
		db_printf("no threads\n");
	    } else {
		if (task->thread_count > 1) {
		    db_printf("%d threads: \n", task->thread_count);
		    flag |= OPTION_INDENT;
		} else
		    flag &= ~OPTION_INDENT;
		thread_id = 0;
		queue_iterate(&task->thread_list, thread,
			      thread_t, thread_list)
		    db_print_thread(thread, thread_id++, flag);
	    }
	}
}

void
db_show_all_tasks(db_expr_t addr,
		  boolean_t have_addr,
		  db_expr_t count,
		  const char *modif)
{
	task_t task;
	int task_id = 0;
	processor_set_t pset;

	db_printf(" ID %-*s NAME [THREADS]\n", 2*sizeof(vm_offset_t), "TASK");

	queue_iterate(&all_psets, pset, processor_set_t, all_psets)
	    queue_iterate(&pset->tasks, task, task_t, pset_tasks) {
		db_printf("%3d %0*X %s [%d]\n",
			  task_id,
			  2*sizeof(vm_offset_t),
			  task,
			  task->name,
			  task->thread_count);
		task_id++;
	    }
}

static void showrq(run_queue_t rq)
{
	db_printf("count(%d) low(%d)\n", rq->count, rq->low);
}

/*ARGSUSED*/
void
db_show_all_runqs(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	const char *	modif)
{
	int i = 0;
	processor_set_t pset;

	queue_iterate(&all_psets, pset, processor_set_t, all_psets) {
		db_printf("Processor set #%d runq:\t", i);
		showrq(&pset->runq);
		i++;
	}
	for (i = 0; i < smp_get_numcpus(); i++) {
	    db_printf("Processor #%d runq:\t", i);
	    showrq(&cpu_to_processor(i)->runq);
	}
	db_printf("Stuck threads:\t%d", stuck_count);
}

/*ARGSUSED*/
void
db_show_all_threads(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	const char *	modif)
{
	task_t task;
	int task_id;
	int flag;
	processor_set_t pset;

	flag = OPTION_TASK_TITLE|OPTION_INDENT;
	if (db_option(modif, 'u'))
	    flag |= OPTION_USER;
	if (db_option(modif, 'l'))
	    flag |= OPTION_LONG;
	if (db_option(modif, 's'))
	    flag |= OPTION_SCHED;

	task_id = 0;
	queue_iterate(&all_psets, pset, processor_set_t, all_psets) {
	    queue_iterate(&pset->tasks, task, task_t, pset_tasks) {
		db_print_task(task, task_id, flag);
		flag &= ~OPTION_TASK_TITLE;
		task_id++;
	    }
	}
}

db_addr_t
db_task_from_space(
	ipc_space_t	space,
	int		*task_id)
{
	task_t task;
	int tid = 0;
	processor_set_t pset;

	queue_iterate(&all_psets, pset, processor_set_t, all_psets) {
	    queue_iterate(&pset->tasks, task, task_t, pset_tasks) {
		    if (task->itk_space == space) {
			    *task_id = tid;
			    return (db_addr_t)task;
		    }
		    tid++;
	    }
	}
	*task_id = 0;
	return (0);
}

/*ARGSUSED*/
void
db_show_one_thread(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	const char *	modif)
{
	int		flag;
	int		thread_id;
	thread_t	thread;

	flag = OPTION_THREAD_TITLE;
	if (db_option(modif, 'u'))
	    flag |= OPTION_USER;
	if (db_option(modif, 'l'))
	    flag |= OPTION_LONG;
	if (db_option(modif, 's'))
	    flag |= OPTION_SCHED;

	if (!have_addr) {
	    thread = current_thread();
	    if (thread == THREAD_NULL) {
		db_error("No thread\n");
		/*NOTREACHED*/
	    }
	} else
	    thread = (thread_t) addr;

	if ((thread_id = db_lookup_thread(thread)) < 0) {
	    db_printf("bad thread address %#X\n", addr);
	    db_error(0);
	    /*NOTREACHED*/
	}

	if (flag & OPTION_USER) {
	    db_printf("TASK%d(%0*X):\n",
		      db_lookup_task(thread->task),
		      2*sizeof(vm_offset_t), thread->task);
	    db_print_thread(thread, thread_id, flag);
	} else {
	    db_printf("task %d(%0*X): thread %d",
		      db_lookup_task(thread->task),
		      2*sizeof(vm_offset_t), thread->task, thread_id);
	    db_print_thread(thread, thread_id, flag);
	}
}

/*ARGSUSED*/
void
db_show_one_task(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	const char *	modif)
{
	int		flag;
	int		task_id;
	task_t		task;

	flag = OPTION_TASK_TITLE;
	if (db_option(modif, 'u'))
	    flag |= OPTION_USER;
	if (db_option(modif, 'l'))
	    flag |= OPTION_LONG;

	if (!have_addr) {
	    task = db_current_task();
	    if (task == TASK_NULL) {
		db_error("No task\n");
		/*NOTREACHED*/
	    }
	} else
	    task = (task_t) addr;

	if ((task_id = db_lookup_task(task)) < 0) {
	    db_printf("bad task address %#X\n", addr);
	    db_error(0);
	    /*NOTREACHED*/
	}

	db_print_task(task, task_id, flag);
}

static int
db_port_iterate(const thread_t thread, void (*func)(int, const ipc_port_t, unsigned, int))
{
	ipc_entry_t entry;
	int n = 0;
	struct rdxtree_iter iter;
	rdxtree_for_each(&thread->task->itk_space->is_map, &iter, entry) {
	    if (entry->ie_bits & MACH_PORT_TYPE_PORT_RIGHTS)
		(*func)(entry->ie_name, (ipc_port_t) entry->ie_object,
			entry->ie_bits, n++);
	}
	return(n);
}

static void
db_print_port_id(int id, const ipc_port_t port, unsigned bits, int n)
{
	if (n != 0 && n % 3 == 0)
	    db_printf("\n");
	db_printf("\tport%d(%s,%x)", id,
		(bits & MACH_PORT_TYPE_RECEIVE)? "r":
		(bits & MACH_PORT_TYPE_SEND)? "s": "S", port);
}

static void
db_print_port_id_long(
	int id,
	const ipc_port_t port,
	unsigned bits,
	int n)
{
	if (n != 0)
	    db_printf("\n");
	db_printf("\tport%d(%s, port=0x%x", id,
		(bits & MACH_PORT_TYPE_RECEIVE)? "r":
		(bits & MACH_PORT_TYPE_SEND)? "s": "S", port);
	db_printf(", receiver_name=0x%x)", port->ip_receiver_name);
}

/* ARGSUSED */
void
db_show_port_id(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	const char *	modif)
{
	thread_t thread;

	if (!have_addr) {
	    thread = current_thread();
	    if (thread == THREAD_NULL) {
		db_error("No thread\n");
		/*NOTREACHED*/
	    }
	} else
	    thread = (thread_t) addr;
	if (db_lookup_thread(thread) < 0) {
	    db_printf("Bad thread address %#X\n", addr);
	    db_error(0);
	    /*NOTREACHED*/
	}
	if (db_option(modif, 'l'))
	  {
	    if (db_port_iterate(thread, db_print_port_id_long))
	      db_printf("\n");
	    return;
	  }
	if (db_port_iterate(thread, db_print_port_id))
	    db_printf("\n");
}

#endif /* MACH_KDB */
