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
/*
 * Interface to new debugger.
 */

#include <string.h>
#include <sys/reboot.h>
#include <vm/pmap.h>

#include <i386/thread.h>
#include <i386/db_machdep.h>
#include <i386/seg.h>
#include <i386/trap.h>
#include <i386/setjmp.h>
#include <i386/pmap.h>
#include <i386/proc_reg.h>
#include <i386/locore.h>
#include "gdt.h"
#include "trap.h"

#include "vm_param.h"
#include <vm/vm_map.h>
#include <vm/vm_fault.h>
#include <kern/cpu_number.h>
#include <kern/printf.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_run.h>
#include <ddb/db_task_thread.h>
#include <ddb/db_trap.h>
#include <ddb/db_watch.h>
#include <machine/db_interface.h>
#include <machine/machspl.h>

#if MACH_KDB
/* Whether the kernel uses any debugging register.  */
static boolean_t kernel_dr;
#endif
/* Whether the current debug registers are zero.  */
static boolean_t zero_dr;

void db_load_context(pcb_t pcb)
{
#if MACH_KDB
	int s = splhigh();

	if (kernel_dr) {
		splx(s);
		return;
	}
#endif
	/* Else set user debug registers, if any */
	unsigned int *dr = pcb->ims.ids.dr;
	boolean_t will_zero_dr = !dr[0] && !dr[1] && !dr[2] && !dr[3] && !dr[7];

	if (!(zero_dr && will_zero_dr))
	{
		set_dr0(dr[0]);
		set_dr1(dr[1]);
		set_dr2(dr[2]);
		set_dr3(dr[3]);
		set_dr7(dr[7]);
		zero_dr = will_zero_dr;
	}

#if MACH_KDB
	splx(s);
#endif
}

void db_get_debug_state(
	pcb_t pcb,
	struct i386_debug_state *state)
{
	*state = pcb->ims.ids;
}

kern_return_t db_set_debug_state(
	pcb_t pcb,
	const struct i386_debug_state *state)
{
	int i;

	for (i = 0; i <= 3; i++)
		if (state->dr[i] < VM_MIN_ADDRESS
	 	 || state->dr[i] >= VM_MAX_ADDRESS)
			return KERN_INVALID_ARGUMENT;

	pcb->ims.ids = *state;

	if (pcb == current_thread()->pcb)
		db_load_context(pcb);

	return KERN_SUCCESS;
}

#if MACH_KDB

struct	 i386_saved_state *i386_last_saved_statep;
struct	 i386_saved_state i386_nested_saved_state;
unsigned i386_last_kdb_sp;

extern	thread_t db_default_thread;

static struct i386_debug_state ids;

void db_dr (
	int		num,
	vm_offset_t	linear_addr,
	int		type,
	int		len,
	int		persistence)
{
	int s = splhigh();
	unsigned long dr7;

	if (!kernel_dr) {
	    if (!linear_addr) {
		splx(s);
		return;
	    }
	    kernel_dr = TRUE;
	    /* Clear user debugging registers */
	    set_dr7(0);
	    set_dr0(0);
	    set_dr1(0);
	    set_dr2(0);
	    set_dr3(0);
	}

	ids.dr[num] = linear_addr;
	switch (num) {
	    case 0: set_dr0(linear_addr); break;
	    case 1: set_dr1(linear_addr); break;
	    case 2: set_dr2(linear_addr); break;
	    case 3: set_dr3(linear_addr); break;
	}

	/* Replace type/len/persistence for DRnum in dr7 */
	dr7 = get_dr7 ();
	dr7 &= ~(0xfUL << (4*num+16)) & ~(0x3UL << (2*num));
	dr7 |= (((len << 2) | type) << (4*num+16)) | (persistence << (2*num));
	set_dr7 (dr7);

	if (kernel_dr) {
	    if (!ids.dr[0] && !ids.dr[1] && !ids.dr[2] && !ids.dr[3]) {
		/* Not used any more, switch back to user debugging registers */
		set_dr7 (0);
		kernel_dr = FALSE;
		zero_dr = TRUE;
		db_load_context(current_thread()->pcb);
	    }
	}
	splx(s);
}

