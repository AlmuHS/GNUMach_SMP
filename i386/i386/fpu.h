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

#ifndef	_I386_FPU_H_
#define	_I386_FPU_H_

/*
 * Macro definitions for routines to manipulate the
 * floating-point processor.
 */

#include <sys/types.h>
#include <i386/proc_reg.h>
#include <kern/thread.h>

/*
 * FPU instructions.
 */
#define	fninit() \
	asm volatile("fninit")

#define	fnstcw(control) \
	asm("fnstcw %0" : "=m" (*(unsigned short *)(control)))

#define	fstcw(control) \
	asm volatile("fstcw %0" : "=m" (*(unsigned short *)(control)))

#define	fldcw(control) \
	asm volatile("fldcw %0" : : "m" (*(unsigned short *) &(control)) )

#define	fnstsw() \
    ({ \
	unsigned short _status__; \
	asm("fnstsw %0" : "=ma" (_status__)); \
	_status__; \
    })

#define	fnclex() \
	asm volatile("fnclex")

#define	fnsave(state) \
	asm volatile("fnsave %0" : "=m" (*state))

#define	frstor(state) \
	asm volatile("frstor %0" : : "m" (state))

#define	fxsave(state) \
	asm volatile("fxsave %0" : "=m" (*state))

#define	fxrstor(state) \
	asm volatile("fxrstor %0" : : "m" (state))

#define fwait() \
    	asm("fwait");

#define	fpu_load_context(pcb)

/*
 * Save thread`s FPU context.
 * If only one CPU, we just set the task-switched bit,
 * to keep the new thread from using the coprocessor.
 * If multiple CPUs, we save the entire state.
 */
#if	NCPUS > 1
#define	fpu_save_context(thread) \
    { \
	register struct i386_fpsave_state *ifps; \
	ifps = (thread)->pcb->ims.ifps; \
	if (ifps != 0 && !ifps->fp_valid) { \
	    /* registers are in FPU - save to memory */ \
	    ifps->fp_valid = TRUE; \
	    if (fp_kind == FP_387X) \
		fxsave(&ifps->xfp_save_state); \
	    else \
		fnsave(&ifps->fp_save_state); \
	    set_ts(); \
	} \
    }
	    
#else	/* NCPUS == 1 */
#define	fpu_save_context(thread) \
    { \
	    set_ts(); \
    }

#endif	/* NCPUS == 1 */

extern int	fp_kind;
extern void fp_save(thread_t thread);
extern void fp_load(thread_t thread);
extern void fp_free(struct i386_fpsave_state *fps);
extern void fpu_module_init(void);
extern kern_return_t fpu_set_state(
    thread_t    thread,
    struct i386_float_state *state);
extern kern_return_t fpu_get_state(
    thread_t    thread,
    struct i386_float_state *state);
extern void fpnoextflt(void);
extern void fpextovrflt(void);
extern void fpexterrflt(void);
extern void fpastintr(void);
extern void init_fpu(void);

#endif	/* _I386_FPU_H_ */
