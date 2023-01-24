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
/*
 * Type definitions for i386 interface routines.
 */

#ifndef	_MACH_MACH_I386_TYPES_H_
#define	_MACH_MACH_I386_TYPES_H_

#ifndef __ASSEMBLER__
/*
 * i386 segment descriptor.
 */
struct segment_descriptor {
	unsigned int	low_word;
	unsigned int	high_word;
};

typedef struct segment_descriptor descriptor_t;
typedef	struct segment_descriptor *descriptor_list_t;
typedef        const struct descriptor *const_segment_descriptor_list_t;

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

#endif /* !__ASSEMBLER__ */

/*
 * i386 I/O port
 */

#ifndef MACH_KERNEL
typedef unsigned short io_port_t;
typedef mach_port_t io_perm_t;
#endif /* !MACH_KERNEL */

#endif	/* _MACH_MACH_I386_TYPES_H_ */
