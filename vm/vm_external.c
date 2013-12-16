/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
 *	This module maintains information about the presence of
 *	pages not in memory.  Since an external memory object
 *	must maintain a complete knowledge of its contents, this
 *	information takes the form of hints.
 */

#include <mach/boolean.h>
#include <kern/slab.h>
#include <vm/vm_external.h>
#include <mach/vm_param.h>
#include <kern/assert.h>



boolean_t	vm_external_unsafe = FALSE;

struct kmem_cache	vm_external_cache;

/*
 *	The implementation uses bit arrays to record whether
 *	a page has been written to external storage.  For
 *	convenience, these bit arrays come in two sizes
 *	(measured in bytes).
 */

#define		SMALL_SIZE	(VM_EXTERNAL_SMALL_SIZE/8)
#define		LARGE_SIZE	(VM_EXTERNAL_LARGE_SIZE/8)

struct kmem_cache	vm_object_small_existence_map_cache;
struct kmem_cache	vm_object_large_existence_map_cache;


vm_external_t	vm_external_create(size)
	vm_offset_t	size;
{
	vm_external_t	result;
	vm_size_t	bytes;
	
	result = (vm_external_t) kmem_cache_alloc(&vm_external_cache);
	result->existence_map = (char *) 0;

	bytes = (atop(size) + 07) >> 3;
	if (bytes <= SMALL_SIZE) {
		result->existence_map =
		 (char *) kmem_cache_alloc(&vm_object_small_existence_map_cache);
		result->existence_size = SMALL_SIZE;
	} else if (bytes <= LARGE_SIZE) {
		result->existence_map =
		 (char *) kmem_cache_alloc(&vm_object_large_existence_map_cache);
		result->existence_size = LARGE_SIZE;
	}
	return(result);
}

void		vm_external_destroy(e)
	vm_external_t	e;
{
	if (e == VM_EXTERNAL_NULL)
		return;

	if (e->existence_map != (char *) 0) {
		if (e->existence_size <= SMALL_SIZE) {
			kmem_cache_free(&vm_object_small_existence_map_cache,
				(vm_offset_t) e->existence_map);
		} else {
			kmem_cache_free(&vm_object_large_existence_map_cache,
				(vm_offset_t) e->existence_map);
		}
	}
	kmem_cache_free(&vm_external_cache, (vm_offset_t) e);
}

vm_external_state_t _vm_external_state_get(e, offset)
	const vm_external_t	e;
	vm_offset_t		offset;
{
	unsigned
	int		bit, byte;

	if (vm_external_unsafe ||
	    (e == VM_EXTERNAL_NULL) ||
	    (e->existence_map == (char *) 0))
		return(VM_EXTERNAL_STATE_UNKNOWN);

	bit = atop(offset);
	byte = bit >> 3;
	if (byte >= e->existence_size) return (VM_EXTERNAL_STATE_UNKNOWN);
	return( (e->existence_map[byte] & (1 << (bit & 07))) ?
		VM_EXTERNAL_STATE_EXISTS : VM_EXTERNAL_STATE_ABSENT );
}

void		vm_external_state_set(e, offset, state)
	vm_external_t	e;
	vm_offset_t	offset;
	vm_external_state_t state;
{
	unsigned
	int		bit, byte;

	if ((e == VM_EXTERNAL_NULL) || (e->existence_map == (char *) 0))
		return;

	if (state != VM_EXTERNAL_STATE_EXISTS)
		return;

	bit = atop(offset);
	byte = bit >> 3;
	if (byte >= e->existence_size) return;
	e->existence_map[byte] |= (1 << (bit & 07));
}

void		vm_external_module_initialize(void)
{
	vm_size_t	size = (vm_size_t) sizeof(struct vm_external);

	kmem_cache_init(&vm_external_cache, "vm_external", size, 0,
			NULL, NULL, NULL, 0);

	kmem_cache_init(&vm_object_small_existence_map_cache,
			"small_existence_map", SMALL_SIZE, 0,
			NULL, NULL, NULL, 0);

	kmem_cache_init(&vm_object_large_existence_map_cache,
			"large_existence_map", LARGE_SIZE, 0,
			NULL, NULL, NULL, 0);
}
