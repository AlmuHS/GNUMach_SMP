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

#if MACH_KDB

#include <mach/boolean.h>
#include <machine/db_machdep.h>
#include <machine/db_interface.h>

#include <ddb/db_access.h>
#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_sym.h>
#include <ddb/db_task_thread.h>
#include <ddb/db_examine.h>
#include <ddb/db_expr.h>
#include <ddb/db_print.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/smp.h>
#include <mach/vm_param.h>
#include <vm/vm_map.h>

#define db_thread_to_task(thread)	((thread)? thread->task: TASK_NULL)

char		db_examine_format[TOK_STRING_SIZE] = "x";
int		db_examine_count = 1;
db_addr_t	db_examine_prev_addr = 0;
thread_t	db_examine_thread = THREAD_NULL;

/*
 * Examine (print) data.
 */
/*ARGSUSED*/
void
db_examine_cmd(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif)
{
	thread_t	thread;

	if (modif[0] != '\0')
	    db_strcpy(db_examine_format, modif);

	if (count == -1)
	    count = 1;
	db_examine_count = count;
	if (db_option(modif, 't'))
	  {
	    if (!db_get_next_thread(&thread, 0))
	      return;
	  }
	else
	  if (db_option(modif, 'u'))
	    thread = current_thread();
	  else
	    thread = THREAD_NULL;

	db_examine_thread = thread;
	db_examine((db_addr_t) addr, db_examine_format, count,
					db_thread_to_task(thread));
}

/* ARGSUSED */
void
db_examine_forward(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif)
{
	db_examine(db_next, db_examine_format, db_examine_count,
				db_thread_to_task(db_examine_thread));
}

/* ARGSUSED */
void
db_examine_backward(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif)
{

	db_examine(db_examine_prev_addr - (db_next - db_examine_prev_addr),
			 db_examine_format, db_examine_count,
				db_thread_to_task(db_examine_thread));
}

void
db_examine(
	db_addr_t	addr,
	const char *	fmt,	/* format string */
	int		count,	/* repeat count */
	task_t		task)
{
	int		c;
	db_expr_t	value;
	int		size;	/* in bytes */
	int		width;
	const char *	fp;

	db_examine_prev_addr = addr;
	while (--count >= 0) {
	    fp = fmt;
	    size = 4;
	    width = 4*size;
	    while ((c = *fp++) != 0) {
		switch (c) {
		    case 'b':
			size = 1;
			width = 4*size;
			break;
		    case 'h':
			size = 2;
			width = 4*size;
			break;
		    case 'l':
			size = 4;
			width = 4*size;
			break;
		    case 'q':
			size = 8;
			width = 4*size;
			break;
		    case 'a':	/* address */
		    case 'A':   /* function address */
			/* always forces a new line */
			if (db_print_position() != 0)
			    db_printf("\n");
			db_prev = addr;
			db_task_printsym(addr,
					(c == 'a')?DB_STGY_ANY:DB_STGY_PROC,
					task);
			db_printf(":\t");
			break;
		    case 'm':
			db_next = db_xcdump(addr, size, count + 1, task);
			return;
		    default:
			if (db_print_position() == 0) {
			    /* If we hit a new symbol, print it */
			    char *	name;
			    db_addr_t	off;

			    db_find_task_sym_and_offset(addr, &name, &off, task);
			    if (off == 0)
				db_printf("%s:\t", name);
			    else
				db_printf("\t\t");

			    db_prev = addr;
			}

			switch (c) {
			    case ',':   /* skip one unit w/o printing */
				addr += size;
				break;

			    case 'r':	/* signed, current radix */
				value = db_get_task_value(addr,size,TRUE,task);
				addr += size;
				db_printf("%-*R", width, value);
				break;
			    case 'x':	/* unsigned hex */
				value = db_get_task_value(addr,size,FALSE,task);
				addr += size;
				db_printf("%-*X", width, value);
				break;
			    case 'z':	/* signed hex */
				value = db_get_task_value(addr,size,TRUE,task);
				addr += size;
				db_printf("%-*Z", width, value);
				break;
			    case 'd':	/* signed decimal */
				value = db_get_task_value(addr,size,TRUE,task);
				addr += size;
				db_printf("%-*D", width, value);
				break;
			    case 'U':	/* unsigned decimal */
				value = db_get_task_value(addr,size,FALSE,task);
				addr += size;
				db_printf("%-*U", width, value);
				break;
			    case 'o':	/* unsigned octal */
				value = db_get_task_value(addr,size,FALSE,task);
				addr += size;
				db_printf("%-*O", width, value);
				break;
			    case 'c':	/* character */
				value = db_get_task_value(addr,1,FALSE,task);
				addr += 1;
				if (value >= ' ' && value <= '~')
				    db_printf("%c", value);
				else
				    db_printf("\\%03o", value);
				break;
			    case 's':	/* null-terminated string */
				for (;;) {
				    value = db_get_task_value(addr,1,FALSE,task);
				    addr += 1;
				    if (value == 0)
					break;
				    if (value >= ' ' && value <= '~')
					db_printf("%c", value);
				    else
					db_printf("\\%03o", value);
				}
				break;
			    case 'i':	/* instruction */
				addr = db_disasm(addr, FALSE, task);
				break;
			    case 'I':	/* instruction, alternate form */
				addr = db_disasm(addr, TRUE, task);
				break;
			    default:
				break;
			}
			if (db_print_position() != 0)
			    db_end_line();
			break;
		}
	    }
	}
	db_next = addr;
}

