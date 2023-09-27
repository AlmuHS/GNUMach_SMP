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

#include <stddef.h>
#include <string.h>

#include <mach/std_types.h>
#include <mach/kern_return.h>
#include <mach/thread_status.h>
#include <mach/exec/exec.h>
#include <mach/xen.h>

#include "vm_param.h"
#include <kern/counters.h>
#include <kern/debug.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <kern/slab.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <i386/thread.h>
#include <i386/proc_reg.h>
#include <i386/seg.h>
#include <i386/user_ldt.h>
#include <i386/db_interface.h>
#include <i386/fpu.h>
#include "eflags.h"
#include "gdt.h"
#include "ldt.h"
#include "msr.h"
#include "ktss.h"
#include "pcb.h"

#include <machine/tss.h>

#if	NCPUS > 1
#include <i386/mp_desc.h>
#endif

struct kmem_cache	pcb_cache;

vm_offset_t	kernel_stack[NCPUS];	/* top of active_stack */

/*
 *	stack_attach:
 *
 *	Attach a kernel stack to a thread.
 */

void stack_attach(
	thread_t 	thread,
	vm_offset_t 	stack,
	void 		(*continuation)(thread_t))
{
	counter(if (++c_stacks_current > c_stacks_max)
			c_stacks_max = c_stacks_current);

	thread->kernel_stack = stack;

	/*
	 *	We want to run continuation, giving it as an argument
	 *	the return value from Load_context/Switch_context.
	 *	Thread_continue takes care of the mismatch between
	 *	the argument-passing/return-value conventions.
	 *	This function will not return normally,
	 *	so we don`t have to worry about a return address.
	 */
	STACK_IKS(stack)->k_eip = (long) Thread_continue;
	STACK_IKS(stack)->k_ebx = (long) continuation;
	STACK_IKS(stack)->k_esp = (long) STACK_IEL(stack);
	STACK_IKS(stack)->k_ebp = (long) 0;

	/*
	 *	Point top of kernel stack to user`s registers.
	 */
	STACK_IEL(stack)->saved_state = USER_REGS(thread);
}

/*
 *	stack_detach:
 *
 *	Detaches a kernel stack from a thread, returning the old stack.
 */

vm_offset_t stack_detach(thread_t thread)
{
	vm_offset_t	stack;

	counter(if (--c_stacks_current < c_stacks_min)
			c_stacks_min = c_stacks_current);

	stack = thread->kernel_stack;
	thread->kernel_stack = 0;

	return stack;
}

#if	NCPUS > 1
#define	curr_gdt(mycpu)		(mp_gdt[mycpu])
#define	curr_ktss(mycpu)	(mp_ktss[mycpu])
#else
#define	curr_gdt(mycpu)		((void)(mycpu), gdt)
#define	curr_ktss(mycpu)	((void)(mycpu), (struct task_tss *)&ktss)
#endif

#define	gdt_desc_p(mycpu,sel) \
	((struct real_descriptor *)&curr_gdt(mycpu)[sel_idx(sel)])

