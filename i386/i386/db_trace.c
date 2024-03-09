/*
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
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

#if MACH_KDB

#include <string.h>

#include <mach/boolean.h>
#include <vm/vm_map.h>
#include <kern/thread.h>
#include <kern/task.h>

#include <machine/db_machdep.h>
#include <machine/spl.h>
#include <machine/db_interface.h>
#include <machine/db_trace.h>
#include <machine/cpu_number.h>
#include <i386at/model_dep.h>

#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_task_thread.h>

#include "trap.h"

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
	{ "cs",	(long *)&ddb_regs.cs,  db_i386_reg_value },
#if !defined(__x86_64__) || defined(USER32)
	{ "ds",	(long *)&ddb_regs.ds,  db_i386_reg_value },
	{ "es",	(long *)&ddb_regs.es,  db_i386_reg_value },
	{ "fs",	(long *)&ddb_regs.fs,  db_i386_reg_value },
	{ "gs",	(long *)&ddb_regs.gs,  db_i386_reg_value },
#endif
	{ "ss",	(long *)&ddb_regs.ss,  db_i386_reg_value },
	{ "eax",(long *)&ddb_regs.eax, db_i386_reg_value },
	{ "ecx",(long *)&ddb_regs.ecx, db_i386_reg_value },
	{ "edx",(long *)&ddb_regs.edx, db_i386_reg_value },
	{ "ebx",(long *)&ddb_regs.ebx, db_i386_reg_value },
	{ "esp",(long *)&ddb_regs.uesp,db_i386_reg_value },
	{ "ebp",(long *)&ddb_regs.ebp, db_i386_reg_value },
	{ "esi",(long *)&ddb_regs.esi, db_i386_reg_value },
	{ "edi",(long *)&ddb_regs.edi, db_i386_reg_value },
	{ "eip",(long *)&ddb_regs.eip, db_i386_reg_value },
	{ "efl",(long *)&ddb_regs.efl, db_i386_reg_value },
#ifdef __x86_64__
	{ "r8", (long *)&ddb_regs.r8,  db_i386_reg_value },
	{ "r9", (long *)&ddb_regs.r9,  db_i386_reg_value },
	{ "r10",(long *)&ddb_regs.r10, db_i386_reg_value },
	{ "r11",(long *)&ddb_regs.r11, db_i386_reg_value },
	{ "r12",(long *)&ddb_regs.r12, db_i386_reg_value },
	{ "r13",(long *)&ddb_regs.r13, db_i386_reg_value },
	{ "r14",(long *)&ddb_regs.r14, db_i386_reg_value },
	{ "r15",(long *)&ddb_regs.r15, db_i386_reg_value },
#endif
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Stack trace.
 */
#define	INKERNEL(va)	(((vm_offset_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

struct i386_frame {
	struct i386_frame	*f_frame;
	long			f_retaddr;
	long			f_arg0;
};

#define	TRAP		1
#define	INTERRUPT	2
#define SYSCALL		3

db_addr_t	db_user_trap_symbol_value = 0;
db_addr_t	db_kernel_trap_symbol_value = 0;
db_addr_t	db_interrupt_symbol_value = 0;
db_addr_t	db_return_to_iret_symbol_value = 0;
db_addr_t	db_syscall_symbol_value = 0;
boolean_t	db_trace_symbols_found = FALSE;

struct i386_kregs {
	char	*name;
	long	offset;
} i386_kregs[] = {
	{ "ebx", (long)(&((struct i386_kernel_state *)0)->k_ebx) },
	{ "esp", (long)(&((struct i386_kernel_state *)0)->k_esp) },
	{ "ebp", (long)(&((struct i386_kernel_state *)0)->k_ebp) },
#ifdef __i386__
	{ "edi", (long)(&((struct i386_kernel_state *)0)->k_edi) },
	{ "esi", (long)(&((struct i386_kernel_state *)0)->k_esi) },
#endif
#ifdef __x86_64__
	{ "r12", (long)(&((struct i386_kernel_state *)0)->k_r12) },
	{ "r13", (long)(&((struct i386_kernel_state *)0)->k_r13) },
	{ "r14", (long)(&((struct i386_kernel_state *)0)->k_r14) },
	{ "r15", (long)(&((struct i386_kernel_state *)0)->k_r15) },
#endif
	{ "eip", (long)(&((struct i386_kernel_state *)0)->k_eip) },
	{ 0 },
};