/*
 * Find out what this address may be
 */
/*ARGSUSED*/
void
db_whatis_cmd(
	db_expr_t	addr,
	int		have_addr,
	db_expr_t	count,
	const char *	modif)
{
	/* TODO: Add whatever you can think of */

	int i;

	{
	    /* tasks */

	    task_t task;
	    int task_id = 0;
	    processor_set_t pset;
	    thread_t thread;
	    int thread_id;
	    vm_map_entry_t entry;

	    queue_iterate(&all_psets, pset, processor_set_t, all_psets)
		queue_iterate(&pset->tasks, task, task_t, pset_tasks) {
		    if (addr >= (vm_offset_t) task
			&& addr < (vm_offset_t) task + sizeof(*task))
			db_printf("%3d %0*X %s [%d]\n",
				  task_id,
				  2*sizeof(vm_offset_t),
				  task,
				  task->name,
				  task->thread_count);

		    if (addr >= (vm_offset_t) task->map
			&& addr < (vm_offset_t) task->map + sizeof(*(task->map)))
			db_printf("$map%d %X for $task%d %s\n",
				task_id, (vm_offset_t) task->map, task_id, task->name);

		    for (entry = vm_map_first_entry(task->map);
			 entry != vm_map_to_entry(task->map);
			 entry = entry->vme_next)
			if (addr >= (vm_offset_t) entry
				&& addr < (vm_offset_t) entry + sizeof(*entry))
			    db_printf("$map%d %X for $task%d %s entry 0x%X: ",
				    task_id, (vm_offset_t) task->map, task_id, task->name,
				    (vm_offset_t) entry);

		    if (pmap_whatis(task->map->pmap, addr))
			db_printf(" in $task%d %s\n", task_id, task->name);

		    if ((task == current_task() || task == kernel_task)
			&& addr >= vm_map_min(task->map)
			&& addr < vm_map_max(task->map)) {
			    db_printf("inside $map%d of $task%d %s\n", task_id, task_id, task->name);

			    for (entry = vm_map_first_entry(task->map);
				 entry != vm_map_to_entry(task->map);
				 entry = entry->vme_next)
				if (addr >= entry->vme_start
					&& addr < entry->vme_end) {
				    db_printf(" entry 0x%X: ", (vm_offset_t) entry);
				    if (entry->is_sub_map)
					db_printf("submap=0x%X, offset=0x%X\n",
						(vm_offset_t) entry->object.sub_map,
						(vm_offset_t) entry->offset);
				    else
					db_printf("object=0x%X, offset=0x%X\n",
						(vm_offset_t) entry->object.vm_object,
						(vm_offset_t) entry->offset);
				}
		    }

		    thread_id = 0;
		    queue_iterate(&task->thread_list, thread, thread_t, thread_list) {
			if (addr >= (vm_offset_t) thread
			    && addr < (vm_offset_t) thread + sizeof(*thread)) {
			    db_printf("In $task%d %s\n", task_id, task->name);
			    db_print_thread(thread, thread_id, 0);
			}
			if (addr >= thread->kernel_stack
				&& addr < thread->kernel_stack + KERNEL_STACK_SIZE) {
			    db_printf("In $task%d %s\n", task_id, task->name);
			    db_printf("  on stack of $thread%d.%d\n", task_id, thread_id);
			    db_print_thread(thread, thread_id, 0);
			}
			thread_id++;
		    }
		    task_id++;
		}
	}

	pmap_whatis(kernel_pmap, addr);

	{
	    /* runqs */
	    if (addr >= (vm_offset_t) &default_pset.runq
		&& addr < (vm_offset_t) &default_pset.runq + sizeof(default_pset.runq))
		db_printf("default runq %p\n", &default_pset.runq);
	    for (i = 0; i < smp_get_numcpus(); i++) {
		processor_t proc = cpu_to_processor(i);
		if (addr >= (vm_offset_t) &proc->runq
		    && addr < (vm_offset_t) &proc->runq + sizeof(proc->runq))
		    db_printf("Processor #%d runq %p\n", &proc->runq);
	    }
	}

	{
	    /* stacks */
	    for (i = 0; i < smp_get_numcpus(); i++) {
		if (addr >= percpu_array[i].active_stack
			&& addr < percpu_array[i].active_stack + KERNEL_STACK_SIZE)
		    db_printf("Processor #%d active stack\n", i);
	    }
	}

	db_whatis_slab(addr);

	{
	    /* page */
	    phys_addr_t pa;
	    if (DB_VALID_KERN_ADDR(addr))
		pa = kvtophys(addr);
	    else
		pa = pmap_extract(current_task()->map->pmap, addr);

	    if (pa) {
		struct vm_page *page = vm_page_lookup_pa(pa);
		db_printf("phys %llx, page %p\n", (unsigned long long) pa, page);
		if (page) {
		    const char *types[] = {
			    [VM_PT_FREE] =	"free",
			    [VM_PT_RESERVED] =	"reserved",
			    [VM_PT_TABLE] =	"table",
			    [VM_PT_KERNEL] =	"kernel",
		    };
		    db_printf("  %s\n", types[page->type]);
		    db_printf("  free %u\n", page->free);
		    db_printf("  external %u\n", page->external);
		    db_printf("  busy %u\n", page->busy);
		    db_printf("  private %u\n", page->private);
		    db_printf("  object %lx\n", page->object);
		    db_printf("  offset %lx\n", page->offset);
		    db_printf("  wired %u\n", page->wire_count);
		    db_printf("  segment %u\n", page->seg_index);
		    db_printf("  order %u\n", page->order);
		}
	    }
	}
}

