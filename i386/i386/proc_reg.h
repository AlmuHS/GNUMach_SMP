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
 * Processor registers for i386 and i486.
 */
#ifndef	_I386_PROC_REG_H_
#define	_I386_PROC_REG_H_

/*
 * CR0
 */
#define	CR0_PG	0x80000000		/*	 enable paging */
#define	CR0_CD	0x40000000		/* i486: cache disable */
#define	CR0_NW	0x20000000		/* i486: no write-through */
#define	CR0_AM	0x00040000		/* i486: alignment check mask */
#define	CR0_WP	0x00010000		/* i486: write-protect kernel access */
#define	CR0_NE	0x00000020		/* i486: handle numeric exceptions */
#define	CR0_ET	0x00000010		/*	 extension type is 80387 */
					/*	 (not official) */
#define	CR0_TS	0x00000008		/*	 task switch */
#define	CR0_EM	0x00000004		/*	 emulate coprocessor */
#define	CR0_MP	0x00000002		/*	 monitor coprocessor */
#define	CR0_PE	0x00000001		/*	 enable protected mode */

/*
 * CR3
 */
#define	CR3_PCD	0x0010			/* Page-level Cache Disable */
#define	CR3_PWT	0x0008			/* Page-level Writes Transparent */

/*
 * CR4
 */
#define	CR4_VME		0x0001		/* Virtual-8086 Mode Extensions */
#define	CR4_PVI		0x0002		/* Protected-Mode Virtual Interrupts */
#define	CR4_TSD		0x0004		/* Time Stamp Disable */
#define	CR4_DE		0x0008		/* Debugging Extensions */
#define	CR4_PSE		0x0010		/* Page Size Extensions */
#define	CR4_PAE		0x0020		/* Physical Address Extension */
#define	CR4_MCE		0x0040		/* Machine-Check Enable */
#define	CR4_PGE		0x0080		/* Page Global Enable */
#define	CR4_PCE		0x0100		/* Performance-Monitoring Counter
					 * Enable */
#define	CR4_OSFXSR	0x0200		/* Operating System Support for FXSAVE
					 * and FXRSTOR instructions */
#define	CR4_OSXMMEXCPT	0x0400		/* Operating System Support for Unmasked
					 * SIMD Floating-Point Exceptions */
#define	CR4_OSXSAVE	0x40000		/* Operating System Support for XSAVE
					 * and XRSTOR instructions */

#ifndef	__ASSEMBLER__
#ifdef	__GNUC__

#ifndef	MACH_HYP
#include <i386/gdt.h>
#include <i386/ldt.h>
#endif	/* MACH_HYP */

static inline unsigned long
get_eflags(void)
{
	unsigned long eflags;
#ifdef __x86_64__
	asm("pushfq; popq %0" : "=r" (eflags));
#else
	asm("pushfl; popl %0" : "=r" (eflags));
#endif
	return eflags;
}

static inline void
set_eflags(unsigned long eflags)
{
#ifdef __x86_64__
	asm volatile("pushq %0; popfq" : : "r" (eflags));
#else
	asm volatile("pushl %0; popfl" : : "r" (eflags));
#endif
}

#define get_esp() \
    ({ \
	register unsigned long _temp__ asm("esp"); \
	_temp__; \
    })

#ifdef __x86_64__
#define get_eflags() \
    ({ \
	register unsigned long _temp__; \
	asm("pushfq; popq %0" : "=r" (_temp__)); \
	_temp__; \
    })
#else
#define get_eflags() \
    ({ \
	register unsigned long _temp__; \
	asm("pushfl; popl %0" : "=r" (_temp__)); \
	_temp__; \
    })
#endif

#define	get_cr0() \
    ({ \
     register unsigned long _temp__; \
	asm volatile("mov %%cr0, %0" : "=r" (_temp__)); \
	_temp__; \
    })

#define	set_cr0(value) \
    ({ \
	register unsigned long _temp__ = (value); \
	asm volatile("mov %0, %%cr0" : : "r" (_temp__)); \
     })

