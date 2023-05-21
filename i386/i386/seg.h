/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.
 *
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
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

#ifndef	_I386_SEG_H_
#define	_I386_SEG_H_

#include <mach/inline.h>
#include <mach/machine/vm_types.h>

/*
 * i386 segmentation.
 */

/* Note: the value of KERNEL_RING is handled by hand in locore.S */
#ifdef	MACH_RING1
#define	KERNEL_RING	1
#else	/* MACH_RING1 */
#define	KERNEL_RING	0
#endif	/* MACH_RING1 */

#ifndef __ASSEMBLER__

/*
 * Real segment descriptor.
 */
struct real_descriptor {
	unsigned int	limit_low:16,	/* limit 0..15 */
			base_low:16,	/* base  0..15 */
			base_med:8,	/* base  16..23 */
			access:8,	/* access byte */
			limit_high:4,	/* limit 16..19 */
			granularity:4,	/* granularity */
			base_high:8;	/* base 24..31 */
};
typedef struct real_descriptor real_descriptor_t;
typedef real_descriptor_t *real_descriptor_list_t;
typedef const real_descriptor_list_t const_real_descriptor_list_t;

#ifdef __x86_64__
struct real_descriptor64 {
	unsigned int	limit_low:16,	/* limit 0..15 */
			base_low:16,	/* base  0..15 */
			base_med:8,	/* base  16..23 */
			access:8,	/* access byte */
			limit_high:4,	/* limit 16..19 */
			granularity:4,	/* granularity */
			base_high:8,	/* base 24..31 */
			base_ext:32,	/* base 32..63 */
			reserved1:8,
			zero:5,
			reserved2:19;
};
#endif

struct real_gate {
	unsigned int	offset_low:16,	/* offset 0..15 */
			selector:16,
			word_count:8,
			access:8,
			offset_high:16;	/* offset 16..31 */
#ifdef __x86_64__
	unsigned int	offset_ext:32,	/* offset 32..63 */
			reserved:32;
#endif
};

#endif /* !__ASSEMBLER__ */

#define	SZ_64		0x2			/* 64-bit segment */
#define	SZ_32		0x4			/* 32-bit segment */
#define SZ_16		0x0			/* 16-bit segment */
#define	SZ_G		0x8			/* 4K limit field */

#define	ACC_A		0x01			/* accessed */
#define	ACC_TYPE	0x1e			/* type field: */

#define	ACC_TYPE_SYSTEM	0x00			/* system descriptors: */

#define	ACC_LDT		0x02			    /* LDT */
#define	ACC_CALL_GATE_16 0x04			    /* 16-bit call gate */
#define	ACC_TASK_GATE	0x05			    /* task gate */
#define	ACC_TSS		0x09			    /* task segment */
#define	ACC_CALL_GATE	0x0c			    /* call gate */
#define	ACC_INTR_GATE	0x0e			    /* interrupt gate */
#define	ACC_TRAP_GATE	0x0f			    /* trap gate */

#define	ACC_TSS_BUSY	0x02			    /* task busy */

#define	ACC_TYPE_USER	0x10			/* user descriptors */

#define	ACC_DATA	0x10			    /* data */
#define	ACC_DATA_W	0x12			    /* data, writable */
#define	ACC_DATA_E	0x14			    /* data, expand-down */
#define	ACC_DATA_EW	0x16			    /* data, expand-down,
							     writable */
#define	ACC_CODE	0x18			    /* code */
#define	ACC_CODE_R	0x1a			    /* code, readable */
#define	ACC_CODE_C	0x1c			    /* code, conforming */
#define	ACC_CODE_CR	0x1e			    /* code, conforming,
						       readable */
#define	ACC_PL		0x60			/* access rights: */
#define	ACC_PL_K	(KERNEL_RING << 5)	/* kernel access only */
#define	ACC_PL_U	0x60			/* user access */
#define	ACC_P		0x80			/* segment present */

/*
 * Components of a selector
 */
#define	SEL_LDT		0x04			/* local selector */
#define	SEL_PL		0x03			/* privilege level: */
#define	SEL_PL_K	KERNEL_RING		    /* kernel selector */
#define	SEL_PL_U	0x03			    /* user selector */

/*
 * Convert selector to descriptor table index.
 */
#define	sel_idx(sel)	((sel)>>3)


#ifndef __ASSEMBLER__

#include <mach/inline.h>
#include <mach/xen.h>