/*
 * Print value.
 */
char	db_print_format = 'x';

/*ARGSUSED*/
void
db_print_cmd(void)
{
	db_expr_t	value;
	int		t;
	task_t		task = TASK_NULL;

	if ((t = db_read_token()) == tSLASH) {
	    if (db_read_token() != tIDENT) {
		db_printf("Bad modifier \"/%s\"\n", db_tok_string);
		db_error(0);
		/* NOTREACHED */
	    }
	    if (db_tok_string[0])
		db_print_format = db_tok_string[0];
	    if (db_option(db_tok_string, 't') && db_default_thread)
		task = db_default_thread->task;
	} else
	    db_unread_token(t);

	for ( ; ; ) {
	    t = db_read_token();
	    if (t == tSTRING) {
		db_printf("%s", db_tok_string);
		continue;
	    }
	    db_unread_token(t);
	    if (!db_expression(&value))
		break;
	    switch (db_print_format) {
	    case 'a':
		db_task_printsym((db_addr_t)value, DB_STGY_ANY, task);
		break;
	    case 'r':
		db_printf("%*r", 3+2*sizeof(db_expr_t), value);
		break;
	    case 'x':
		db_printf("%*x", 2*sizeof(db_expr_t), value);
		break;
	    case 'z':
		db_printf("%*z", 2*sizeof(db_expr_t), value);
		break;
	    case 'd':
		db_printf("%*d", 3+2*sizeof(db_expr_t), value);
		break;
	    case 'u':
		db_printf("%*u", 3+2*sizeof(db_expr_t), value);
		break;
	    case 'o':
		db_printf("%o", 4*sizeof(db_expr_t), value);
		break;
	    case 'c':
		value = value & 0xFF;
		if (value >= ' ' && value <= '~')
		    db_printf("%c", value);
		else
		    db_printf("\\%03o", value);
		break;
	    default:
		db_printf("Unknown format %c\n", db_print_format);
		db_print_format = 'x';
		db_error(0);
	    }
	}
}

void
db_print_loc_and_inst(
	db_addr_t	loc,
	task_t		task)
{
	db_task_printsym(loc, DB_STGY_PROC, task);
	db_printf(":\t");
	(void) db_disasm(loc, TRUE, task);
}

void
db_strcpy(char *dst, const char *src)
{
	while ((*dst++ = *src++))
	    ;
}

