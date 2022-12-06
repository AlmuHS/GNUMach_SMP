/*
 * Mach Operating System
 * Copyright (c) 1992,1991,1990,1989,1988 Carnegie Mellon University
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
 *	File:	vm_types.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date: 1985
 *
 *	Header file for VM data types.  I386 version.
 */

#ifndef	_MACHINE_VM_TYPES_H_
#define _MACHINE_VM_TYPES_H_	1

#ifdef	__ASSEMBLER__
#else	/* __ASSEMBLER__ */

#include <mach/machine/stdint.h>

#ifdef MACH_KERNEL
#include <kern/assert.h>
#endif

/*
 * A natural_t is the type for the native
 * unsigned integer type, usually 32 bits. It is suitable for
 * most counters with a small chance of overflow.
 * While historically natural_t was meant to be the same
 * as a pointer, that is not the case here.
 */
typedef unsigned int	natural_t;

/*
 * An integer_t is the signed counterpart
 * of the natural_t type. Both types are
 * only supposed to be used to define
 * other types in a machine-independent
 * way.
 */
typedef int		integer_t;

/*
 * A long_natural_t is a possibly larger unsigned integer type than natural_t.
 * Should be used instead of natural_t when we want the data to be less subject
 * to overflows.
 */
typedef unsigned long long_natural_t;

/*
 * A vm_offset_t is a type-neutral pointer,
 * e.g. an offset into a virtual memory space.
 */
typedef	__mach_uintptr_t	vm_offset_t;
typedef	vm_offset_t *	vm_offset_array_t;

/*
 * A type for physical addresses.
 */
#ifdef MACH_KERNEL
#ifdef PAE
typedef unsigned long long phys_addr_t;
#else /* PAE */
typedef unsigned long phys_addr_t;
#endif /* PAE */
#else
typedef unsigned long long phys_addr_t;
#endif
typedef unsigned long long rpc_phys_addr_t;

/*
 * A vm_size_t is the proper type for e.g.
 * expressing the difference between two
 * vm_offset_t entities.
 */
typedef __mach_uintptr_t vm_size_t;
typedef	vm_size_t *	vm_size_array_t;

/*
 * rpc_types are for user/kernel interfaces. On kernel side they may differ from
 * the native types, while on user space they shall be the same.
 * These three types are always of the same size, so we can reuse the conversion
 * functions.
 */
#if defined(MACH_KERNEL) && defined(USER32)
typedef __mach_uint32_t	rpc_vm_address_t;
typedef __mach_uint32_t	rpc_vm_offset_t;
typedef __mach_uint32_t	rpc_vm_size_t;
static inline __mach_uint64_t convert_vm_from_user(__mach_uint32_t uaddr)
{
    return (__mach_uint64_t)uaddr;
}
static inline __mach_uint32_t convert_vm_to_user(__mach_uint64_t kaddr)
{
    assert(kaddr <= 0xFFFFFFFF);
    return (__mach_uint32_t)kaddr;
}
typedef __mach_uint32_t rpc_long_natural_t;
#else /* MACH_KERNEL */
typedef vm_offset_t	rpc_vm_address_t;
typedef vm_offset_t	rpc_vm_offset_t;
typedef vm_size_t	rpc_vm_size_t;
#define convert_vm_to_user null_conversion
#define convert_vm_from_user null_conversion
typedef long_natural_t rpc_long_natural_t;
#endif /* MACH_KERNEL */

#endif	/* __ASSEMBLER__ */

/*
 * If composing messages by hand (please dont)
 */

#define	MACH_MSG_TYPE_INTEGER_T	MACH_MSG_TYPE_INTEGER_32

#endif	/* _MACHINE_VM_TYPES_H_ */