#define	get_cr2() \
    ({ \
	register unsigned long _temp__; \
	asm volatile("mov %%cr2, %0" : "=r" (_temp__)); \
	_temp__; \
    })

#ifdef	MACH_PV_PAGETABLES
extern unsigned long cr3;
#define get_cr3() (cr3)
#define	set_cr3(value) \
    ({ \
	cr3 = (value); \
	if (!hyp_set_cr3(value)) \
		panic("set_cr3"); \
    })
#else	/* MACH_PV_PAGETABLES */
#define	get_cr3() \
    ({ \
	register unsigned long _temp__; \
	asm volatile("mov %%cr3, %0" : "=r" (_temp__)); \
	_temp__; \
    })

#define	set_cr3(value) \
    ({ \
	register unsigned long _temp__ = (value); \
	asm volatile("mov %0, %%cr3" : : "r" (_temp__) : "memory"); \
     })
#endif	/* MACH_PV_PAGETABLES */

#define flush_tlb() set_cr3(get_cr3())

#ifndef	MACH_PV_PAGETABLES
#define invlpg(addr) \
    ({ \
	asm volatile("invlpg (%0)" : : "r" (addr)); \
    })

#define invlpg_linear(start) \
    ({ \
	asm volatile( \
		    "movw %w1,%%es\n" \
		  "\tinvlpg %%es:(%0)\n" \
		  "\tmovw %w2,%%es" \
		:: "r" (start), "q" (LINEAR_DS), "q" (KERNEL_DS)); \
    })

#define invlpg_linear_range(start, end) \
    ({ \
	register unsigned long var = trunc_page(start); \
	asm volatile( \
		    "movw %w2,%%es\n" \
		"1:\tinvlpg %%es:(%0)\n" \
		  "\taddl %c4,%0\n" \
		  "\tcmpl %0,%1\n" \
		  "\tjb 1b\n" \
		  "\tmovw %w3,%%es" \
		: "+r" (var) : "r" (end), \
		  "q" (LINEAR_DS), "q" (KERNEL_DS), "i" (PAGE_SIZE)); \
    })
#endif	/* MACH_PV_PAGETABLES */

#define	get_cr4() \
    ({ \
	register unsigned long _temp__; \
	asm volatile("mov %%cr4, %0" : "=r" (_temp__)); \
	_temp__; \
    })

#define	set_cr4(value) \
    ({ \
	register unsigned long _temp__ = (value); \
	asm volatile("mov %0, %%cr4" : : "r" (_temp__)); \
     })


#ifdef	MACH_RING1
#define	set_ts() \
	hyp_fpu_taskswitch(1)
#define	clear_ts() \
	hyp_fpu_taskswitch(0)
#else	/* MACH_RING1 */
#define	set_ts() \
	set_cr0(get_cr0() | CR0_TS)

#define	clear_ts() \
	asm volatile("clts")
#endif	/* MACH_RING1 */

#define	get_tr() \
    ({ \
	unsigned short _seg__; \
	asm volatile("str %0" : "=rm" (_seg__) ); \
	_seg__; \
    })

#define	set_tr(seg) \
	asm volatile("ltr %0" : : "rm" ((unsigned short)(seg)) )

#define	get_ldt() \
    ({ \
	unsigned short _seg__; \
	asm volatile("sldt %0" : "=rm" (_seg__) ); \
	_seg__; \
    })

#define	set_ldt(seg) \
	asm volatile("lldt %0" : : "rm" ((unsigned short)(seg)) )

/* This doesn't set a processor register,
   but it's often used immediately after setting one,
   to flush the instruction queue.  */
#define flush_instr_queue() \
	asm("jmp 0f\n" \
            "0:\n")

#ifdef MACH_RING1
#define get_dr0() hyp_get_debugreg(0)
#else
#define	get_dr0() \
    ({ \
	register unsigned long _temp__; \
	asm volatile("mov %%dr0, %0" : "=r" (_temp__)); \
	_temp__; \
    })
#endif

#ifdef MACH_RING1
#define set_dr0(value) hyp_set_debugreg(0, value)
#else
#define	set_dr0(value) \
    ({ \
	register unsigned long _temp__ = (value); \
	asm volatile("mov %0,%%dr0" : : "r" (_temp__)); \
    })
