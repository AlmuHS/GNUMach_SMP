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
 *	File:	ipc/ipc_hash.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Entry hash table operations.
 */

#include <kern/printf.h>
#include <mach/boolean.h>
#include <mach/port.h>
#include <kern/lock.h>
#include <kern/kalloc.h>
#include <ipc/port.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_hash.h>
#include <ipc/ipc_init.h>
#include <ipc/ipc_types.h>

#if	MACH_IPC_DEBUG
#include <mach/kern_return.h>
#include <mach_debug/hash_info.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_user.h>
#endif



/*
 *	Routine:	ipc_hash_lookup
 *	Purpose:
 *		Converts (space, obj) -> (name, entry).
 *		Returns TRUE if an entry was found.
 *	Conditions:
 *		The space must be locked (read or write) throughout.
 */

boolean_t
ipc_hash_lookup(
	ipc_space_t space,
	ipc_object_t obj,
	mach_port_t *namep,
	ipc_entry_t *entryp)
{
	return (ipc_hash_local_lookup(space, obj, namep, entryp) ||
		((space->is_tree_hash > 0) &&
		 ipc_hash_global_lookup(space, obj, namep,
					(ipc_tree_entry_t *) entryp)));
}

/*
 *	Routine:	ipc_hash_insert
 *	Purpose:
 *		Inserts an entry into the appropriate reverse hash table,
 *		so that ipc_hash_lookup will find it.
 *	Conditions:
 *		The space must be write-locked.
 */

void
ipc_hash_insert(
	ipc_space_t	space,
	ipc_object_t	obj,
	mach_port_t	name,
	ipc_entry_t	entry)
{
	mach_port_index_t index;

	index = MACH_PORT_INDEX(name);
	if ((index < space->is_table_size) &&
	    (entry == &space->is_table[index]))
		ipc_hash_local_insert(space, obj, index, entry);
	else
		ipc_hash_global_insert(space, obj, name,
				       (ipc_tree_entry_t) entry);
}

/*
 *	Routine:	ipc_hash_delete
 *	Purpose:
 *		Deletes an entry from the appropriate reverse hash table.
 *	Conditions:
 *		The space must be write-locked.
 */

void
ipc_hash_delete(
	ipc_space_t	space,
	ipc_object_t	obj,
	mach_port_t	name,
	ipc_entry_t	entry)
{
	mach_port_index_t index;

	index = MACH_PORT_INDEX(name);
	if ((index < space->is_table_size) &&
	    (entry == &space->is_table[index]))
		ipc_hash_local_delete(space, obj, index, entry);
	else
		ipc_hash_global_delete(space, obj, name,
				       (ipc_tree_entry_t) entry);
}

/*
 *	The global reverse hash table holds splay tree entries.
 *	It is a simple open-chaining hash table with singly-linked buckets.
 *	Each bucket is locked separately, with an exclusive lock.
 *	Within each bucket, move-to-front is used.
 */

ipc_hash_index_t ipc_hash_global_size;
ipc_hash_index_t ipc_hash_global_mask;

#define IH_GLOBAL_HASH(space, obj)					\
	(((((ipc_hash_index_t) ((vm_offset_t)space)) >> 4) +		\
	  (((ipc_hash_index_t) ((vm_offset_t)obj)) >> 6)) &		\
	 ipc_hash_global_mask)

typedef struct ipc_hash_global_bucket {
	decl_simple_lock_data(, ihgb_lock_data)
	ipc_tree_entry_t ihgb_head;
} *ipc_hash_global_bucket_t;

#define	IHGB_NULL	((ipc_hash_global_bucket_t) 0)

#define	ihgb_lock_init(ihgb)	simple_lock_init(&(ihgb)->ihgb_lock_data)
#define	ihgb_lock(ihgb)		simple_lock(&(ihgb)->ihgb_lock_data)
#define	ihgb_unlock(ihgb)	simple_unlock(&(ihgb)->ihgb_lock_data)

ipc_hash_global_bucket_t ipc_hash_global_table;

/*
 *	Routine:	ipc_hash_global_lookup
 *	Purpose:
 *		Converts (space, obj) -> (name, entry).
 *		Looks in the global table, for splay tree entries.
 *		Returns TRUE if an entry was found.
 *	Conditions:
 *		The space must be locked (read or write) throughout.
 */