void switch_ktss(pcb_t pcb)
{
	int			mycpu = cpu_number();
    {
	vm_offset_t		pcb_stack_top;

	/*
	 *	Save a pointer to the top of the "kernel" stack -
	 *	actually the place in the PCB where a trap into
	 *	kernel mode will push the registers.
	 *	The location depends on V8086 mode.  If we are
	 *	not in V8086 mode, then a trap into the kernel
	 *	won`t save the v86 segments, so we leave room.
	 */

#if !defined(__x86_64__) || defined(USER32)
	pcb_stack_top = (pcb->iss.efl & EFL_VM)
			? (long) (&pcb->iss + 1)
			: (long) (&pcb->iss.v86_segs);
#else
	pcb_stack_top = (vm_offset_t) (&pcb->iss + 1);
#endif

#ifdef __x86_64__
	assert((pcb_stack_top & 0xF) == 0);
#endif

#ifdef	MACH_RING1
	/* No IO mask here */
	if (hyp_stack_switch(KERNEL_DS, pcb_stack_top))
		panic("stack_switch");
#else	/* MACH_RING1 */
#ifdef __x86_64__
	curr_ktss(mycpu)->tss.rsp0 = pcb_stack_top;
#else /* __x86_64__ */
	curr_ktss(mycpu)->tss.esp0 = pcb_stack_top;
#endif /* __x86_64__ */
#endif	/* MACH_RING1 */
    }

    {
	user_ldt_t	tldt = pcb->ims.ldt;
	/*
	 * Set the thread`s LDT.
	 */
	if (tldt == 0) {
	    /*
	     * Use system LDT.
	     */
#ifdef	MACH_PV_DESCRIPTORS
	    hyp_set_ldt(&ldt, LDTSZ);
#else	/* MACH_PV_DESCRIPTORS */
	    if (get_ldt() != KERNEL_LDT)
		set_ldt(KERNEL_LDT);
#endif	/* MACH_PV_DESCRIPTORS */
	}
	else {
	    /*
	     * Thread has its own LDT.
	     */
#ifdef	MACH_PV_DESCRIPTORS
	    hyp_set_ldt(tldt->ldt,
	    		(tldt->desc.limit_low|(tldt->desc.limit_high<<16)) /
				sizeof(struct real_descriptor));
#else	/* MACH_PV_DESCRIPTORS */
	    *gdt_desc_p(mycpu,USER_LDT) = tldt->desc;
	    set_ldt(USER_LDT);
#endif	/* MACH_PV_DESCRIPTORS */
	}
    }

#ifdef	MACH_PV_DESCRIPTORS
    {
	int i;
	for (i=0; i < USER_GDT_SLOTS; i++) {
	    if (memcmp(gdt_desc_p (mycpu, USER_GDT + (i << 3)),
		&pcb->ims.user_gdt[i], sizeof pcb->ims.user_gdt[i])) {
		union {
			struct real_descriptor real_descriptor;
			uint64_t descriptor;
		} user_gdt;
		user_gdt.real_descriptor = pcb->ims.user_gdt[i];

		if (hyp_do_update_descriptor(kv_to_ma(gdt_desc_p (mycpu, USER_GDT + (i << 3))),
			user_gdt.descriptor))
		    panic("couldn't set user gdt %d\n",i);
	    }
	}
    }
#else /* MACH_PV_DESCRIPTORS */

    /* Copy in the per-thread GDT slots.  No reloading is necessary
       because just restoring the segment registers on the way back to
       user mode reloads the shadow registers from the in-memory GDT.  */
    memcpy (gdt_desc_p (mycpu, USER_GDT),
        pcb->ims.user_gdt, sizeof pcb->ims.user_gdt);
#endif /* MACH_PV_DESCRIPTORS */

#if defined(__x86_64__) && !defined(USER32)
	wrmsr(MSR_REG_FSBASE, pcb->ims.sbs.fsbase);
	wrmsr(MSR_REG_GSBASE, pcb->ims.sbs.gsbase);
#endif

	db_load_context(pcb);

	/*
	 * Load the floating-point context, if necessary.
	 */
	fpu_load_context(pcb);

}

/* If NEW_IOPB is not null, the SIZE denotes the number of bytes in
   the new bitmap.  Expects iopb_lock to be held.  */
void
update_ktss_iopb (unsigned char *new_iopb, io_port_t size)
{
  struct task_tss *tss = curr_ktss (cpu_number ());

  if (new_iopb && size > 0)
    {
      tss->tss.io_bit_map_offset
       = offsetof (struct task_tss, barrier) - size;
      memcpy (((char *) tss) + tss->tss.io_bit_map_offset,
             new_iopb, size);
    }
  else
    tss->tss.io_bit_map_offset = IOPB_INVAL;
}

/*
 *	stack_handoff:
 *
 *	Move the current thread's kernel stack to the new thread.
 */

