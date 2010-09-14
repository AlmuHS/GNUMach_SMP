/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 * Hardware trap/fault handler.
 */

#include <sys/types.h>
#include <string.h>

#include <mach/machine/eflags.h>
#include <i386/trap.h>
#include <i386/fpu.h>
#include <i386/model_dep.h>
#include <intel/read_fault.h>
#include <machine/machspl.h>	/* for spl_t */

#include <mach/exception.h>
#include <mach/kern_return.h>
#include "vm_param.h"
#include <mach/machine/thread_status.h>

#include <vm/vm_fault.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <kern/ast.h>
#include <kern/debug.h>
#include <kern/printf.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>

#if MACH_KDB
#include <ddb/db_run.h>
#include <ddb/db_watch.h>
#endif

#include "debug.h"

extern void exception() __attribute__ ((noreturn));
extern void thread_exception_return() __attribute__ ((noreturn));

extern void i386_exception()  __attribute__ ((noreturn));

#if	MACH_KDB
boolean_t	debug_all_traps_with_kdb = FALSE;
extern struct db_watchpoint *db_watchpoint_list;
extern boolean_t db_watchpoints_inserted;

void
thread_kdb_return()
{
	register thread_t thread = current_thread();
	register struct i386_saved_state *regs = USER_REGS(thread);

	if (kdb_trap(regs->trapno, regs->err, regs)) {
		thread_exception_return();
		/*NOTREACHED*/
	}
}
#endif	/* MACH_KDB */

#if	MACH_TTD
extern boolean_t kttd_enabled;
boolean_t debug_all_traps_with_kttd = TRUE;
#endif	/* MACH_TTD */

void
user_page_fault_continue(kr)
	kern_return_t kr;
{
	register thread_t thread = current_thread();
	register struct i386_saved_state *regs = USER_REGS(thread);

	if (kr == KERN_SUCCESS) {
#if	MACH_KDB
		if (db_watchpoint_list &&
		    db_watchpoints_inserted &&
		    (regs->err & T_PF_WRITE) &&
		    db_find_watchpoint(thread->task->map,
				       (vm_offset_t)regs->cr2,
				       regs))
			kdb_trap(T_WATCHPOINT, 0, regs);
#endif	/* MACH_KDB */
		thread_exception_return();
		/*NOTREACHED*/
	}

#if	MACH_KDB
	if (debug_all_traps_with_kdb &&
	    kdb_trap(regs->trapno, regs->err, regs)) {
		thread_exception_return();
		/*NOTREACHED*/
	}
#endif	/* MACH_KDB */

	i386_exception(EXC_BAD_ACCESS, kr, regs->cr2);
	/*NOTREACHED*/
}

/*
 * Fault recovery in copyin/copyout routines.
 */
struct recovery {
	int	fault_addr;
	int	recover_addr;
};

extern struct recovery	recover_table[];
extern struct recovery	recover_table_end[];

/*
 * Recovery from Successful fault in copyout does not
 * return directly - it retries the pte check, since
 * the 386 ignores write protection in kernel mode.
 */
extern struct recovery	retry_table[];
extern struct recovery	retry_table_end[];


static char *trap_type[] = {
	"Divide error",
	"Debug trap",
	"NMI",
	"Breakpoint",
	"Overflow",
	"Bounds check",
	"Invalid opcode",
	"No coprocessor",
	"Double fault",
	"Coprocessor overrun",
	"Invalid TSS",
	"Segment not present",
	"Stack bounds",
	"General protection",
	"Page fault",
	"(reserved)",
	"Coprocessor error"
};
#define TRAP_TYPES (sizeof(trap_type)/sizeof(trap_type[0]))

char *trap_name(unsigned int trapnum)
{
	return trapnum < TRAP_TYPES ? trap_type[trapnum] : "(unknown)";
}


boolean_t	brb = TRUE;

/*
 * Trap from kernel mode.  Only page-fault errors are recoverable,
 * and then only in special circumstances.  All other errors are
 * fatal.
 */