/* Format of a "pseudo-descriptor", used for loading the IDT and GDT.  */
struct pseudo_descriptor
{
	unsigned short limit;
	unsigned long linear_base;
	short pad;
} __attribute__((packed));


/* Load the processor's IDT, GDT, or LDT pointers.  */
static inline void lgdt(struct pseudo_descriptor *pdesc)
{
	__asm volatile("lgdt %0" : : "m" (*pdesc));
}
static inline void lidt(struct pseudo_descriptor *pdesc)
{
	__asm volatile("lidt %0" : : "m" (*pdesc));
}
static inline void lldt(unsigned short ldt_selector)
{
	__asm volatile("lldt %w0" : : "r" (ldt_selector) : "memory");
}

#ifdef CODE16
#define i16_lgdt lgdt
#define i16_lidt lidt
#define i16_lldt lldt
#endif


/* Fill a segment descriptor.  */
static inline void
fill_descriptor(struct real_descriptor *_desc, vm_offset_t base, vm_offset_t limit,
		unsigned char access, unsigned char sizebits)
{
	/* TODO: when !MACH_PV_DESCRIPTORS, setting desc and just memcpy isn't simpler actually */
#ifdef	MACH_PV_DESCRIPTORS
	struct real_descriptor __desc, *desc = &__desc;
#else	/* MACH_PV_DESCRIPTORS */
	struct real_descriptor *desc = _desc;
#endif	/* MACH_PV_DESCRIPTORS */
	if (limit > 0xfffff)
	{
		limit >>= 12;
		sizebits |= SZ_G;
	}
	desc->limit_low = limit & 0xffff;
	desc->base_low = base & 0xffff;
	desc->base_med = (base >> 16) & 0xff;
	desc->access = access | ACC_P;
	desc->limit_high = limit >> 16;
	desc->granularity = sizebits;
	desc->base_high = base >> 24;
#ifdef	MACH_PV_DESCRIPTORS
	if (hyp_do_update_descriptor(kv_to_ma(_desc), *(uint64_t*)desc))
		panic("couldn't update descriptor(%zu to %08lx%08lx)\n", (vm_offset_t) kv_to_ma(_desc), *(((unsigned long*)desc)+1), *(unsigned long *)desc);
#endif	/* MACH_PV_DESCRIPTORS */
}

#ifdef __x86_64__
static inline void
fill_descriptor64(struct real_descriptor64 *_desc, unsigned long base, unsigned limit,
		  unsigned char access, unsigned char sizebits)
{
	/* TODO: when !MACH_PV_DESCRIPTORS, setting desc and just memcpy isn't simpler actually */
#ifdef	MACH_PV_DESCRIPTORS
	struct real_descriptor64 __desc, *desc = &__desc;
#else	/* MACH_PV_DESCRIPTORS */
	struct real_descriptor64 *desc = _desc;
#endif	/* MACH_PV_DESCRIPTORS */
	if (limit > 0xfffff)
	{
		limit >>= 12;
		sizebits |= SZ_G;
	}
	desc->limit_low = limit & 0xffff;
	desc->base_low = base & 0xffff;
	desc->base_med = (base >> 16) & 0xff;
	desc->access = access | ACC_P;
	desc->limit_high = limit >> 16;
	desc->granularity = sizebits;
	desc->base_high = base >> 24;
	desc->base_ext = base >> 32;
	desc->reserved1 = 0;
	desc->zero = 0;
	desc->reserved2 = 0;
#ifdef	MACH_PV_DESCRIPTORS
	if (hyp_do_update_descriptor(kv_to_ma(_desc), *(uint64_t*)desc))
		panic("couldn't update descriptor(%lu to %08lx%08lx)\n", (vm_offset_t) kv_to_ma(_desc), *(((unsigned long*)desc)+1), *(unsigned long *)desc);
#endif	/* MACH_PV_DESCRIPTORS */
}
#endif

/* Fill a gate with particular values.  */
static inline void
fill_gate(struct real_gate *gate, unsigned long offset, unsigned short selector,
	  unsigned char access, unsigned char word_count)
{
	gate->offset_low = offset & 0xffff;
	gate->selector = selector;
	gate->word_count = word_count;
	gate->access = access | ACC_P;
	gate->offset_high = (offset >> 16) & 0xffff;
#ifdef __x86_64__
	gate->offset_ext = offset >> 32;
	gate->reserved = 0;
#endif
}

#endif /* !__ASSEMBLER__ */

#endif	/* _I386_SEG_H_ */