void stack_handoff(
	thread_t	old,
	thread_t	new)
{
	int		mycpu = cpu_number();
	vm_offset_t	stack;

	/*
	 *	Save FP registers if in use.
	 */
	fpu_save_context(old);

	/*
	 *	Switch address maps if switching tasks.
	 */
    {
	task_t old_task, new_task;

	if ((old_task = old->task) != (new_task = new->task)) {
		PMAP_DEACTIVATE_USER(vm_map_pmap(old_task->map),
				     old, mycpu);
		PMAP_ACTIVATE_USER(vm_map_pmap(new_task->map),
				   new, mycpu);

		simple_lock (&new_task->machine.iopb_lock);
#if NCPUS>1
#warning SMP support missing (avoid races with io_perm_modify).
#else
		/* This optimization only works on a single processor
		   machine, where old_task's iopb can not change while
		   we are switching.  */
		if (old_task->machine.iopb || new_task->machine.iopb)
#endif
		  update_ktss_iopb (new_task->machine.iopb,
				    new_task->machine.iopb_size);
		simple_unlock (&new_task->machine.iopb_lock);
	}
    }

	/*
	 *	Load the rest of the user state for the new thread
	 */
	switch_ktss(new->pcb);

	/*
	 *	Switch to new thread
	 */
	stack = current_stack();
	old->kernel_stack = 0;
	new->kernel_stack = stack;
	percpu_assign(active_thread, new);

	/*
	 *	Switch exception link to point to new
	 *	user registers.
	 */

	STACK_IEL(stack)->saved_state = USER_REGS(new);

}

/*
 * Switch to the first thread on a CPU.
 */
void load_context(thread_t new)
{
	switch_ktss(new->pcb);
	Load_context(new);
}

/*
 * Switch to a new thread.
 * Save the old thread`s kernel state or continuation,
 * and return it.
 */
thread_t switch_context(
	thread_t	old,
	continuation_t	continuation,
	thread_t	new)
{
	/*
	 *	Save FP registers if in use.
	 */
	fpu_save_context(old);

	/*
	 *	Switch address maps if switching tasks.
	 */
    {
	task_t old_task, new_task;
	int	mycpu = cpu_number();

	if ((old_task = old->task) != (new_task = new->task)) {
		PMAP_DEACTIVATE_USER(vm_map_pmap(old_task->map),
				     old, mycpu);
		PMAP_ACTIVATE_USER(vm_map_pmap(new_task->map),
				   new, mycpu);

		simple_lock (&new_task->machine.iopb_lock);
#if NCPUS>1
#warning SMP support missing (avoid races with io_perm_modify).
#else
		/* This optimization only works on a single processor
		   machine, where old_task's iopb can not change while
		   we are switching.  */
		if (old_task->machine.iopb || new_task->machine.iopb)
#endif
		  update_ktss_iopb (new_task->machine.iopb,
				    new_task->machine.iopb_size);
		simple_unlock (&new_task->machine.iopb_lock);
	}
    }

	/*
	 *	Load the rest of the user state for the new thread
	 */
	switch_ktss(new->pcb);
	return Switch_context(old, continuation, new);
}

void pcb_module_init(void)
{
	kmem_cache_init(&pcb_cache, "pcb", sizeof(struct pcb),
			KERNEL_STACK_ALIGN, NULL, 0);

	fpu_module_init();
}

void pcb_init(task_t parent_task, thread_t thread)
{
	pcb_t		pcb;

	pcb = (pcb_t) kmem_cache_alloc(&pcb_cache);
	if (pcb == 0)
		panic("pcb_init");

	counter(if (++c_threads_current > c_threads_max)
			c_threads_max = c_threads_current);

	/*
	 *	We can't let random values leak out to the user.
	 */
	memset(pcb, 0, sizeof *pcb);
	simple_lock_init(&pcb->lock);

	/*
	 *	Guarantee that the bootstrapped thread will be in user
	 *	mode.
	 */
	pcb->iss.cs = USER_CS;
	pcb->iss.ss = USER_DS;
#if !defined(__x86_64__) || defined(USER32)
	pcb->iss.ds = USER_DS;
	pcb->iss.es = USER_DS;
	pcb->iss.fs = USER_DS;
	pcb->iss.gs = USER_DS;
#endif
	pcb->iss.efl = EFL_USER_SET;

	thread->pcb = pcb;

	/* This is a new thread for the current task, make it inherit our FPU
	   state.  */
	if (current_thread() && parent_task == current_task())
		fpinherit(current_thread(), thread);
}