static long *
db_lookup_i386_kreg(
	const char *name,
	const long *kregp)
{
	struct i386_kregs *kp;

	for (kp = i386_kregs; kp->name; kp++) {
	    if (strcmp(name, kp->name) == 0)
		return (long *)((long)kregp + kp->offset);
	}
	return 0;
}

void
db_i386_reg_value(
	struct	db_variable	*vp,
	db_expr_t		*valuep,
	int			flag,
	db_var_aux_param_t	ap)
{
	long			*dp = 0;
	db_expr_t		null_reg = 0;
	thread_t		thread = ap->thread;

	if (db_option(ap->modif, 'u')) {
	    if (thread == THREAD_NULL) {
		if ((thread = current_thread()) == THREAD_NULL)
		    db_error("no user registers\n");
	    }
	    if (thread == current_thread()) {
		if (ddb_regs.cs & 0x3)
		    dp = vp->valuep;
		else if (ON_INT_STACK(ddb_regs.ebp, cpu_number()))
		    db_error("cannot get/set user registers in nested interrupt\n");
	    }
	} else {
	    if (thread == THREAD_NULL || thread == current_thread()) {
		dp = vp->valuep;
	    } else if ((thread->state & TH_SWAPPED) == 0 &&
			thread->kernel_stack) {
		dp = db_lookup_i386_kreg(vp->name,
				(long *)(STACK_IKS(thread->kernel_stack)));
		if (dp == 0)
		    dp = &null_reg;
	    } else if ((thread->state & TH_SWAPPED) &&
			thread->swap_func != thread_exception_return) {
/*.....this breaks t/t $taskN.0...*/
		/* only EIP is valid */
		if (vp->valuep == (long *) &ddb_regs.eip) {
		    dp = (long *)(&thread->swap_func);
		} else {
		    dp = &null_reg;
		}
	    }
	}
	if (dp == 0) {
	    if (thread->pcb == 0)
		db_error("no pcb\n");
	    dp = (long *)((long)(&thread->pcb->iss) +
		    ((long)vp->valuep - (long)&ddb_regs));
	}
	if (flag == DB_VAR_SET)
	    *dp = *valuep;
	else
	    *valuep = *dp;
}

static void
db_find_trace_symbols(void)
{
	db_expr_t	value;
#ifdef __ELF__
#define P
#else
#define P "_"
#endif
	if (db_value_of_name(P"user_trap", &value))
	    db_user_trap_symbol_value = (db_addr_t) value;
	if (db_value_of_name(P"kernel_trap", &value))
	    db_kernel_trap_symbol_value = (db_addr_t) value;
	if (db_value_of_name(P"interrupt", &value))
	    db_interrupt_symbol_value = (db_addr_t) value;
	if (db_value_of_name(P"return_to_iret", &value))
	    db_return_to_iret_symbol_value = (db_addr_t) value;
	if (db_value_of_name(P"syscall", &value))
	    db_syscall_symbol_value = (db_addr_t) value;
#undef P
	db_trace_symbols_found = TRUE;
}

/*
 * Figure out how many arguments were passed into the frame at "fp".
 */
const int db_numargs_default = 5;

