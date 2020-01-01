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
 *	Mach kernel startup.
 */

#include <string.h>

#include <mach/boolean.h>
#include <mach/machine.h>
#include <mach/task_special_ports.h>
#include <mach/vm_param.h>
#include <ipc/ipc_init.h>
#include <kern/cpu_number.h>
#include <kern/debug.h>
#include <kern/gsync.h>
#include <kern/machine.h>
#include <kern/mach_factor.h>
#include <kern/mach_clock.h>
#include <kern/processor.h>
#include <kern/rdxtree.h>
#include <kern/sched_prim.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/thread_swap.h>
#include <kern/timer.h>
#include <kern/xpr.h>
#include <kern/bootstrap.h>
#include <kern/time_stamp.h>
#include <kern/startup.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_init.h>
#include <vm/vm_pageout.h>
#include <machine/machspl.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/model_dep.h>
#include <mach/version.h>
#include <device/device_init.h>

#if MACH_KDB
#include <device/cons.h>
#endif /* MACH_KDB */

#if ! MACH_KBD
boolean_t reboot_on_panic = TRUE;
#endif

#if	NCPUS > 1
#include <machine/mp_desc.h>
#include <kern/machine.h>
#endif	/* NCPUS > 1 */

/* XX */
extern char *kernel_cmdline;

/*
 *	Running in virtual memory, on the interrupt stack.
 *	Does not return.  Dispatches initial thread.
 *
 *	Assumes that master_cpu is set.
 */
void setup_main(void)
{
	thread_t		startup_thread;
	phys_addr_t		memsize;

#if	MACH_KDB
	/*
	 * Cause a breakpoint trap to the debugger before proceeding
	 * any further if the proper option flag was specified
	 * on the kernel's command line.
	 * XXX check for surrounding spaces.
	 */
	if (strstr(kernel_cmdline, "-d ")) {
	    cninit();		/* need console for debugger */
	    SoftDebugger("init");
	}
#else	/* MACH_KDB */
	if (strstr (kernel_cmdline, "-H ")) {
	    reboot_on_panic = FALSE;
	}
#endif	/* MACH_KDB */

	panic_init();

	sched_init();
	vm_mem_bootstrap();
	rdxtree_cache_init();
	ipc_bootstrap();
	vm_mem_init();
	ipc_init();

	/*
	 * As soon as the virtual memory system is up, we record
	 * that this CPU is using the kernel pmap.
	 */
	PMAP_ACTIVATE_KERNEL(master_cpu);

	init_timers();
	init_timeout();

#if	XPR_DEBUG
	xprbootstrap();
#endif	/* XPR_DEBUG */

	timestamp_init();

	machine_init();

	mapable_time_init();

	machine_info.max_cpus = NCPUS;
	memsize = vm_page_mem_size();
	machine_info.memory_size = memsize;
	if (machine_info.memory_size < memsize)
		/* Overflow, report at least 4GB */
		machine_info.memory_size = ~0;
	machine_info.avail_cpus = 0;
	machine_info.major_version = KERNEL_MAJOR_VERSION;
	machine_info.minor_version = KERNEL_MINOR_VERSION;

	/*
	 *	Initialize the IPC, task, and thread subsystems.
	 */
	task_init();
	thread_init();
	swapper_init();
#if	MACH_HOST
	pset_sys_init();
#endif	/* MACH_HOST */

	/*
	 *	Kick off the time-out driven routines by calling
	 *	them the first time.
	 */
	recompute_priorities(NULL);
	compute_mach_factor();

	gsync_setup ();

	/*
	 *	Create a kernel thread to start the other kernel
	 *	threads.  Thread_resume (from kernel_thread) calls
	 *	thread_setrun, which may look at current thread;
	 *	we must avoid this, since there is no current thread.
	 */

	/*
	 * Create the thread, and point it at the routine.
	 */
	(void) thread_create(kernel_task, &startup_thread);
	thread_start(startup_thread, start_kernel_threads);

	/*
	 * Give it a kernel stack.
	 */
	thread_doswapin(startup_thread);

	/*
	 * Pretend it is already running, and resume it.
	 * Since it looks as if it is running, thread_resume
	 * will not try to put it on the run queues.
	 *
	 * We can do all of this without locking, because nothing
	 * else is running yet.
	 */
	startup_thread->state |= TH_RUN;
	(void) thread_resume(startup_thread);

	/*
	 * Start the thread.
	 */
	cpu_launch_first_thread(startup_thread);
	/*NOTREACHED*/
}

/*
 * Now running in a thread.  Create the rest of the kernel threads
 * and the bootstrap task.
 */
void start_kernel_threads(void)
{
	int	i;

	/*
	 *	Create the idle threads and the other
	 *	service threads.
	 */
	for (i = 0; i < NCPUS; i++) {
	    if (machine_slot[i].is_cpu) {
		thread_t	th;

		(void) thread_create(kernel_task, &th);
		thread_bind(th, cpu_to_processor(i));
		thread_start(th, idle_thread);
		thread_doswapin(th);
		(void) thread_resume(th);
	    }
	}

	(void) kernel_thread(kernel_task, reaper_thread, (char *) 0);
	(void) kernel_thread(kernel_task, swapin_thread, (char *) 0);
	(void) kernel_thread(kernel_task, sched_thread, (char *) 0);

#if	NCPUS > 1
	/*
	 *	Create the shutdown thread.
	 */
	(void) kernel_thread(kernel_task, action_thread, (char *) 0);

	/*
	 *	Allow other CPUs to run.
	 */
	start_other_cpus();
#endif	/* NCPUS > 1 */

	/*
	 *	Create the device service.
	 */
	device_service_create();

	/*
	 * 	Initialize kernel task's creation time.
	 * When we created the kernel task in task_init, the mapped
	 * time was not yet available.  Now, last thing before starting
	 * the user bootstrap, record the current time as the kernel
	 * task's creation time.
	 */
	record_time_stamp (&kernel_task->creation_time);

	/*
	 *	Start the user bootstrap.
	 */
	bootstrap_create();

#if	XPR_DEBUG
	xprinit();		/* XXX */
#endif	/* XPR_DEBUG */

	/*
	 *	Become the pageout daemon.
	 */
	(void) spl0();
	vm_pageout();
	/*NOTREACHED*/
}

#if	NCPUS > 1
void slave_main(void)
{
	cpu_launch_first_thread(THREAD_NULL);
}
#endif	/* NCPUS > 1 */

/*
 *	Start up the first thread on a CPU.
 *	First thread is specified for the master CPU.
 */
void cpu_launch_first_thread(thread_t th)
{
	int	mycpu;

	mycpu = cpu_number();

	cpu_up(mycpu);

	start_timer(&kernel_timer[mycpu]);

	/*
	 * Block all interrupts for choose_thread.
	 */
	(void) splhigh();

	if (th == THREAD_NULL)
	    th = choose_thread(cpu_to_processor(mycpu));
	if (th == THREAD_NULL)
	    panic("cpu_launch_first_thread");

	PMAP_ACTIVATE_KERNEL(mycpu);

	active_threads[mycpu] = th;
	active_stacks[mycpu] = th->kernel_stack;
	thread_lock(th);
	th->state &= ~TH_UNINT;
	thread_unlock(th);
	timer_switch(&th->system_timer);

	PMAP_ACTIVATE_USER(vm_map_pmap(th->task->map), th, mycpu);

	startrtclock();		/* needs an active thread */

	load_context(th);
	/*NOTREACHED*/
}