void pcb_terminate(thread_t thread)
{
	pcb_t		pcb = thread->pcb;

	counter(if (--c_threads_current < c_threads_min)
			c_threads_min = c_threads_current);

	if (pcb->ims.ifps != 0)
		fp_free(pcb->ims.ifps);
	if (pcb->ims.ldt != 0)
		user_ldt_free(pcb->ims.ldt);
	kmem_cache_free(&pcb_cache, (vm_offset_t) pcb);
	thread->pcb = 0;
}

/*
 *	pcb_collect:
 *
 *	Attempt to free excess pcb memory.
 */

void pcb_collect(__attribute__((unused)) const thread_t thread)
{
}


/*
 *	thread_setstatus:
 *
 *	Set the status of the specified thread.
 */

kern_return_t thread_setstatus(
	thread_t		thread,
	int			flavor,
	thread_state_t		tstate,
	unsigned int		count)
{
	switch (flavor) {
	    case i386_THREAD_STATE:
	    case i386_REGS_SEGS_STATE:
	    {
		struct i386_thread_state	*state;
		struct i386_saved_state	*saved_state;

		if (count < i386_THREAD_STATE_COUNT) {
		    return(KERN_INVALID_ARGUMENT);
		}

		state = (struct i386_thread_state *) tstate;

		if (flavor == i386_REGS_SEGS_STATE) {
		    /*
		     * Code and stack selectors must not be null,
		     * and must have user protection levels.
		     * Only the low 16 bits are valid.
		     */
		    state->cs &= 0xffff;
		    state->ss &= 0xffff;
#if !defined(__x86_64__) || defined(USER32)
		    state->ds &= 0xffff;
		    state->es &= 0xffff;
		    state->fs &= 0xffff;
		    state->gs &= 0xffff;
#endif

		    if (state->cs == 0 || (state->cs & SEL_PL) != SEL_PL_U
		     || state->ss == 0 || (state->ss & SEL_PL) != SEL_PL_U)
			return KERN_INVALID_ARGUMENT;
		}

		saved_state = USER_REGS(thread);

		/*
		 * General registers
		 */
#if defined(__x86_64__) && !defined(USER32)
		saved_state->r8 = state->r8;
		saved_state->r9 = state->r9;
		saved_state->r10 = state->r10;
		saved_state->r11 = state->r11;
		saved_state->r12 = state->r12;
		saved_state->r13 = state->r13;
		saved_state->r14 = state->r14;
		saved_state->r15 = state->r15;
		saved_state->edi = state->rdi;
		saved_state->esi = state->rsi;
		saved_state->ebp = state->rbp;
		saved_state->uesp = state->ursp;
		saved_state->ebx = state->rbx;
		saved_state->edx = state->rdx;
		saved_state->ecx = state->rcx;
		saved_state->eax = state->rax;
		saved_state->eip = state->rip;
		saved_state->efl = (state->rfl & ~EFL_USER_CLEAR)
				    | EFL_USER_SET;
#else
		saved_state->edi = state->edi;
		saved_state->esi = state->esi;
		saved_state->ebp = state->ebp;
		saved_state->uesp = state->uesp;
		saved_state->ebx = state->ebx;
		saved_state->edx = state->edx;
		saved_state->ecx = state->ecx;
		saved_state->eax = state->eax;
		saved_state->eip = state->eip;
		saved_state->efl = (state->efl & ~EFL_USER_CLEAR)
				    | EFL_USER_SET;
#endif /* __x86_64__ && !USER32 */

#if !defined(__x86_64__) || defined(USER32)
		/*
		 * Segment registers.  Set differently in V8086 mode.
		 */
		if (saved_state->efl & EFL_VM) {
		    /*
		     * Set V8086 mode segment registers.
		     */
		    saved_state->cs = state->cs & 0xffff;
		    saved_state->ss = state->ss & 0xffff;
		    saved_state->v86_segs.v86_ds = state->ds & 0xffff;
		    saved_state->v86_segs.v86_es = state->es & 0xffff;
		    saved_state->v86_segs.v86_fs = state->fs & 0xffff;
		    saved_state->v86_segs.v86_gs = state->gs & 0xffff;

		    /*
		     * Zero protected mode segment registers.
		     */
		    saved_state->ds = 0;
		    saved_state->es = 0;
		    saved_state->fs = 0;
		    saved_state->gs = 0;

		    if (thread->pcb->ims.v86s.int_table) {
			/*
			 * Hardware assist on.
			 */
			thread->pcb->ims.v86s.flags =
			    saved_state->efl & (EFL_TF | EFL_IF);
		    }
		} else
#endif
		if (flavor == i386_THREAD_STATE) {
		    /*
		     * 386 mode.  Set segment registers for flat
		     * 32-bit address space.
		     */
		    saved_state->cs = USER_CS;
		    saved_state->ss = USER_DS;
#if !defined(__x86_64__) || defined(USER32)
		    saved_state->ds = USER_DS;
		    saved_state->es = USER_DS;
		    saved_state->fs = USER_DS;
		    saved_state->gs = USER_DS;
#endif
		}
		else {
		    /*
		     * User setting segment registers.
		     * Code and stack selectors have already been
		     * checked.  Others will be reset by 'iret'
		     * if they are not valid.
		     */
		    saved_state->cs = state->cs;
		    saved_state->ss = state->ss;
#if !defined(__x86_64__) || defined(USER32)
		    saved_state->ds = state->ds;
		    saved_state->es = state->es;
		    saved_state->fs = state->fs;
		    saved_state->gs = state->gs;
#endif
		}
		break;
	    }

	    case i386_FLOAT_STATE: {

		if (count < i386_FLOAT_STATE_COUNT)
			return(KERN_INVALID_ARGUMENT);

		return fpu_set_state(thread,
				(struct i386_float_state *) tstate);
	    }

	    /*
	     * Temporary - replace by i386_io_map
	     */
	    case i386_ISA_PORT_MAP_STATE: {
		//register struct i386_isa_port_map_state *state;

		if (count < i386_ISA_PORT_MAP_STATE_COUNT)
			return(KERN_INVALID_ARGUMENT);

#if 0
		/*
		 *	If the thread has no ktss yet,
		 *	we must allocate one.
		 */

		state = (struct i386_isa_port_map_state *) tstate;
		tss = thread->pcb->ims.io_tss;
		if (tss == 0) {
			tss = iopb_create();
			thread->pcb->ims.io_tss = tss;
		}

		memcpy(tss->bitmap,
		       state->pm,
		       sizeof state->pm);
#endif
		break;
	    }
#if !defined(__x86_64__) || defined(USER32)
	    case i386_V86_ASSIST_STATE:
	    {
		struct i386_v86_assist_state *state;
		vm_offset_t	int_table;
		int		int_count;

		if (count < i386_V86_ASSIST_STATE_COUNT)
		    return KERN_INVALID_ARGUMENT;

		state = (struct i386_v86_assist_state *) tstate;
		int_table = state->int_table;
		int_count = state->int_count;

		if (int_table >= VM_MAX_USER_ADDRESS ||
		    int_table +
			int_count * sizeof(struct v86_interrupt_table)
			    > VM_MAX_USER_ADDRESS)
		    return KERN_INVALID_ARGUMENT;

		thread->pcb->ims.v86s.int_table = int_table;
		thread->pcb->ims.v86s.int_count = int_count;

		thread->pcb->ims.v86s.flags =
			USER_REGS(thread)->efl & (EFL_TF | EFL_IF);
		break;
	    }
#endif
	    case i386_DEBUG_STATE:
	    {
		struct i386_debug_state *state;
		kern_return_t ret;

		if (count < i386_DEBUG_STATE_COUNT)
		    return KERN_INVALID_ARGUMENT;

		state = (struct i386_debug_state *) tstate;
		ret = db_set_debug_state(thread->pcb, state);
		if (ret)
			return ret;
		break;
	    }
#if defined(__x86_64__) && !defined(USER32)
	    case i386_FSGS_BASE_STATE:
            {
                    struct i386_fsgs_base_state *state;
                    if (count < i386_FSGS_BASE_STATE_COUNT)
                            return KERN_INVALID_ARGUMENT;

                    state = (struct i386_fsgs_base_state *) tstate;
                    thread->pcb->ims.sbs.fsbase = state->fs_base;
                    thread->pcb->ims.sbs.gsbase = state->gs_base;
                    if (thread == current_thread()) {
                            wrmsr(MSR_REG_FSBASE, state->fs_base);
                            wrmsr(MSR_REG_GSBASE, state->gs_base);
                    }
                    break;
            }
#endif
	    default:
		return(KERN_INVALID_ARGUMENT);
	}

	return(KERN_SUCCESS);
}