#ifdef __x86_64
/* Args are in registers */
#define db_numargs(fp, task) -1
#else
static int
db_numargs(
	struct i386_frame *fp,
	task_t task)
{
	long	*argp;
	long	inst;
	long	args;
	extern char	etext[];

	argp = (long *)db_get_task_value((long)&fp->f_retaddr, sizeof(long), FALSE, task);
	if (argp < (long *)VM_MIN_KERNEL_ADDRESS || argp > (long *)etext)
	    args = db_numargs_default;
	else if (!DB_CHECK_ACCESS((long)argp, sizeof(long), task))
	    args = db_numargs_default;
	else {
	    inst = db_get_task_value((long)argp, sizeof(long), FALSE, task);
	    if ((inst & 0xff) == 0x59)	/* popl %ecx */
		args = 1;
	    else if ((inst & 0xffff) == 0xc483)	/* addl %n, %esp */
		args = ((inst >> 16) & 0xff) / 4;
	    else
		args = db_numargs_default;
	}
	return args;
}
#endif

struct interrupt_frame {
	struct i386_frame *if_frame;	/* point to next frame */
	long		  if_retaddr;	/* return address to _interrupt */
	long		  if_unit;	/* unit number */
	spl_t		  if_spl;	/* saved spl */
	long		  if_iretaddr;	/* _return_to_{iret,iret_i} */
	long		  if_edx;	/* old sp(iret) or saved edx(iret_i) */
	long		  if_ecx;	/* saved ecx(iret_i) */
	long		  if_eax;	/* saved eax(iret_i) */
	long		  if_eip;	/* saved eip(iret_i) */
	long		  if_cs;	/* saved cs(iret_i) */
	long		  if_efl;	/* saved efl(iret_i) */
};

/*
 * Figure out the next frame up in the call stack.
 * For trap(), we print the address of the faulting instruction and
 *   proceed with the calling frame.  We return the ip that faulted.
 *   If the trap was caused by jumping through a bogus pointer, then
 *   the next line in the backtrace will list some random function as
 *   being called.  It should get the argument list correct, though.
 *   It might be possible to dig out from the next frame up the name
 *   of the function that faulted, but that could get hairy.
 */
static void
db_nextframe(
	struct i386_frame **lfp,	/* in/out */
	struct i386_frame **fp,		/* in/out */
	db_addr_t	  *sp,		/* out */
	db_addr_t	  *ip,		/* out */
	long 		  frame_type,	/* in */
	const thread_t	  thread)	/* in */
{
	struct i386_saved_state *saved_regs;
	struct interrupt_frame *ifp;
	task_t task = (thread != THREAD_NULL)? thread->task: TASK_NULL;

	switch(frame_type) {
	case TRAP:
	    /*
	     * We know that trap() has 1 argument and we know that
	     * it is an (struct i386_saved_state *).
	     */
	    saved_regs = (struct i386_saved_state *)
		db_get_task_value((long)&((*fp)->f_arg0),sizeof(long),FALSE,task);
	    db_printf(">>>>> %s (%d)",
			trap_name(saved_regs->trapno), saved_regs->trapno);
	    if (saved_regs->trapno == T_PAGE_FAULT)
		db_printf(" for %s%s%s %lx",
			saved_regs->err & T_PF_PROT ? "P" : "",
			saved_regs->err & T_PF_WRITE ? "W" : "",
			saved_regs->err & T_PF_USER ? "U" : "",
			lintokv(saved_regs->cr2));
	    db_printf(" at ");
	    db_task_printsym(saved_regs->eip, DB_STGY_PROC, task);
	    db_printf(" <<<<<\n");
	    *fp = (struct i386_frame *)saved_regs->ebp;
	    *sp = (db_addr_t)saved_regs->uesp;
	    *ip = (db_addr_t)saved_regs->eip;
	    break;
	case INTERRUPT:
	    if (*lfp == 0) {
		db_printf(">>>>> interrupt <<<<<\n");
		goto miss_frame;
	    }
	    db_printf(">>>>> interrupt at ");
	    ifp = (struct interrupt_frame *)(*lfp);
	    *fp = ifp->if_frame;
	    *sp = (db_addr_t) ifp->if_frame;
	    if (ifp->if_iretaddr == db_return_to_iret_symbol_value)
		*ip = ((struct i386_interrupt_state *) ifp->if_edx)->eip;
	    else
		*ip = (db_addr_t) ifp->if_eip;
	    db_task_printsym(*ip, DB_STGY_PROC, task);
	    db_printf(" <<<<<\n");
	    break;
	case SYSCALL:
	    if (thread != THREAD_NULL && thread->pcb) {
		*ip = (db_addr_t) thread->pcb->iss.eip;
		*sp = (db_addr_t) thread->pcb->iss.uesp;
		*fp = (struct i386_frame *) thread->pcb->iss.ebp;
		break;
	    }
	    /* falling down for unknown case */
	default:
	miss_frame:
	    *ip = (db_addr_t)
		db_get_task_value((long)&(*fp)->f_retaddr, sizeof(long), FALSE, task);
	    *lfp = *fp;
	    *fp = (struct i386_frame *)
		db_get_task_value((long)&(*fp)->f_frame, sizeof(long), FALSE, task);
	    *sp = (db_addr_t) *fp;
	    break;
	}
}

