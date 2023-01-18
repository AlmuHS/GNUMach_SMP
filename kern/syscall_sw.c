/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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

#include <mach/port.h>
#include <mach/kern_return.h>
#include <kern/debug.h>
#include <kern/syscall_sw.h>

/* Include declarations of the trap functions. */
#include <mach/mach_traps.h>
#include <mach/message.h>
#include <kern/syscall_subr.h>
#include <kern/ipc_mig.h>
#include <kern/eventcount.h>
#include <ipc/mach_port.server.h>


/*
 *	To add a new entry:
 *		Add an "MACH_TRAP(routine, arg count)" to the table below.
 *
 *		Add trap definition to mach/syscall_sw.h and
 *		recompile user library.
 *
 * WARNING:	If you add a trap which requires more than 7
 *		parameters, mach/ca/syscall_sw.h and ca/trap.c both need
 *		to be modified for it to work successfully on an
 *		RT.  Similarly, mach/mips/syscall_sw.h and mips/locore.s
 *		need to be modified before it will work on Pmaxen.
 *
 * WARNING:	Don't use numbers 0 through -9.  They (along with
 *		the positive numbers) are reserved for Unix.
 */

boolean_t kern_invalid_debug = FALSE;

static mach_port_name_t	null_port(void)
{
	if (kern_invalid_debug) SoftDebugger("null_port mach trap");
	return(MACH_PORT_NULL);
}

static kern_return_t	kern_invalid(void)
{
	if (kern_invalid_debug) SoftDebugger("kern_invalid mach trap");
	return(KERN_INVALID_ARGUMENT);
}