/*
 *	thread_getstatus:
 *
 *	Get the status of the specified thread.
 */

kern_return_t thread_getstatus(
	thread_t		thread,
	int			flavor,
	thread_state_t		tstate,	/* pointer to OUT array */
	unsigned int		*count)		/* IN/OUT */
{
	switch (flavor)  {
	    case THREAD_STATE_FLAVOR_LIST:
#if !defined(__x86_64__) || defined(USER32)
		unsigned int ncount = 4;
#else
		unsigned int ncount = 3;
#endif
		if (*count < ncount)
		    return (KERN_INVALID_ARGUMENT);
		tstate[0] = i386_THREAD_STATE;
		tstate[1] = i386_FLOAT_STATE;
		tstate[2] = i386_ISA_PORT_MAP_STATE;
#if !defined(__x86_64__) || defined(USER32)
		tstate[3] = i386_V86_ASSIST_STATE;
#endif
		*count = ncount;
		break;

	    case i386_THREAD_STATE:
	    case i386_REGS_SEGS_STATE:
	    {
		struct i386_thread_state	*state;
		struct i386_saved_state	*saved_state;

		if (*count < i386_THREAD_STATE_COUNT)
		    return(KERN_INVALID_ARGUMENT);

		state = (struct i386_thread_state *) tstate;
		saved_state = USER_REGS(thread);

		/*
		 * General registers.
		 */
#if defined(__x86_64__) && !defined(USER32)
		state->r8 = saved_state->r8;
		state->r9 = saved_state->r9;
		state->r10 = saved_state->r10;
		state->r11 = saved_state->r11;
		state->r12 = saved_state->r12;
		state->r13 = saved_state->r13;
		state->r14 = saved_state->r14;
		state->r15 = saved_state->r15;
		state->rdi = saved_state->edi;
		state->rsi = saved_state->esi;
		state->rbp = saved_state->ebp;
		state->rbx = saved_state->ebx;
		state->rdx = saved_state->edx;
		state->rcx = saved_state->ecx;
		state->rax = saved_state->eax;
		state->rip = saved_state->eip;
		state->ursp = saved_state->uesp;
		state->rfl = saved_state->efl;
		state->rsp = 0;	/* unused */
#else
		state->edi = saved_state->edi;
		state->esi = saved_state->esi;
		state->ebp = saved_state->ebp;
		state->ebx = saved_state->ebx;
		state->edx = saved_state->edx;
		state->ecx = saved_state->ecx;
		state->eax = saved_state->eax;
		state->eip = saved_state->eip;
		state->uesp = saved_state->uesp;
		state->efl = saved_state->efl;
		state->esp = 0;	/* unused */
#endif /* __x86_64__ && !USER32 */

		state->cs = saved_state->cs;
		state->ss = saved_state->ss;
#if !defined(__x86_64__) || defined(USER32)
		if (saved_state->efl & EFL_VM) {
		    /*
		     * V8086 mode.
		     */
		    state->ds = saved_state->v86_segs.v86_ds & 0xffff;
		    state->es = saved_state->v86_segs.v86_es & 0xffff;
		    state->fs = saved_state->v86_segs.v86_fs & 0xffff;
		    state->gs = saved_state->v86_segs.v86_gs & 0xffff;

		    if (thread->pcb->ims.v86s.int_table) {
			/*
			 * Hardware assist on
			 */
			if ((thread->pcb->ims.v86s.flags &
					(EFL_IF|V86_IF_PENDING))
				== 0)
			    saved_state->efl &= ~EFL_IF;
		    }
		} else {
		    /*
		     * 386 mode.
		     */
		    state->ds = saved_state->ds & 0xffff;
		    state->es = saved_state->es & 0xffff;
		    state->fs = saved_state->fs & 0xffff;
		    state->gs = saved_state->gs & 0xffff;
		}
#endif
		*count = i386_THREAD_STATE_COUNT;
		break;
	    }

	    case i386_FLOAT_STATE: {

		if (*count < i386_FLOAT_STATE_COUNT)
			return(KERN_INVALID_ARGUMENT);

		*count = i386_FLOAT_STATE_COUNT;
		return fpu_get_state(thread,
				(struct i386_float_state *)tstate);
	    }

	    /*
	     * Temporary - replace by i386_io_map
	     */
	    case i386_ISA_PORT_MAP_STATE: {
		struct i386_isa_port_map_state *state;

		if (*count < i386_ISA_PORT_MAP_STATE_COUNT)
			return(KERN_INVALID_ARGUMENT);

		state = (struct i386_isa_port_map_state *) tstate;

		simple_lock (&thread->task->machine.iopb_lock);
		if (thread->task->machine.iopb == 0)
		  memset (state->pm, 0xff, sizeof state->pm);
		else
		  memcpy(state->pm,
			 thread->task->machine.iopb,
			 sizeof state->pm);
		simple_unlock (&thread->task->machine.iopb_lock);

		*count = i386_ISA_PORT_MAP_STATE_COUNT;
		break;
	    }
#if !defined(__x86_64__) || defined(USER32)
	    case i386_V86_ASSIST_STATE:
	    {
		struct i386_v86_assist_state *state;

		if (*count < i386_V86_ASSIST_STATE_COUNT)
		    return KERN_INVALID_ARGUMENT;

		state = (struct i386_v86_assist_state *) tstate;
		state->int_table = thread->pcb->ims.v86s.int_table;
		state->int_count = thread->pcb->ims.v86s.int_count;

		*count = i386_V86_ASSIST_STATE_COUNT;
		break;
	    }
#endif
	    case i386_DEBUG_STATE:
	    {
		struct i386_debug_state *state;

		if (*count < i386_DEBUG_STATE_COUNT)
		    return KERN_INVALID_ARGUMENT;

		state = (struct i386_debug_state *) tstate;
		db_get_debug_state(thread->pcb, state);

		*count = i386_DEBUG_STATE_COUNT;
		break;
	    }
#if defined(__x86_64__) && !defined(USER32)
	    case i386_FSGS_BASE_STATE:
            {
                    struct i386_fsgs_base_state *state;
                    if (*count < i386_FSGS_BASE_STATE_COUNT)
                            return KERN_INVALID_ARGUMENT;

                    state = (struct i386_fsgs_base_state *) tstate;
                    state->fs_base = thread->pcb->ims.sbs.fsbase;
                    state->gs_base = thread->pcb->ims.sbs.gsbase;
                    *count = i386_FSGS_BASE_STATE_COUNT;
                    break;
            }
#endif
	    default:
		return(KERN_INVALID_ARGUMENT);
	}

	return(KERN_SUCCESS);
}