void kernel_trap(regs)
	register struct i386_saved_state *regs;
{
	int	code;
	int	subcode;
	register int	type;
	vm_map_t	map;
	kern_return_t	result;
	register thread_t	thread;
	extern char _start[], etext[];

	type = regs->trapno;
	code = regs->err;
	thread = current_thread();

#if 0
((short*)0xb8700)[0] = 0x0f00+'K';
((short*)0xb8700)[1] = 0x0f30+(type / 10);
((short*)0xb8700)[2] = 0x0f30+(type % 10);
#endif
#if 0
printf("kernel trap %d error %d\n", type, code);
dump_ss(regs);
#endif

	switch (type) {
	    case T_NO_FPU:
		fpnoextflt();
		return;

	    case T_FPU_FAULT:
		fpextovrflt();
		return;

	    case T_FLOATING_POINT_ERROR:
		fpexterrflt();
		return;

	    case T_PAGE_FAULT:

		/* Get faulting linear address */
		subcode = regs->cr2;
#if 0
		printf("kernel page fault at linear address %08x\n", subcode);
#endif

		/* If it's in the kernel linear address region,
		   convert it to a kernel virtual address
		   and use the kernel map to process the fault.  */
		if (subcode >= LINEAR_MIN_KERNEL_ADDRESS) {
#if 0
		printf("%08x in kernel linear address range\n", subcode);
#endif
			map = kernel_map;
			subcode = lintokv(subcode);
#if 0
		printf("now %08x\n", subcode);
#endif
			if (trunc_page(subcode) == 0
			    || (subcode >= (int)_start
				&& subcode < (int)etext)) {
				printf("Kernel page fault at address 0x%x, "
				       "eip = 0x%x\n",
				       subcode, regs->eip);
				goto badtrap;
			}
		} else {
			assert(thread);
			map = thread->task->map;
			if (map == kernel_map) {
				printf("kernel page fault at %08x:\n", subcode);
				dump_ss(regs);
				panic("kernel thread accessed user space!\n");
			}
		}

		/*
		 * Since the 386 ignores write protection in
		 * kernel mode, always try for write permission
		 * first.  If that fails and the fault was a
		 * read fault, retry with read permission.
		 */
		result = vm_fault(map,
				  trunc_page((vm_offset_t)subcode),
				  VM_PROT_READ|VM_PROT_WRITE,
				  FALSE,
				  FALSE,
				  (void (*)()) 0);
#if	MACH_KDB
		if (result == KERN_SUCCESS) {
		    /* Look for watchpoints */
		    if (db_watchpoint_list &&
			db_watchpoints_inserted &&
			(code & T_PF_WRITE) &&
			db_find_watchpoint(map,
				(vm_offset_t)subcode, regs))
			kdb_trap(T_WATCHPOINT, 0, regs);
		}
		else
#endif	/* MACH_KDB */
		if ((code & T_PF_WRITE) == 0 &&
		    result == KERN_PROTECTION_FAILURE)
		{
		    /*
		     *	Must expand vm_fault by hand,
		     *	so that we can ask for read-only access
		     *	but enter a (kernel)writable mapping.
		     */
		    result = intel_read_fault(map,
					  trunc_page((vm_offset_t)subcode));
		}

		if (result == KERN_SUCCESS) {
		    /*
		     * Certain faults require that we back up
		     * the EIP.
		     */
		    register struct recovery *rp;

		    for (rp = retry_table; rp < retry_table_end; rp++) {
			if (regs->eip == rp->fault_addr) {
			    regs->eip = rp->recover_addr;
			    break;
			}
		    }
		    return;
		}

		/*
		 * If there is a failure recovery address
		 * for this fault, go there.
		 */
		{
		    register struct recovery *rp;

		    for (rp = recover_table;
			 rp < recover_table_end;
			 rp++) {
			if (regs->eip == rp->fault_addr) {
			    regs->eip = rp->recover_addr;
			    return;
			}
		    }
		}

		/*
		 * Check thread recovery address also -
		 * v86 assist uses it.
		 */
		if (thread->recover) {
		    regs->eip = thread->recover;
		    thread->recover = 0;
		    return;
		}

		/*
		 * Unanticipated page-fault errors in kernel
		 * should not happen.
		 */
		/* fall through */

	    default:
	    badtrap:
	    	printf("Kernel ");
		if (type < TRAP_TYPES)
			printf("%s trap", trap_type[type]);
		else
			printf("trap %d", type);
		printf(", eip 0x%x\n", regs->eip);
#if	MACH_TTD
		if (kttd_enabled && kttd_trap(type, code, regs))
			return;
#endif	/* MACH_TTD */
#if	MACH_KDB
		if (kdb_trap(type, code, regs))
		    return;
#endif	/* MACH_KDB */
		splhigh();
		printf("kernel trap, type %d, code = %x\n",
			type, code);
		dump_ss(regs);
		panic("trap");
		return;
	}
}