boolean_t
ipc_hash_global_lookup(
	ipc_space_t		space,
	ipc_object_t		obj,
	mach_port_t		*namep,
	ipc_tree_entry_t	*entryp)
{
	ipc_hash_global_bucket_t bucket;
	ipc_tree_entry_t this, *last;

	assert(space != IS_NULL);
	assert(obj != IO_NULL);

	bucket = &ipc_hash_global_table[IH_GLOBAL_HASH(space, obj)];
	ihgb_lock(bucket);

	if ((this = bucket->ihgb_head) != ITE_NULL) {
		if ((this->ite_object == obj) &&
		    (this->ite_space == space)) {
			/* found it at front; no need to move */

			*namep = this->ite_name;
			*entryp = this;
		} else for (last = &this->ite_next;
			    (this = *last) != ITE_NULL;
			    last = &this->ite_next) {
			if ((this->ite_object == obj) &&
			    (this->ite_space == space)) {
				/* found it; move to front */

				*last = this->ite_next;
				this->ite_next = bucket->ihgb_head;
				bucket->ihgb_head = this;

				*namep = this->ite_name;
				*entryp = this;
				break;
			}
		}
	}

	ihgb_unlock(bucket);
	return this != ITE_NULL;
}

/*
 *	Routine:	ipc_hash_global_insert
 *	Purpose:
 *		Inserts an entry into the global reverse hash table.
 *	Conditions:
 *		The space must be write-locked.
 */

void
ipc_hash_global_insert(
	ipc_space_t		space,
	ipc_object_t		obj,
	mach_port_t		name,
	ipc_tree_entry_t	entry)
{
	ipc_hash_global_bucket_t bucket;


	assert(entry->ite_name == name);
	assert(space != IS_NULL);
	assert(entry->ite_space == space);
	assert(obj != IO_NULL);
	assert(entry->ite_object == obj);

	space->is_tree_hash++;
	assert(space->is_tree_hash <= space->is_tree_total);

	bucket = &ipc_hash_global_table[IH_GLOBAL_HASH(space, obj)];
	ihgb_lock(bucket);

	/* insert at front of bucket */

	entry->ite_next = bucket->ihgb_head;
	bucket->ihgb_head = entry;

	ihgb_unlock(bucket);
}

/*
 *	Routine:	ipc_hash_global_delete
 *	Purpose:
 *		Deletes an entry from the global reverse hash table.
 *	Conditions:
 *		The space must be write-locked.
 */

void
ipc_hash_global_delete(
	ipc_space_t		space,
	ipc_object_t		obj,
	mach_port_t		name,
	ipc_tree_entry_t	entry)
{
	ipc_hash_global_bucket_t bucket;
	ipc_tree_entry_t this, *last;

	assert(entry->ite_name == name);
	assert(space != IS_NULL);
	assert(entry->ite_space == space);
	assert(obj != IO_NULL);
	assert(entry->ite_object == obj);

	assert(space->is_tree_hash > 0);
	space->is_tree_hash--;

	bucket = &ipc_hash_global_table[IH_GLOBAL_HASH(space, obj)];
	ihgb_lock(bucket);

	for (last = &bucket->ihgb_head;
	     (this = *last) != ITE_NULL;
	     last = &this->ite_next) {
		if (this == entry) {
			/* found it; remove from bucket */

			*last = this->ite_next;
			break;
		}
	}
	assert(this != ITE_NULL);

	ihgb_unlock(bucket);
}

/*
 *	Each space has a local reverse mapping from objects to entries
 *	from the space's table.  This used to be a hash table.
 */

#define IPC_LOCAL_HASH_INVARIANT(S, O, N, E)				\
	MACRO_BEGIN							\
	assert(IE_BITS_TYPE((E)->ie_bits) == MACH_PORT_TYPE_SEND ||	\
	       IE_BITS_TYPE((E)->ie_bits) == MACH_PORT_TYPE_SEND_RECEIVE || \
	       IE_BITS_TYPE((E)->ie_bits) == MACH_PORT_TYPE_NONE);	\
	assert((E)->ie_object == (O));					\
	assert((E)->ie_index == (N));					\
	assert(&(S)->is_table[N] == (E));				\
	MACRO_END

/*
 *	Routine:	ipc_hash_local_lookup
 *	Purpose:
 *		Converts (space, obj) -> (name, entry).
 *		Looks in the space's local table, for table entries.
 *		Returns TRUE if an entry was found.
 *	Conditions:
 *		The space must be locked (read or write) throughout.
 */