/*
 * Alter the thread`s state so that a following thread_exception_return
 * will make the thread return 'retval' from a syscall.
 */
void
thread_set_syscall_return(
	thread_t	thread,
	kern_return_t	retval)
{
	thread->pcb->iss.eax = retval;
}

/*
 * Return preferred address of user stack.
 * Always returns low address.  If stack grows up,
 * the stack grows away from this address;
 * if stack grows down, the stack grows towards this
 * address.
 */
vm_offset_t
user_stack_low(vm_size_t stack_size)
{
	return (VM_MAX_USER_ADDRESS - stack_size);
}

/*
 * Allocate argument area and set registers for first user thread.
 */
vm_offset_t
set_user_regs(vm_offset_t stack_base, /* low address */
	      vm_offset_t stack_size,
	      const struct exec_info *exec_info,
	      vm_size_t arg_size)
{
	vm_offset_t	arg_addr;
	struct i386_saved_state *saved_state;

	assert(P2ALIGNED(stack_size, USER_STACK_ALIGN));
	assert(P2ALIGNED(stack_base, USER_STACK_ALIGN));
	arg_size = P2ROUND(arg_size, USER_STACK_ALIGN);
	arg_addr = stack_base + stack_size - arg_size;

	saved_state = USER_REGS(current_thread());
	saved_state->uesp = (rpc_vm_offset_t)arg_addr;
	saved_state->eip = exec_info->entry;

	return (arg_addr);
}