boolean_t
db_set_hw_watchpoint(
	const db_watchpoint_t	watch,
	unsigned		num)
{
	vm_size_t	size = watch->hiaddr - watch->loaddr;
	db_addr_t	addr = watch->loaddr;
	vm_offset_t 	kern_addr;

	if (num >= 4)
	    return FALSE;
	if (size != 1 && size != 2 && size != 4)
	    return FALSE;

	if (addr & (size-1))
	    /* Unaligned */
	    return FALSE;

	if (watch->task) {
	    if (db_user_to_kernel_address(watch->task, addr, &kern_addr, 1) < 0)
		return FALSE;
	    addr = kern_addr;
	}
	addr = kvtolin(addr);

	db_dr (num, addr, I386_DB_TYPE_W, size-1, I386_DB_LOCAL|I386_DB_GLOBAL);

	db_printf("Hardware watchpoint %d set for %x\n", num, addr);
	return TRUE;
}

boolean_t
db_clear_hw_watchpoint(
	unsigned	num)
{
	if (num >= 4)
		return FALSE;

	db_dr (num, 0, 0, 0, 0);
	return TRUE;
}

/*
 * Print trap reason.
 */
void
kdbprinttrap(
	int	type, 
	int	code)
{
	printf("kernel: %s (%d), code=%x\n",
		trap_name(type), type, code);
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */

extern jmp_buf_t *db_recover;
spl_t saved_ipl[NCPUS];	/* just to know what was IPL before trap */

boolean_t
kdb_trap(
	int	type,
	int	code,
	struct i386_saved_state *regs)
{
	spl_t	s;

	s = splhigh();
	saved_ipl[cpu_number()] = s;

	switch (type) {
	    case T_DEBUG:	/* single_step */
	    {
		int addr;
	    	int status = get_dr6();

		if (status & 0xf) {	/* hmm hdw break */
			addr =	status & 0x8 ? get_dr3() :
				status & 0x4 ? get_dr2() :
				status & 0x2 ? get_dr1() :
					       get_dr0();
			regs->efl |= EFL_RF;
			db_single_step_cmd(addr, 0, 1, "p");
		}
	    }
	    case T_INT3:	/* breakpoint */
	    case T_WATCHPOINT:	/* watchpoint */
	    case -1:	/* keyboard interrupt */
		break;

	    default:
		if (db_recover) {
		    i386_nested_saved_state = *regs;
		    db_printf("Caught %s (%d), code = %x, pc = %x\n",
			trap_name(type), type, code, regs->eip);
		    db_error("");
		    /*NOTREACHED*/
		}
		kdbprinttrap(type, code);
	}

#if	NCPUS > 1
	if (db_enter())
#endif	/* NCPUS > 1 */
	{
	    i386_last_saved_statep = regs;
	    i386_last_kdb_sp = (unsigned) &type;

	    /* XXX Should switch to ddb`s own stack here. */

	    ddb_regs = *regs;
	    if ((regs->cs & 0x3) == KERNEL_RING) {
		/*
		 * Kernel mode - esp and ss not saved
		 */
		ddb_regs.uesp = (int)&regs->uesp;   /* kernel stack pointer */
		ddb_regs.ss   = KERNEL_DS;
	    }

	    cnpollc(TRUE);
	    db_task_trap(type, code, (regs->cs & 0x3) != 0);
	    cnpollc(FALSE);

	    regs->eip    = ddb_regs.eip;
	    regs->efl    = ddb_regs.efl;
	    regs->eax    = ddb_regs.eax;
	    regs->ecx    = ddb_regs.ecx;
	    regs->edx    = ddb_regs.edx;
	    regs->ebx    = ddb_regs.ebx;
	    if ((regs->cs & 0x3) != KERNEL_RING) {
		/*
		 * user mode - saved esp and ss valid
		 */
		regs->uesp = ddb_regs.uesp;		/* user stack pointer */
		regs->ss   = ddb_regs.ss & 0xffff;	/* user stack segment */
	    }
	    regs->ebp    = ddb_regs.ebp;
	    regs->esi    = ddb_regs.esi;
	    regs->edi    = ddb_regs.edi;
	    regs->es     = ddb_regs.es & 0xffff;
	    regs->cs     = ddb_regs.cs & 0xffff;
	    regs->ds     = ddb_regs.ds & 0xffff;
	    regs->fs     = ddb_regs.fs & 0xffff;
	    regs->gs     = ddb_regs.gs & 0xffff;

	    if ((type == T_INT3) &&
		(db_get_task_value(regs->eip, BKPT_SIZE, FALSE, TASK_NULL)
								 == BKPT_INST))
		regs->eip += BKPT_SIZE;
	}
#if	NCPUS > 1
	db_leave();
#endif	/* NCPUS > 1 */

	splx(s);
	return 1;
}

/*
 *	Enter KDB through a keyboard trap.
 *	We show the registers as of the keyboard interrupt
 *	instead of those at its call to KDB.
 */
struct int_regs {
	long	edi;
	long	esi;
	long	ebp;
	long	ebx;
	struct i386_interrupt_state *is;
};

void
kdb_kentry(
	struct int_regs	*int_regs)
{
	struct i386_interrupt_state *is = int_regs->is;
	spl_t	s = splhigh();

#if	NCPUS > 1
	if (db_enter())
#endif	/* NCPUS > 1 */
	{
	    if ((is->cs & 0x3) != KERNEL_RING) {
		ddb_regs.uesp = ((int *)(is+1))[0];
		ddb_regs.ss   = ((int *)(is+1))[1];
	    }
	    else {
		ddb_regs.ss  = KERNEL_DS;
		ddb_regs.uesp= (int)(is+1);
	    }
	    ddb_regs.efl = is->efl;
	    ddb_regs.cs  = is->cs;
	    ddb_regs.eip = is->eip;
	    ddb_regs.eax = is->eax;
	    ddb_regs.ecx = is->ecx;
	    ddb_regs.edx = is->edx;
	    ddb_regs.ebx = int_regs->ebx;
	    ddb_regs.ebp = int_regs->ebp;
	    ddb_regs.esi = int_regs->esi;
	    ddb_regs.edi = int_regs->edi;
	    ddb_regs.ds  = is->ds;
	    ddb_regs.es  = is->es;
	    ddb_regs.fs  = is->fs;
	    ddb_regs.gs  = is->gs;

	    cnpollc(TRUE);
	    db_task_trap(-1, 0, (ddb_regs.cs & 0x3) != 0);
	    cnpollc(FALSE);

	    if ((ddb_regs.cs & 0x3) != KERNEL_RING) {
		((int *)(is+1))[0] = ddb_regs.uesp;
		((int *)(is+1))[1] = ddb_regs.ss & 0xffff;
	    }
	    is->efl = ddb_regs.efl;
	    is->cs  = ddb_regs.cs & 0xffff;
	    is->eip = ddb_regs.eip;
	    is->eax = ddb_regs.eax;
	    is->ecx = ddb_regs.ecx;
	    is->edx = ddb_regs.edx;
	    int_regs->ebx = ddb_regs.ebx;
	    int_regs->ebp = ddb_regs.ebp;
	    int_regs->esi = ddb_regs.esi;
	    int_regs->edi = ddb_regs.edi;
	    is->ds  = ddb_regs.ds & 0xffff;
	    is->es  = ddb_regs.es & 0xffff;
	    is->fs  = ddb_regs.fs & 0xffff;
	    is->gs  = ddb_regs.gs & 0xffff;
	}
#if	NCPUS > 1
	db_leave();
#endif	/* NCPUS > 1 */

	(void) splx(s);
}

boolean_t db_no_vm_fault = TRUE;

int
db_user_to_kernel_address(
	const task_t	task,
	vm_offset_t	addr,
	vm_offset_t	*kaddr,
	int		flag)
{
	pt_entry_t *ptp;
	boolean_t	faulted = FALSE;

	retry:
	ptp = pmap_pte(task->map->pmap, addr);
	if (ptp == PT_ENTRY_NULL || (*ptp & INTEL_PTE_VALID) == 0) {
	    if (!faulted && !db_no_vm_fault) {
		kern_return_t	err;

		faulted = TRUE;
		err = vm_fault( task->map,
				trunc_page(addr),
				VM_PROT_READ,
				FALSE, FALSE, 0);
		if (err == KERN_SUCCESS)
		    goto retry;
	    }
	    if (flag) {
		db_printf("\nno memory is assigned to address %08x\n", addr);
	    }
	    return(-1);
	}
	*kaddr = ptetokv(*ptp) + (addr & (INTEL_PGBYTES-1));
	return(0);
}

/*
 * Read bytes from kernel address space for debugger.
 */

boolean_t
db_read_bytes(
	vm_offset_t	addr,
	int		size,
	char		*data,
	task_t		task)
{
	char		*src;
	int		n;
	vm_offset_t	kern_addr;

	src = (char *)addr;
	if ((addr >= VM_MIN_KERNEL_ADDRESS && addr < VM_MAX_KERNEL_ADDRESS) || task == TASK_NULL) {
	    if (task == TASK_NULL)
	        task = db_current_task();
	    while (--size >= 0) {
		if (addr < VM_MIN_KERNEL_ADDRESS && task == TASK_NULL) {
		    db_printf("\nbad address %x\n", addr);
		    return FALSE;
		}
		addr++;
		*data++ = *src++;
	    }
	    return TRUE;
	}
	while (size > 0) {
	    if (db_user_to_kernel_address(task, addr, &kern_addr, 1) < 0)
		return FALSE;
	    src = (char *)kern_addr;
	    n = intel_trunc_page(addr+INTEL_PGBYTES) - addr;
	    if (n > size)
		n = size;
	    size -= n;
	    addr += n;
	    while (--n >= 0)
		*data++ = *src++;
	}
	return TRUE;
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(
	vm_offset_t	addr,
	int		size,
	char		*data,
	task_t		task)
{
	char		*dst;

	pt_entry_t *ptep0 = 0;
	pt_entry_t	oldmap0 = 0;
	vm_offset_t	addr1;
	pt_entry_t *ptep1 = 0;
	pt_entry_t	oldmap1 = 0;
	extern char	etext;

	if ((addr < VM_MIN_KERNEL_ADDRESS) ^
	    ((addr + size) <= VM_MIN_KERNEL_ADDRESS)) {
	    db_error("\ncannot write data into mixed space\n");
	    /* NOTREACHED */
	}
	if (addr < VM_MIN_KERNEL_ADDRESS) {
	    if (task) {
		db_write_bytes_user_space(addr, size, data, task);
		return;
	    } else if (db_current_task() == TASK_NULL) {
		db_printf("\nbad address %x\n", addr);
		db_error(0);
		/* NOTREACHED */
	    }
	}

	if (addr >= VM_MIN_KERNEL_ADDRESS &&
	    addr <= (vm_offset_t)&etext)
	{
	    ptep0 = pmap_pte(kernel_pmap, addr);
	    oldmap0 = *ptep0;
	    *ptep0 |= INTEL_PTE_WRITE;

	    addr1 = i386_trunc_page(addr + size - 1);
	    if (i386_trunc_page(addr) != addr1) {
		/* data crosses a page boundary */

		ptep1 = pmap_pte(kernel_pmap, addr1);
		oldmap1 = *ptep1;
		*ptep1 |= INTEL_PTE_WRITE;
	    }
	    if (CPU_HAS_FEATURE(CPU_FEATURE_PGE))
		set_cr4(get_cr4() & ~CR4_PGE);
	    flush_tlb();
	}

	dst = (char *)addr;

	while (--size >= 0)
	    *dst++ = *data++;

	if (ptep0) {
	    *ptep0 = oldmap0;
	    if (ptep1) {
		*ptep1 = oldmap1;
	    }
	    flush_tlb();
	    if (CPU_HAS_FEATURE(CPU_FEATURE_PGE))
		set_cr4(get_cr4() | CR4_PGE);
	}
}

void
db_write_bytes_user_space(
	vm_offset_t	addr,
	int		size,
	char		*data,
	task_t		task)
{
	char		*dst;
	int		n;
	vm_offset_t	kern_addr;

	while (size > 0) {
	    if (db_user_to_kernel_address(task, addr, &kern_addr, 1) < 0)
		return;
	    dst = (char *)kern_addr;
	    n = intel_trunc_page(addr+INTEL_PGBYTES) - addr;
	    if (n > size)
		n = size;
	    size -= n;
	    addr += n;
	    while (--n >= 0)
		*dst++ = *data++;
	}
}

boolean_t
db_check_access(
	vm_offset_t	addr,
	int		size,
	task_t		task)
{
	int	n;
	vm_offset_t	kern_addr;

	if (addr >= VM_MIN_KERNEL_ADDRESS) {
	    if (kernel_task == TASK_NULL)
	        return TRUE;
	    task = kernel_task;
	} else if (task == TASK_NULL) {
	    if (current_thread() == THREAD_NULL)
		return FALSE;
	    task = current_thread()->task;
	}
	while (size > 0) {
	    if (db_user_to_kernel_address(task, addr, &kern_addr, 0) < 0)
		return FALSE;
	    n = intel_trunc_page(addr+INTEL_PGBYTES) - addr;
	    if (n > size)
		n = size;
	    size -= n;
	    addr += n;
	}
	return TRUE;
}

boolean_t
db_phys_eq(
	task_t		task1,
	vm_offset_t	addr1,
	const task_t	task2,
	vm_offset_t	addr2)
{
	vm_offset_t	kern_addr1, kern_addr2;

	if (addr1 >= VM_MIN_KERNEL_ADDRESS || addr2 >= VM_MIN_KERNEL_ADDRESS)
	    return FALSE;
	if ((addr1 & (INTEL_PGBYTES-1)) != (addr2 & (INTEL_PGBYTES-1)))
	    return FALSE;
	if (task1 == TASK_NULL) {
	    if (current_thread() == THREAD_NULL)
		return FALSE;
	    task1 = current_thread()->task;
	}
	if (db_user_to_kernel_address(task1, addr1, &kern_addr1, 0) < 0
		|| db_user_to_kernel_address(task2, addr2, &kern_addr2, 0) < 0)
	    return FALSE;
	return(kern_addr1 == kern_addr2);
}

#define DB_USER_STACK_ADDR		(VM_MIN_KERNEL_ADDRESS)
#define DB_NAME_SEARCH_LIMIT		(DB_USER_STACK_ADDR-(INTEL_PGBYTES*3))

#define GNU

#ifndef GNU
static boolean_t
db_search_null(
	const task_t	task,
	vm_offset_t	*svaddr,
	vm_offset_t	evaddr,
	vm_offset_t	*skaddr,
	int		flag)
{
	unsigned vaddr;
	unsigned *kaddr;

	kaddr = (unsigned *)*skaddr;
	for (vaddr = *svaddr; vaddr > evaddr; ) {
	    if (vaddr % INTEL_PGBYTES == 0) {
		vaddr -= sizeof(unsigned);
		if (db_user_to_kernel_address(task, vaddr, skaddr, 0) < 0)
		    return FALSE;
		kaddr = (vm_offset_t *)*skaddr;
	    } else {
		vaddr -= sizeof(unsigned);
		kaddr--;
	    }
	    if ((*kaddr == 0) ^ (flag  == 0)) {
		*svaddr = vaddr;
		*skaddr = (unsigned)kaddr;
		return TRUE;
	    }
	}
	return FALSE;
}
#endif /* GNU */

#ifdef GNU
static boolean_t
looks_like_command(
	const task_t	task,
	char*		kaddr)
{
	char *c;

	assert(!((vm_offset_t) kaddr & (INTEL_PGBYTES-1)));

	/*
	 * Must be the environment.
	 */
	if (!memcmp(kaddr, "PATH=", 5) || !memcmp(kaddr, "TERM=", 5) || !memcmp(kaddr, "SHELL=", 6) || !memcmp(kaddr, "LOCAL_PART=", 11) || !memcmp(kaddr, "LC_ALL=", 7))
		return FALSE;

	/*
	 * This is purely heuristical but works quite nicely.
	 * We know that it should look like words separated by \0, and
	 * eventually only \0s.
	 */
	c = kaddr;
	while (c < kaddr + INTEL_PGBYTES) {
		if (!*c) {
			if (c == kaddr)
				/* Starts by \0.  */
				return FALSE;
			break;
		}
		while (c < kaddr + INTEL_PGBYTES && *c)
			c++;
		if (c < kaddr + INTEL_PGBYTES)
			c++;	/* Skip \0 */
	}
	/*
	 * Check that the remainder is just \0s.
	 */
	while (c < kaddr + INTEL_PGBYTES)
		if (*c++)
			return FALSE;

	return TRUE;
}
#endif /* GNU */

void
db_task_name(
	const task_t task)
{
	char *p;
	int n;
	vm_offset_t vaddr, kaddr;
	unsigned sp;

	if (task->name[0]) {
		db_printf("%s", task->name);
		return;
	}

#ifdef GNU
	/*
	 * GNU Hurd-specific heuristics.
	 */

	/* Heuristical address first.  */
	vaddr = 0x1026000;
	if (db_user_to_kernel_address(task, vaddr, &kaddr, 0) >= 0 &&
		looks_like_command(task, (char*) kaddr))
			goto ok;

	/* Try to catch SP of the main thread.  */
	thread_t thread;

	task_lock(task);
	thread = (thread_t) queue_first(&task->thread_list);
	if (!thread) {
		task_unlock(task);
		db_printf(DB_NULL_TASK_NAME);
		return;
	}
	sp = thread->pcb->iss.uesp;
	task_unlock(task);

	vaddr = (sp & ~(INTEL_PGBYTES - 1)) + INTEL_PGBYTES;
	while (1) {
		if (db_user_to_kernel_address(task, vaddr, &kaddr, 0) < 0)
			return;
		if (looks_like_command(task, (char*) kaddr))
			break;
		vaddr += INTEL_PGBYTES;
	}
#else /* GNU */
	vaddr = DB_USER_STACK_ADDR;
	kaddr = 0;

	/*
	 * skip nulls at the end
	 */
	if (!db_search_null(task, &vaddr, DB_NAME_SEARCH_LIMIT, &kaddr, 0)) {
	    db_printf(DB_NULL_TASK_NAME);
	    return;
	}
	/*
	 * search start of args
	 */
	if (!db_search_null(task, &vaddr, DB_NAME_SEARCH_LIMIT, &kaddr, 1)) {
	    db_printf(DB_NULL_TASK_NAME);
	    return;
	}
#endif /* GNU */

ok:
	n = DB_TASK_NAME_LEN-1;
#ifdef GNU
	p = (char *)kaddr;
	for (; n > 0; vaddr++, p++, n--) {
#else /* GNU */
	p = (char *)kaddr + sizeof(unsigned);
	for (vaddr += sizeof(int); vaddr < DB_USER_STACK_ADDR && n > 0;
							vaddr++, p++, n--) {
#endif  /* GNU */
	    if (vaddr % INTEL_PGBYTES == 0) {
		(void)db_user_to_kernel_address(task, vaddr, &kaddr, 0);
		p = (char*)kaddr;
	    }
	    db_printf("%c", (*p < ' ' || *p > '~')? ' ': *p);
	}
	while (n-- >= 0)	/* compare with >= 0 for one more space */
	    db_printf(" ");
}

#endif /* MACH_KDB */
