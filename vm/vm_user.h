/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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
 *	File:	vm/vm_user.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1986
 *
 *	Declarations of user-visible virtual address space
 *	management functionality.
 */

#ifndef	_VM_VM_USER_H_
#define _VM_VM_USER_H_

#include <mach/kern_return.h>
#include <mach/std_types.h>

extern kern_return_t	vm_allocate(vm_map_t, vm_offset_t *, vm_size_t,
				    boolean_t);
extern kern_return_t	vm_deallocate(vm_map_t, vm_offset_t, vm_size_t);
extern kern_return_t	vm_inherit(vm_map_t, vm_offset_t, vm_size_t,
				   vm_inherit_t);
extern kern_return_t	vm_protect(vm_map_t, vm_offset_t, vm_size_t, boolean_t,
				   vm_prot_t);
extern kern_return_t	vm_statistics(vm_map_t, vm_statistics_data_t *);
extern kern_return_t	vm_read(vm_map_t, vm_address_t, vm_size_t, pointer_t *,
				vm_size_t *);
extern kern_return_t	vm_write(vm_map_t, vm_address_t, pointer_t, vm_size_t);
extern kern_return_t	vm_copy(vm_map_t, vm_address_t, vm_size_t,
				vm_address_t);
extern kern_return_t	vm_map(vm_map_t, vm_offset_t *, vm_size_t, vm_offset_t,
			       boolean_t, ipc_port_t, vm_offset_t, boolean_t,
			       vm_prot_t, vm_prot_t, vm_inherit_t);

#endif	/* _VM_VM_USER_H_ */
