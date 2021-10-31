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

static inline uint64_t xgetbv(uint32_t n) {
	uint32_t eax, edx;
	asm volatile("xgetbv" : "=a" (eax), "=d" (edx) : "c" (n));
	return eax + ((uint64_t) edx << 32);
}

static inline uint64_t get_xcr0(void) {
	return xgetbv(0);
}

static inline void xsetbv(uint32_t n, uint64_t value) {
	uint32_t eax, edx;

	eax = value;
	edx = value >> 32;

	asm volatile("xsetbv" : : "c" (n), "a" (eax), "d" (edx));
}

static inline void set_xcr0(uint64_t value) {
	xsetbv(0, value);
}

#define CPU_XCR0_X87	(1 << 0)
#define CPU_XCR0_SSE	(1 << 1)
#define CPU_XCR0_AVX	(1 << 2)
#define CPU_XCR0_MPX	(3 << 3)
#define CPU_XCR0_AVX512	(7 << 5)

#define CPU_FEATURE_XSAVEOPT	(1 << 0)
#define CPU_FEATURE_XSAVEC	(1 << 1)
#define CPU_FEATURE_XGETBV1	(1 << 2)
#define CPU_FEATURE_XSAVES	(1 << 3)

#define	xsave(state) \
	asm volatile("xsave %0" \
			: "=m" (*state) \
			: "a" ((unsigned) fp_xsave_support) \
			, "d" ((unsigned) (fp_xsave_support >> 32)))

#define	xsaveopt(state) \
	asm volatile("xsaveopt %0" \
			: "=m" (*state) \
			: "a" ((unsigned) fp_xsave_support) \
			, "d" ((unsigned) (fp_xsave_support >> 32)))

#define	xsavec(state) \
	asm volatile("xsavec %0" \
			: "=m" (*state) \
			: "a" ((unsigned) fp_xsave_support) \
			, "d" ((unsigned) (fp_xsave_support >> 32)))

#define	xsaves(state) \
	asm volatile("xsaves %0" \
			: "=m" (*state) \
			: "a" ((unsigned) fp_xsave_support) \
			, "d" ((unsigned) (fp_xsave_support >> 32)))

#define	xrstor(state) \
	asm volatile("xrstor %0" : : "m" (state) \
			, "a" ((unsigned) fp_xsave_support) \
			, "d" ((unsigned) (fp_xsave_support >> 32)))

#define	xrstors(state) \
	asm volatile("xrstors %0" : : "m" (state) \
			, "a" ((unsigned) fp_xsave_support) \
			, "d" ((unsigned) (fp_xsave_support >> 32)))

#define fwait() \
    	asm("fwait");

#define	fpu_load_context(pcb)

#define fpu_save(ifps) \
    do { \
	switch (fp_save_kind) { \
	    case FP_XSAVE: \
		xsave(&(ifps)->xfp_save_state); \
		break; \
	    case FP_XSAVEOPT: \
		xsaveopt(&(ifps)->xfp_save_state); \
		break; \
	    case FP_XSAVEC: \
		xsavec(&(ifps)->xfp_save_state); \
		break; \
	    case FP_XSAVES: \
		xsaves(&(ifps)->xfp_save_state); \
		break; \
	    case FP_FXSAVE: \
		fxsave(&(ifps)->xfp_save_state); \
		break; \
	    case FP_FNSAVE: \
		fnsave(&(ifps)->fp_save_state); \
		break; \
	} \
	(ifps)->fp_valid = TRUE; \
    } while (0)

#define fpu_rstor(ifps) \
    do { \
	switch (fp_save_kind) { \
	    case FP_XSAVE: \
	    case FP_XSAVEOPT: \
	    case FP_XSAVEC: \
		xrstor((ifps)->xfp_save_state); \
		break; \
	    case FP_XSAVES: \
		xrstors((ifps)->xfp_save_state); \
		break; \
	    case FP_FXSAVE: \
		fxrstor((ifps)->xfp_save_state); \
		break; \
	    case FP_FNSAVE: \
		frstor((ifps)->fp_save_state); \
		break; \
	} \
    } while (0)

/*
 * Save thread`s FPU context.
 * If only one CPU, we just set the task-switched bit,
 * to keep the new thread from using the coprocessor.
 * If multiple CPUs, we save the entire state.
 */
#if	NCPUS > 1
#define	fpu_save_context(thread) \
    { \
	struct i386_fpsave_state *ifps; \
	ifps = (thread)->pcb->ims.ifps; \
	if (ifps != 0 && !ifps->fp_valid) { \
	    /* registers are in FPU - save to memory */ \
	    fpu_save(ifps); \
	    set_ts(); \
	} \
    }
	    
#else	/* NCPUS == 1 */
#define	fpu_save_context(thread) \
    { \
	    set_ts(); \
    }

#endif	/* NCPUS == 1 */

enum fp_save_kind {
	FP_FNSAVE,
	FP_FXSAVE,
	FP_XSAVE,
	FP_XSAVEOPT,
	FP_XSAVEC,
	FP_XSAVES,
};
extern int	fp_kind;
extern enum fp_save_kind	fp_save_kind;
extern struct i386_fpsave_state *fp_default_state;
extern uint64_t	fp_xsave_support;
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
extern void fpintr(int unit);
extern void fpinherit(thread_t parent_thread, thread_t thread);

#endif	/* _I386_FPU_H_ */