boolean_t
ipc_hash_local_lookup(
	ipc_space_t	space,
	ipc_object_t	obj,
	mach_port_t	*namep,
	ipc_entry_t	*entryp)
{
	assert(space != IS_NULL);
	assert(obj != IO_NULL);

	*entryp = ipc_reverse_lookup(space, obj);
	if (*entryp != IE_NULL) {
		*namep = (*entryp)->ie_index;
		IPC_LOCAL_HASH_INVARIANT(space, obj, *namep, *entryp);
		return TRUE;
	}
	return FALSE;
}

/*
 *	Routine:	ipc_hash_local_insert
 *	Purpose:
 *		Inserts an entry into the space's reverse hash table.
 *	Conditions:
 *		The space must be write-locked.
 */

void
ipc_hash_local_insert(
	ipc_space_t		space,
	ipc_object_t		obj,
	mach_port_index_t	index,
	ipc_entry_t		entry)
{
	kern_return_t kr;
	assert(index != 0);
	assert(space != IS_NULL);
	assert(obj != IO_NULL);

	entry->ie_index = index;
	IPC_LOCAL_HASH_INVARIANT(space, obj, index, entry);
	kr = ipc_reverse_insert(space, obj, entry);
	assert(kr == 0);
}

/*
 *	Routine:	ipc_hash_local_delete
 *	Purpose:
 *		Deletes an entry from the space's reverse hash table.
 *	Conditions:
 *		The space must be write-locked.
 */

void
ipc_hash_local_delete(
	ipc_space_t		space,
	ipc_object_t		obj,
	mach_port_index_t	index,
	ipc_entry_t		entry)
{
	ipc_entry_t removed;
	assert(index != MACH_PORT_NULL);
	assert(space != IS_NULL);
	assert(obj != IO_NULL);

	IPC_LOCAL_HASH_INVARIANT(space, obj, index, entry);
	removed = ipc_reverse_remove(space, obj);
	assert(removed == entry);
}

/*
 *	Routine:	ipc_hash_init
 *	Purpose:
 *		Initialize the reverse hash table implementation.
 */

void
ipc_hash_init(void)
{
	ipc_hash_index_t i;

	/* initialize ipc_hash_global_size */

	ipc_hash_global_size = IPC_HASH_GLOBAL_SIZE;

	/* make sure it is a power of two */

	ipc_hash_global_mask = ipc_hash_global_size - 1;
	if ((ipc_hash_global_size & ipc_hash_global_mask) != 0) {
		natural_t bit;

		/* round up to closest power of two */

		for (bit = 1;; bit <<= 1) {
			ipc_hash_global_mask |= bit;
			ipc_hash_global_size = ipc_hash_global_mask + 1;

			if ((ipc_hash_global_size & ipc_hash_global_mask) == 0)
				break;
		}
	}

	/* allocate ipc_hash_global_table */

	ipc_hash_global_table = (ipc_hash_global_bucket_t)
		kalloc((vm_size_t) (ipc_hash_global_size *
				    sizeof(struct ipc_hash_global_bucket)));
	assert(ipc_hash_global_table != IHGB_NULL);

	/* and initialize it */

	for (i = 0; i < ipc_hash_global_size; i++) {
		ipc_hash_global_bucket_t bucket;

		bucket = &ipc_hash_global_table[i];
		ihgb_lock_init(bucket);
		bucket->ihgb_head = ITE_NULL;
	}
}

#if	MACH_IPC_DEBUG

/*
 *	Routine:	ipc_hash_info
 *	Purpose:
 *		Return information about the global reverse hash table.
 *		Fills the buffer with as much information as possible
 *		and returns the desired size of the buffer.
 *	Conditions:
 *		Nothing locked.  The caller should provide
 *		possibly-pageable memory.
 */


ipc_hash_index_t
ipc_hash_info(
	hash_info_bucket_t	*info,
	mach_msg_type_number_t count)
{
	ipc_hash_index_t i;

	if (ipc_hash_global_size < count)
		count = ipc_hash_global_size;

	for (i = 0; i < count; i++) {
		ipc_hash_global_bucket_t bucket = &ipc_hash_global_table[i];
		unsigned int bucket_count = 0;
		ipc_tree_entry_t entry;

		ihgb_lock(bucket);
		for (entry = bucket->ihgb_head;
		     entry != ITE_NULL;
		     entry = entry->ite_next)
			bucket_count++;
		ihgb_unlock(bucket);

		/* don't touch pageable memory while holding locks */
		info[i].hib_count = bucket_count;
	}

	return ipc_hash_global_size;
}

#endif	/* MACH_IPC_DEBUG */