/*
 * Search for a value in memory.
 * Syntax: search [/bhl] addr value [mask] [,count] [thread]
 */
void
db_search_cmd(
	db_expr_t e,
	boolean_t b,
	db_expr_t e2,
	const char * cc)
{
	int		t;
	db_addr_t	addr;
	int		size = 0;
	db_expr_t	value;
	db_expr_t	mask;
	db_addr_t	count;
	thread_t	thread;
	boolean_t	thread_flag = FALSE;
	char		*p;

	t = db_read_token();
	if (t == tSLASH) {
	    t = db_read_token();
	    if (t != tIDENT) {
	      bad_modifier:
		db_printf("Bad modifier \"/%s\"\n", db_tok_string);
		db_flush_lex();
		return;
	    }

	    for (p = db_tok_string; *p; p++) {
		switch(*p) {
		case 'b':
		    size = sizeof(char);
		    break;
		case 'h':
		    size = sizeof(short);
		    break;
		case 'l':
		    size = sizeof(long);
		    break;
		case 't':
		    thread_flag = TRUE;
		    break;
		default:
		    goto bad_modifier;
		}
	    }
	} else {
	    db_unread_token(t);
	    size = sizeof(int);
	}

	if (!db_expression((db_expr_t *)&addr)) {
	    db_printf("Address missing\n");
	    db_flush_lex();
	    return;
	}

	if (!db_expression(&value)) {
	    db_printf("Value missing\n");
	    db_flush_lex();
	    return;
	}

	if (!db_expression(&mask))
	    mask = ~0;

	t = db_read_token();
	if (t == tCOMMA) {
	    if (!db_expression((db_expr_t *)&count)) {
		db_printf("Count missing\n");
		db_flush_lex();
		return;
	    }
	} else {
	    db_unread_token(t);
	    count = -1;		/* effectively forever */
	}
	if (thread_flag) {
	    if (!db_get_next_thread(&thread, 0))
		return;
	} else
	    thread = THREAD_NULL;

	db_search(addr, size, value, mask, count, db_thread_to_task(thread));
}

void
db_search(
	db_addr_t	addr,
	int		size,
	db_expr_t	value,
	db_expr_t	mask,
	unsigned int	count,
	task_t		task)
{
	while (count-- != 0) {
		db_prev = addr;
		if ((db_get_task_value(addr, size, FALSE, task) & mask) == value)
			break;
		addr += size;
	}
	db_next = addr;
}

#define DB_XCDUMP_NC	16

int
db_xcdump(
	db_addr_t	addr,
	int		size,
	int		count,
	task_t		task)
{
	int 		i, n;
	db_expr_t	value;
	int		bcount;
	db_addr_t	off;
	char		*name;
	char		data[DB_XCDUMP_NC];

	db_find_task_sym_and_offset(addr, &name, &off, task);
	for (n = count*size; n > 0; n -= bcount) {
	    db_prev = addr;
	    if (off == 0) {
		db_printf("%s:\n", name);
		off = -1;
	    }
	    db_printf("%0*X:%s", 2*sizeof(db_addr_t), addr,
	    		(size != 1)? " ": "");
	    bcount = ((n > DB_XCDUMP_NC)? DB_XCDUMP_NC: n);
	    if (trunc_page(addr) != trunc_page(addr+bcount-1)) {
		db_addr_t next_page_addr = trunc_page(addr+bcount-1);
		if (!DB_CHECK_ACCESS(next_page_addr, sizeof(int), task))
		    bcount = next_page_addr - addr;
	    }
	    if (!db_read_bytes(addr, bcount, data, task)) {
		db_printf("*\n");
		continue;
	    }
	    for (i = 0; i < bcount && off != 0; i += size) {
		if (i % 4 == 0)
			db_printf(" ");
		value = db_get_task_value(addr, size, FALSE, task);
		db_printf("%0*x ", size*2, value);
		addr += size;
		db_find_task_sym_and_offset(addr, &name, &off, task);
	    }
	    db_printf("%*s",
			((DB_XCDUMP_NC-i)/size)*(size*2+1)+(DB_XCDUMP_NC-i)/4,
			 "");
	    bcount = i;
	    db_printf("%s*", (size != 1)? " ": "");
	    for (i = 0; i < bcount; i++) {
		value = data[i];
		db_printf("%c", (value >= ' ' && value <= '~')? value: '.');
	    }
	    db_printf("*\n");
	}
	return(addr);
}

#endif /* MACH_KDB */