/*
 *	Trap from user mode.
 *	Return TRUE if from emulated system call.
 */
int user_trap(regs)
	register struct i386_saved_state *regs;
{
	int	exc = 0;	/* Suppress gcc warning */
	int	code;
	int	subcode;
	register int	type;
	register thread_t thread = current_thread();

	if ((vm_offset_t)thread < phys_last_addr) {
		printf("user_trap: bad thread pointer 0x%p\n", thread);
		printf("trap type %d, code 0x%x, va 0x%x, eip 0x%x\n",
		       regs->trapno, regs->err, regs->cr2, regs->eip);
		asm volatile ("1: hlt; jmp 1b");
	}
#if 0
printf("user trap %d error %d sub %08x\n", type, code, subcode);
#endif

	type = regs->trapno;
	code = 0;
	subcode = 0;

#if 0
	((short*)0xb8700)[3] = 0x0f00+'U';
	((short*)0xb8700)[4] = 0x0f30+(type / 10);
	((short*)0xb8700)[5] = 0x0f30+(type % 10);
#endif
#if 0
	printf("user trap %d error %d\n", type, code);
	dump_ss(regs);
#endif

	switch (type) {

	    case T_DIVIDE_ERROR:
		exc = EXC_ARITHMETIC;
		code = EXC_I386_DIV;
		break;

	    case T_DEBUG:
#if	MACH_TTD
		if (kttd_enabled && kttd_in_single_step()) {
			if (kttd_trap(type, regs->err, regs))
				return 0;
		}
#endif	/* MACH_TTD */
#if	MACH_KDB
		if (db_in_single_step()) {
		    if (kdb_trap(type, regs->err, regs))
			return 0;
		}
#endif
		exc = EXC_BREAKPOINT;
		code = EXC_I386_SGL;
		break;

	    case T_INT3:
#if	MACH_TTD
		if (kttd_enabled && kttd_trap(type, regs->err, regs))
			return 0;
		break;
#endif	/* MACH_TTD */
#if	MACH_KDB
	    {
		boolean_t db_find_breakpoint_here();

		if (db_find_breakpoint_here(
			(current_thread())? current_thread()->task: TASK_NULL,
			regs->eip - 1)) {
		    if (kdb_trap(type, regs->err, regs))
			return 0;
		}
	    }
#endif
		exc = EXC_BREAKPOINT;
		code = EXC_I386_BPT;
		break;

	    case T_OVERFLOW:
		exc = EXC_ARITHMETIC;
		code = EXC_I386_INTO;
		break;

	    case T_OUT_OF_BOUNDS:
		exc = EXC_SOFTWARE;
		code = EXC_I386_BOUND;
		break;

	    case T_INVALID_OPCODE:
		exc = EXC_BAD_INSTRUCTION;
		code = EXC_I386_INVOP;
		break;

	    case T_NO_FPU:
	    case 32:		/* XXX */
		fpnoextflt();
		return 0;

	    case T_FPU_FAULT:
		fpextovrflt();
		return 0;

	    case 10:		/* invalid TSS == iret with NT flag set */
		exc = EXC_BAD_INSTRUCTION;
		code = EXC_I386_INVTSSFLT;
		subcode = regs->err & 0xffff;
		break;

	    case T_SEGMENT_NOT_PRESENT:
		exc = EXC_BAD_INSTRUCTION;
		code = EXC_I386_SEGNPFLT;
		subcode = regs->err & 0xffff;
		break;

	    case T_STACK_FAULT:
		exc = EXC_BAD_INSTRUCTION;
		code = EXC_I386_STKFLT;
		subcode = regs->err & 0xffff;
		break;

	    case T_GENERAL_PROTECTION:
		/* Check for an emulated int80 system call.
		   NetBSD-current and Linux use trap instead of call gate. */
		if (thread->task->eml_dispatch) {
			unsigned char opcode, intno;

			opcode = inst_fetch(regs->eip, regs->cs);
			intno  = inst_fetch(regs->eip+1, regs->cs);
			if (opcode == 0xcd && intno == 0x80) {
				regs->eip += 2;
				return 1;
			}
		}
		exc = EXC_BAD_INSTRUCTION;
		code = EXC_I386_GPFLT;
		subcode = regs->err & 0xffff;
		break;

	    case T_PAGE_FAULT:
		subcode = regs->cr2;
#if 0
		printf("user page fault at linear address %08x\n", subcode);
		dump_ss (regs);

#endif
		if (subcode >= LINEAR_MIN_KERNEL_ADDRESS)
			i386_exception(EXC_BAD_ACCESS, EXC_I386_PGFLT, subcode);
		(void) vm_fault(thread->task->map,
				trunc_page((vm_offset_t)subcode),
				(regs->err & T_PF_WRITE)
				  ? VM_PROT_READ|VM_PROT_WRITE
				  : VM_PROT_READ,
				FALSE,
				FALSE,
				user_page_fault_continue);
		/*NOTREACHED*/
		break;

#ifdef MACH_XEN
	    case 15:
		{
			static unsigned count = 0;
			count++;
			if (!(count % 10000))
				printf("%d 4gb segments accesses\n", count);
			if (count > 1000000) {
				printf("A million 4gb segment accesses, stopping reporting them.");
				if (hyp_vm_assist(VMASST_CMD_disable, VMASST_TYPE_4gb_segments_notify))
					panic("couldn't disable 4gb segments vm assist notify");
			}
			return 0;
		}
#endif

	    case T_FLOATING_POINT_ERROR:
		fpexterrflt();
		return 0;

	    default:
#if	MACH_TTD
		if (kttd_enabled && kttd_trap(type, regs->err, regs))
			return 0;
#endif	/* MACH_TTD */
#if	MACH_KDB
		if (kdb_trap(type, regs->err, regs))
		    return 0;
#endif	/* MACH_KDB */
		splhigh();
		printf("user trap, type %d, code = %x\n",
		       type, regs->err);
		dump_ss(regs);
		panic("trap");
		return 0;
	}

#if	MACH_TTD
	if (debug_all_traps_with_kttd && kttd_trap(type, regs->err, regs))
		return 0;
#endif	/* MACH_TTD */
#if	MACH_KDB
	if (debug_all_traps_with_kdb &&
	    kdb_trap(type, regs->err, regs))
		return 0;
#endif	/* MACH_KDB */

	i386_exception(exc, code, subcode);
	/*NOTREACHED*/
}