#endif

#ifdef MACH_RING1
#define get_dr1() hyp_get_debugreg(1)
#else
#define	get_dr1() \
    ({ \
	register unsigned long _temp__; \
	asm volatile("mov %%dr1, %0" : "=r" (_temp__)); \
	_temp__; \
    })
#endif

#ifdef MACH_RING1
#define set_dr1(value) hyp_set_debugreg(1, value)
#else
#define	set_dr1(value) \
    ({ \
	register unsigned long _temp__ = (value); \
	asm volatile("mov %0,%%dr1" : : "r" (_temp__)); \
    })
#endif

#ifdef MACH_RING1
#define get_dr2() hyp_get_debugreg(2)
#else
#define	get_dr2() \
    ({ \
	register unsigned long _temp__; \
	asm volatile("mov %%dr2, %0" : "=r" (_temp__)); \
	_temp__; \
    })
#endif

#ifdef MACH_RING1
#define set_dr2(value) hyp_set_debugreg(2, value)
#else
#define	set_dr2(value) \
    ({ \
	register unsigned long _temp__ = (value); \
	asm volatile("mov %0,%%dr2" : : "r" (_temp__)); \
    })
#endif

#ifdef MACH_RING1
#define get_dr3() hyp_get_debugreg(3)
#else
#define	get_dr3() \
    ({ \
	register unsigned long _temp__; \
	asm volatile("mov %%dr3, %0" : "=r" (_temp__)); \
	_temp__; \
    })
#endif

#ifdef MACH_RING1
#define set_dr3(value) hyp_set_debugreg(3, value)
#else
#define	set_dr3(value) \
    ({ \
	register unsigned long _temp__ = (value); \
	asm volatile("mov %0,%%dr3" : : "r" (_temp__)); \
    })
#endif

#ifdef MACH_RING1
#define get_dr6() hyp_get_debugreg(6)
#else
#define	get_dr6() \
    ({ \
	register unsigned long _temp__; \
	asm volatile("mov %%dr6, %0" : "=r" (_temp__)); \
	_temp__; \
    })
#endif

#ifdef MACH_RING1
#define set_dr6(value) hyp_set_debugreg(6, value)
#else
#define	set_dr6(value) \
    ({ \
	register unsigned long _temp__ = (value); \
	asm volatile("mov %0,%%dr6" : : "r" (_temp__)); \
    })
#endif

#ifdef MACH_RING1
#define get_dr7() hyp_get_debugreg(7)
#else
#define	get_dr7() \
    ({ \
	register unsigned long _temp__; \
	asm volatile("mov %%dr7, %0" : "=r" (_temp__)); \
	_temp__; \
    })
#endif

#ifdef MACH_RING1
#define set_dr7(value) hyp_set_debugreg(7, value)
#else
#define	set_dr7(value) \
    ({ \
	register unsigned long _temp__ = (value); \
	asm volatile("mov %0,%%dr7" : : "r" (_temp__)); \
    })
#endif

/* Note: gcc might want to use bx or the stack for %1 addressing, so we can't
 * use them :/ */
#ifdef __x86_64__
#define cpuid(eax, ebx, ecx, edx) \
{ \
	uint64_t sav_rbx; \
	asm(	"mov %%rbx,%2\n\t" \
		"cpuid\n\t" \
		"xchg %2,%%rbx\n\t" \
		"movl %k2,%1\n\t" \
		: "+a" (eax), "=m" (ebx), "=&r" (sav_rbx), "+c" (ecx), "=&d" (edx)); \
}
#else
#define cpuid(eax, ebx, ecx, edx) \
{ \
	asm (	"mov %%ebx,%1\n\t" \
		"cpuid\n\t" \
		"xchg %%ebx,%1\n\t" \
		: "+a" (eax), "=&SD" (ebx), "+c" (ecx), "=&d" (edx)); \
}
#endif

#endif	/* __GNUC__ */
#endif	/* __ASSEMBLER__ */

#endif	/* _I386_PROC_REG_H_ */