#define	F_USER_TRACE	1
#define F_TRACE_THREAD	2

void
db_stack_trace_cmd(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	const char	*modif)
{
	boolean_t	trace_thread = FALSE;
	struct i386_frame *frame;
	db_addr_t	callpc, sp;
	int		flags = 0;
	thread_t	th;

	{
	    const char *cp = modif;
	    char c;

	    while ((c = *cp++) != 0) {
		if (c == 't')
		    trace_thread = TRUE;
		if (c == 'u')
		    flags |= F_USER_TRACE;
	    }
	}

	if (!have_addr && !trace_thread) {
	    frame = (struct i386_frame *)ddb_regs.ebp;
	    sp = (db_addr_t)ddb_regs.uesp;
	    callpc = (db_addr_t)ddb_regs.eip;
	    th = current_thread();
	} else if (trace_thread) {
	    if (have_addr) {
		th = (thread_t) addr;
		if (!db_check_thread_address_valid(th))
		    return;
	    } else {
		th = db_default_thread;
		if (th == THREAD_NULL)
		   th = current_thread();
		if (th == THREAD_NULL) {
		   db_printf("no active thread\n");
		   return;
		}
	    }
	    if (th == current_thread()) {
	        frame = (struct i386_frame *)ddb_regs.ebp;
	        sp = (db_addr_t)ddb_regs.uesp;
	        callpc = (db_addr_t)ddb_regs.eip;
	    } else {
		if (th->pcb == 0) {
		    db_printf("thread has no pcb\n");
		    return;
		}
		if ((th->state & TH_SWAPPED) || th->kernel_stack == 0) {
		    struct i386_saved_state *iss = &th->pcb->iss;

		    db_printf("Continuation ");
		    db_task_printsym((db_addr_t)th->swap_func,
				      DB_STGY_PROC,
				      th->task);
		    db_printf("\n");

		    frame = (struct i386_frame *) (iss->ebp);
		    sp = (db_addr_t) (iss->uesp);
		    callpc = (db_addr_t) (iss->eip);
		} else {
		    struct i386_kernel_state *iks;
		    iks = STACK_IKS(th->kernel_stack);
		    frame = (struct i386_frame *) (iks->k_ebp);
		    sp = (db_addr_t) (iks->k_esp);
		    callpc = (db_addr_t) (iks->k_eip);
	        }
	    }
	} else {
	    frame = (struct i386_frame *)addr;
	    sp = (db_addr_t)addr;
	    th = (db_default_thread)? db_default_thread: current_thread();
	    callpc = (db_addr_t)db_get_task_value((long)&frame->f_retaddr, sizeof(long),
						  FALSE,
						  (th == THREAD_NULL) ? TASK_NULL : th->task);
	}

	db_i386_stack_trace( th, frame, sp, callpc, count, flags );
}