mach_trap_t	mach_trap_table[] = {
	MACH_TRAP(kern_invalid, 0),		/* 0 */		/* Unix */
	MACH_TRAP(kern_invalid, 0),		/* 1 */		/* Unix */
	MACH_TRAP(kern_invalid, 0),		/* 2 */		/* Unix */
	MACH_TRAP(kern_invalid, 0),		/* 3 */		/* Unix */
	MACH_TRAP(kern_invalid, 0),		/* 4 */		/* Unix */
	MACH_TRAP(kern_invalid, 0),		/* 5 */		/* Unix */
	MACH_TRAP(kern_invalid, 0),		/* 6 */		/* Unix */
	MACH_TRAP(kern_invalid, 0),		/* 7 */		/* Unix */
	MACH_TRAP(kern_invalid, 0),		/* 8 */		/* Unix */
	MACH_TRAP(kern_invalid, 0),		/* 9 */		/* Unix */
	MACH_TRAP(null_port, 0),		/* 10 */
	MACH_TRAP(null_port, 0),		/* 11 */
	MACH_TRAP(null_port, 0),		/* 12 */
	MACH_TRAP(null_port, 0),		/* 13 */
	MACH_TRAP(kern_invalid, 0),		/* 14 */
	MACH_TRAP(kern_invalid, 0),		/* 15 */
	MACH_TRAP(kern_invalid, 0),		/* 16 */
	MACH_TRAP_STACK(evc_wait, 1),		/* 17 */
	MACH_TRAP_STACK(evc_wait_clear, 1),	/* 18 */
	MACH_TRAP(kern_invalid, 0),		/* 19 */
	MACH_TRAP(kern_invalid, 0),		/* 20 */
	MACH_TRAP(kern_invalid, 0),		/* 21 */
	MACH_TRAP(kern_invalid, 0),		/* 22 */
	MACH_TRAP(kern_invalid, 0),		/* 23 */
	MACH_TRAP(kern_invalid, 0),		/* 24 */
	MACH_TRAP_STACK(mach_msg_trap, 7),	/* 25 */
	MACH_TRAP(mach_reply_port, 0),		/* 26 */
	MACH_TRAP(mach_thread_self, 0),		/* 27 */
	MACH_TRAP(mach_task_self, 0),		/* 28 */
	MACH_TRAP(mach_host_self, 0),		/* 29 */
#ifdef MACH_KDB
	MACH_TRAP_STACK(mach_print, 1),		/* 30 */
#else /* MACH_KDB */
	MACH_TRAP_STACK(kern_invalid, 0),	/* 30 */
#endif /* MACH_KDB */

	MACH_TRAP(kern_invalid, 0),		/* 31 */
	MACH_TRAP(kern_invalid, 0),		/* 32 */
	MACH_TRAP(kern_invalid, 0),		/* 33 emul: task_by_pid */
	MACH_TRAP(kern_invalid, 0),		/* 34 emul: pid_by_task */
	MACH_TRAP(kern_invalid, 0),		/* 35 */
	MACH_TRAP(kern_invalid, 0),		/* 36 */
	MACH_TRAP(kern_invalid, 0),		/* 37 */
	MACH_TRAP(kern_invalid, 0),		/* 38 */

 	MACH_TRAP(syscall_device_writev_request, 6),	/* 39 */
 	MACH_TRAP(syscall_device_write_request, 6),	/* 40 */

	MACH_TRAP(kern_invalid, 0),		/* 41 emul: init_process */
	MACH_TRAP(kern_invalid, 0),		/* 42 */
	MACH_TRAP(kern_invalid, 0),		/* 43 emul: map_fd */
	MACH_TRAP(kern_invalid, 0),		/* 44 emul: rfs_make_symlink */
	MACH_TRAP(kern_invalid, 0),		/* 45 */
	MACH_TRAP(kern_invalid, 0),		/* 46 */
	MACH_TRAP(kern_invalid, 0),		/* 47 */
	MACH_TRAP(kern_invalid, 0),		/* 48 */
	MACH_TRAP(kern_invalid, 0),		/* 49 */

	MACH_TRAP(kern_invalid, 0),		/* 50 */
	MACH_TRAP(kern_invalid, 0),		/* 51 */
	MACH_TRAP(kern_invalid, 0),		/* 52 emul: htg_syscall */
	MACH_TRAP(kern_invalid, 0),	        /* 53 emul: set_ras_address */
	MACH_TRAP(kern_invalid, 0),	        /* 54 */
	MACH_TRAP(null_port, 0),		/* 55 */
	MACH_TRAP(null_port, 0),		/* 56 */
	MACH_TRAP(kern_invalid, 0),		/* 57 */
	MACH_TRAP(kern_invalid, 0),		/* 58 */
 	MACH_TRAP_STACK(swtch_pri, 1),		/* 59 */

	MACH_TRAP_STACK(swtch, 0),		/* 60 */
	MACH_TRAP_STACK(thread_switch, 3),	/* 61 */
	MACH_TRAP(kern_invalid, 0),		/* 62 */
	MACH_TRAP(kern_invalid, 0),		/* 63 */
	MACH_TRAP(syscall_vm_map, 11),			/* 64 */
	MACH_TRAP(syscall_vm_allocate, 4),		/* 65 */
	MACH_TRAP(syscall_vm_deallocate, 3),		/* 66 */
	MACH_TRAP(kern_invalid, 0),			/* 67 */
	MACH_TRAP(syscall_task_create, 3),		/* 68 */
	MACH_TRAP(syscall_task_terminate, 1),		/* 69 */

	MACH_TRAP(syscall_task_suspend, 1),		/* 70 */
	MACH_TRAP(syscall_task_set_special_port, 3),	/* 71 */
	MACH_TRAP(syscall_mach_port_allocate, 3),	/* 72 */
	MACH_TRAP(syscall_mach_port_deallocate, 2),	/* 73 */
	MACH_TRAP(syscall_mach_port_insert_right, 4),	/* 74 */
	MACH_TRAP(syscall_mach_port_allocate_name, 3),	/* 75 */
	MACH_TRAP(syscall_thread_depress_abort, 1),	/* 76 */
	MACH_TRAP(kern_invalid, 0),		/* 77 */
	MACH_TRAP(kern_invalid, 0),		/* 78 */
	MACH_TRAP(kern_invalid, 0),		/* 79 */

	MACH_TRAP(kern_invalid, 0),                   /* 80 */
	MACH_TRAP(kern_invalid, 0),                   /* 81 */
	MACH_TRAP(kern_invalid, 0),		      /* 82 */
	MACH_TRAP(kern_invalid, 0),		      /* 83 */
	MACH_TRAP(kern_invalid, 0),	              /* 84 */
	MACH_TRAP(kern_invalid, 0),		      /* 85 */
	MACH_TRAP(kern_invalid, 0),	              /* 86 */
	MACH_TRAP(kern_invalid, 0),		      /* 87 */
	MACH_TRAP(kern_invalid, 0),		      /* 88 */
	MACH_TRAP(kern_invalid, 0),		      /* 89 */
	MACH_TRAP(kern_invalid, 0),		      /* 90 */
	MACH_TRAP(kern_invalid, 0),	              /* 91 */
	MACH_TRAP(kern_invalid, 0),                   /* 92 */
	MACH_TRAP(kern_invalid, 0),	              /* 93 */
	MACH_TRAP(kern_invalid, 0),                   /* 94 */
	MACH_TRAP(kern_invalid, 0),                   /* 95 */

	MACH_TRAP(kern_invalid, 0),		      /* 96 */
	MACH_TRAP(kern_invalid, 0),		      /* 97 */

	MACH_TRAP(kern_invalid, 0),		      /* 98 */
	MACH_TRAP(kern_invalid, 0),		      /* 99 */

	MACH_TRAP(kern_invalid, 0),		/* 100 */
	MACH_TRAP(kern_invalid, 0),		/* 101 */
	MACH_TRAP(kern_invalid, 0),		/* 102 */
	MACH_TRAP(kern_invalid, 0),		/* 103 */
	MACH_TRAP(kern_invalid, 0),		/* 104 */
	MACH_TRAP(kern_invalid, 0),		/* 105 */
	MACH_TRAP(kern_invalid, 0),		/* 106 */
	MACH_TRAP(kern_invalid, 0),		/* 107 */
	MACH_TRAP(kern_invalid, 0),		/* 108 */
	MACH_TRAP(kern_invalid, 0),		/* 109 */

	MACH_TRAP(kern_invalid, 0),		/* 110 */
	MACH_TRAP(kern_invalid, 0),		/* 111 */
	MACH_TRAP(kern_invalid, 0),		/* 112 */
	MACH_TRAP(kern_invalid, 0),		/* 113 */
	MACH_TRAP(kern_invalid, 0),		/* 114 */
	MACH_TRAP(kern_invalid, 0),		/* 115 */
	MACH_TRAP(kern_invalid, 0),		/* 116 */
	MACH_TRAP(kern_invalid, 0),		/* 117 */
	MACH_TRAP(kern_invalid, 0),		/* 118 */
	MACH_TRAP(kern_invalid, 0),		/* 119 */

	MACH_TRAP(kern_invalid, 0),		/* 120 */
	MACH_TRAP(kern_invalid, 0),		/* 121 */
	MACH_TRAP(kern_invalid, 0),		/* 122 */
	MACH_TRAP(kern_invalid, 0),		/* 123 */
	MACH_TRAP(kern_invalid, 0),		/* 124 */
	MACH_TRAP(kern_invalid, 0),		/* 125 */
	MACH_TRAP(kern_invalid, 0),		/* 126 */
	MACH_TRAP(kern_invalid, 0),		/* 127 */
	MACH_TRAP(kern_invalid, 0),		/* 128 */
	MACH_TRAP(kern_invalid, 0),		/* 129 */
};

int	mach_trap_count = (sizeof(mach_trap_table) / sizeof(mach_trap_table[0]));