/*
 *	V86 mode assist for interrupt handling.
 */
boolean_t v86_assist_on = TRUE;
boolean_t v86_unsafe_ok = FALSE;
boolean_t v86_do_sti_cli = TRUE;
boolean_t v86_do_sti_immediate = FALSE;

#define	V86_IRET_PENDING 0x4000

int cli_count = 0;
int sti_count = 0;

/*
 * Handle AST traps for i386.
 * Check for delayed floating-point exception from
 * AT-bus machines.
 */
void
i386_astintr()
{
	int	mycpu = cpu_number();

	(void) splsched();	/* block interrupts to check reasons */
#ifndef	MACH_XEN
	if (need_ast[mycpu] & AST_I386_FP) {
	    /*
	     * AST was for delayed floating-point exception -
	     * FP interrupt occured while in kernel.
	     * Turn off this AST reason and handle the FPU error.
	     */
	    ast_off(mycpu, AST_I386_FP);
	    (void) spl0();

	    fpastintr();
	}
	else
#endif	/* MACH_XEN */
	{
	    /*
	     * Not an FPU trap.  Handle the AST.
	     * Interrupts are still blocked.
	     */
	    ast_taken();
	}
}

/*
 * Handle exceptions for i386.
 *
 * If we are an AT bus machine, we must turn off the AST for a
 * delayed floating-point exception.
 *
 * If we are providing floating-point emulation, we may have
 * to retrieve the real register values from the floating point
 * emulator.
 */
void
i386_exception(exc, code, subcode)
	int	exc;
	int	code;
	int	subcode;
{
	spl_t	s;

	/*
	 * Turn off delayed FPU error handling.
	 */
	s = splsched();
	ast_off(cpu_number(), AST_I386_FP);
	splx(s);

	exception(exc, code, subcode);
	/*NOTREACHED*/
}

#if	MACH_PCSAMPLE > 0
/*
 * return saved state for interrupted user thread
 */
unsigned
interrupted_pc(t)
	thread_t t;
{
	register struct i386_saved_state *iss;

 	iss = USER_REGS(t);
 	return iss->eip;
}
#endif	/* MACH_PCSAMPLE > 0*/