void
db_i386_stack_trace(
	const thread_t	th,
	struct i386_frame *frame,
	db_addr_t	sp,
	db_addr_t	callpc,
	db_expr_t	count,
	int		flags)
{
	task_t		task;
	boolean_t	kernel_only;
	long		*argp;
	long		user_frame = 0;
	struct i386_frame *lastframe;
	int		frame_type;
	char		*filename;
	int		linenum;
	extern unsigned	long db_maxoff;

	if (count == -1)
	    count = 65535;

	kernel_only = (flags & F_USER_TRACE) == 0;

	task = (th == THREAD_NULL) ? TASK_NULL : th->task;

	if (!db_trace_symbols_found)
	    db_find_trace_symbols();

	if (!INKERNEL(callpc) && !INKERNEL(frame)) {
	    db_printf(">>>>> user space <<<<<\n");
	    user_frame++;
	}

	lastframe = 0;
	while (count--) {
	    int 	narg;
	    char *	name;
	    db_expr_t	offset;

	    if (INKERNEL(callpc) && user_frame == 0) {
		db_addr_t call_func = 0;

		db_sym_t sym_tmp;
		db_symbol_values(0, 
				 sym_tmp = db_search_task_symbol(callpc,
								 DB_STGY_XTRN, 
								 (db_addr_t *)&offset,
								 TASK_NULL),
				 &name, (db_expr_t *)&call_func);
		db_free_symbol(sym_tmp);
		if ((db_user_trap_symbol_value && call_func == db_user_trap_symbol_value) ||
		    (db_kernel_trap_symbol_value && call_func == db_kernel_trap_symbol_value)) {
		    frame_type = TRAP;
		    narg = 1;
		} else if (db_interrupt_symbol_value && call_func == db_interrupt_symbol_value) {
		    frame_type = INTERRUPT;
		    goto next_frame;
		} else if (db_syscall_symbol_value && call_func == db_syscall_symbol_value) {
		    frame_type = SYSCALL;
		    goto next_frame;
		} else {
		    frame_type = 0;
		    if (frame)
			narg = db_numargs(frame, task);
		    else
			narg = -1;
		}
	    } else if (!frame || INKERNEL(callpc) ^ INKERNEL(frame)) {
		frame_type = 0;
		narg = -1;
	    } else {
		frame_type = 0;
		narg = db_numargs(frame, task);
	    }

	    db_find_task_sym_and_offset(callpc, &name,
					(db_addr_t *)&offset, task);
	    if (name == 0 || offset > db_maxoff) {
		db_printf("0x%x(", callpc);
		offset = 0;
	    } else
	        db_printf("%s(", name);

	    if (!frame) {
		db_printf(")\n");
	    }

	    if (sp) {
		unsigned char inst = db_get_task_value(callpc, sizeof(char), FALSE, task);
		if (inst == 0xc3) {
		    /* RET, unwind this directly */
		    callpc = db_get_task_value(sp, sizeof(callpc), FALSE, task);
		    sp += sizeof(callpc);
		    continue;
		}
	    }

	    if (!frame) {
		break;
	    }

	    argp = &frame->f_arg0;
	    while (narg > 0) {
		db_printf("%x", db_get_task_value((long)argp,sizeof(long),FALSE,task));
		argp++;
		if (--narg != 0)
		    db_printf(",");
	    }
	    if (narg < 0)
		db_printf("...");
	    db_printf(")");
	    if (offset) {
		db_printf("+0x%x", offset);
            }
	    if (db_line_at_pc(0, &filename, &linenum, callpc)) {
		db_printf(" [%s", filename);
		if (linenum > 0)
		    db_printf(":%d", linenum);
		db_printf("]");
	    }
	    db_printf("\n");

	next_frame:
	    db_nextframe(&lastframe, &frame, &sp, &callpc, frame_type, th);

	    if (!INKERNEL(lastframe) ||
		(!INKERNEL(callpc) && !INKERNEL(frame)))
		user_frame++;
	    if (user_frame == 1) {
		db_printf(">>>>> user space <<<<<\n");
		if (kernel_only)
		    break;
	    }
	    if (frame && frame <= lastframe) {
		if (INKERNEL(lastframe) && !INKERNEL(frame))
		    continue;
		db_printf("Bad frame pointer: 0x%x\n", frame);
		break;
	    }
	}
}

#endif /* MACH_KDB */
