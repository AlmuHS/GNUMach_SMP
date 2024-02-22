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
/*
 *	File:	vm/vm_map.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Virtual memory map module definitions.
 *
 * Contributors:
 *	avie, dlb, mwyoung
 */

#ifndef	_VM_VM_MAP_H_
#define _VM_VM_MAP_H_

#include <mach/kern_return.h>
#include <mach/boolean.h>
#include <mach/machine/vm_types.h>
#include <mach/vm_attributes.h>
#include <mach/vm_prot.h>
#include <mach/vm_inherit.h>
#include <mach/vm_wire.h>
#include <mach/vm_sync.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_types.h>
#include <kern/list.h>
#include <kern/lock.h>
#include <kern/rbtree.h>
#include <kern/macros.h>

/* TODO: make it dynamic */
#define KENTRY_DATA_SIZE (256*PAGE_SIZE)

/*
 *	Types defined:
 *
 *	vm_map_entry_t		an entry in an address map.
 *	vm_map_version_t	a timestamp of a map, for use with vm_map_lookup
 *	vm_map_copy_t		represents memory copied from an address map,
 *				 used for inter-map copy operations
 */

/*
 *	Type:		vm_map_object_t [internal use only]
 *
 *	Description:
 *		The target of an address mapping, either a virtual
 *		memory object or a sub map (of the kernel map).
 */
typedef union vm_map_object {
	struct vm_object	*vm_object;	/* object object */
	struct vm_map		*sub_map;	/* belongs to another map */
} vm_map_object_t;

/*
 *	Type:		vm_map_entry_t [internal use only]
 *
 *	Description:
 *		A single mapping within an address map.
 *
 *	Implementation:
 *		Address map entries consist of start and end addresses,
 *		a VM object (or sub map) and offset into that object,
 *		and user-exported inheritance and protection information.
 *		Control information for virtual copy operations is also
 *		stored in the address map entry.
 */
struct vm_map_links {
	struct vm_map_entry	*prev;		/* previous entry */
	struct vm_map_entry	*next;		/* next entry */
	vm_offset_t		start;		/* start address */
	vm_offset_t		end;		/* end address */
};

struct vm_map_entry {
	struct vm_map_links	links;		/* links to other entries */
#define vme_prev		links.prev
#define vme_next		links.next
#define vme_start		links.start
#define vme_end			links.end
	struct rbtree_node	tree_node;	/* links to other entries in tree */
	struct rbtree_node	gap_node;	/* links to other entries in gap tree */
	struct list		gap_list;	/* links to other entries with
						   the same gap size */
	vm_size_t		gap_size;	/* size of available memory
						   following this entry */
	union vm_map_object	object;		/* object I point to */
	vm_offset_t		offset;		/* offset into object */
	unsigned int
	/* boolean_t */		in_gap_tree:1,	/* entry is in the gap tree if true,
						   or linked to other entries with
						   the same gap size if false */
	/* boolean_t */		is_shared:1,	/* region is shared */
	/* boolean_t */		is_sub_map:1,	/* Is "object" a submap? */
	/* boolean_t */		in_transition:1, /* Entry being changed */
	/* boolean_t */		needs_wakeup:1,  /* Waiters on in_transition */
		/* Only used when object is a vm_object: */
	/* boolean_t */		needs_copy:1;    /* does object need to be copied */

		/* Only in task maps: */
	vm_prot_t		protection;	/* protection code */
	vm_prot_t		max_protection;	/* maximum protection */
	vm_inherit_t		inheritance;	/* inheritance */
	unsigned short		wired_count;	/* can be paged if = 0 */
	vm_prot_t		wired_access;	/* wiring access types, as accepted
						   by vm_map_pageable; used on wiring
						   scans when protection != VM_PROT_NONE */
	struct vm_map_entry     *projected_on;  /* 0 for normal map entry
           or persistent kernel map projected buffer entry;
           -1 for non-persistent kernel map projected buffer entry;
           pointer to corresponding kernel map entry for user map
           projected buffer entry */
};

typedef struct vm_map_entry	*vm_map_entry_t;

#define VM_MAP_ENTRY_NULL	((vm_map_entry_t) 0)

/*
 *	Type:		struct vm_map_header
 *
 *	Description:
 *		Header for a vm_map and a vm_map_copy.
 */
struct vm_map_header {
	struct vm_map_links	links;		/* first, last, min, max */
	struct rbtree		tree;		/* Sorted tree of entries */
	struct rbtree		gap_tree;	/* Sorted tree of gap lists
						   for allocations */
	int			nentries;	/* Number of entries */
};

/*
 *	Type:		vm_map_t [exported; contents invisible]
 *
 *	Description:
 *		An address map -- a directory relating valid
 *		regions of a task's address space to the corresponding
 *		virtual memory objects.
 *
 *	Implementation:
 *		Maps are doubly-linked lists of map entries, sorted
 *		by address.  They're also contained in a red-black tree.
 *		One hint is used to start searches again at the last
 *		successful search, insertion, or removal.  If the hint
 *		lookup failed (i.e. the hint didn't refer to the requested
 *		entry), a BST lookup is performed.  Another hint is used to
 *		quickly find free space.
 */
struct vm_map {
	lock_data_t		lock;		/* Lock for map data */
	struct vm_map_header	hdr;		/* Map entry header */
#define min_offset		hdr.links.start	/* start of range */
#define max_offset		hdr.links.end	/* end of range */
	pmap_t			pmap;		/* Physical map */
	vm_size_t		size;		/* virtual size */
	vm_size_t		size_wired;	/* wired size */
	int			ref_count;	/* Reference count */
	decl_simple_lock_data(,	ref_lock)	/* Lock for ref_count field */
	vm_map_entry_t		hint;		/* hint for quick lookups */
	decl_simple_lock_data(,	hint_lock)	/* lock for hint storage */
	vm_map_entry_t		first_free;	/* First free space hint */

	/* Flags */
	unsigned int	wait_for_space:1,	/* Should callers wait
						   for space? */
	/* boolean_t */ wiring_required:1;	/* New mappings are wired? */

	unsigned int		timestamp;	/* Version number */

	const char		*name;		/* Associated name */
};

#define vm_map_to_entry(map)	((struct vm_map_entry *) &(map)->hdr.links)
#define vm_map_first_entry(map)	((map)->hdr.links.next)
#define vm_map_last_entry(map)	((map)->hdr.links.prev)

/*
 *	Type:		vm_map_version_t [exported; contents invisible]
 *
 *	Description:
 *		Map versions may be used to quickly validate a previous
 *		lookup operation.
 *
 *	Usage note:
 *		Because they are bulky objects, map versions are usually
 *		passed by reference.
 *
 *	Implementation:
 *		Just a timestamp for the main map.
 */
typedef struct vm_map_version {
	unsigned int	main_timestamp;
} vm_map_version_t;

/*
 *	Type:		vm_map_copy_t [exported; contents invisible]
 *
 *	Description:
 *		A map copy object represents a region of virtual memory
 *		that has been copied from an address map but is still
 *		in transit.
 *
 *		A map copy object may only be used by a single thread
 *		at a time.
 *
 *	Implementation:
 * 		There are three formats for map copy objects.
 *		The first is very similar to the main
 *		address map in structure, and as a result, some
 *		of the internal maintenance functions/macros can
 *		be used with either address maps or map copy objects.
 *
 *		The map copy object contains a header links
 *		entry onto which the other entries that represent
 *		the region are chained.
 *
 *		The second format is a single vm object.  This is used
 *		primarily in the pageout path.  The third format is a
 *		list of vm pages.  An optional continuation provides
 *		a hook to be called to obtain more of the memory,
 *		or perform other operations.  The continuation takes 3
 *		arguments, a saved arg buffer, a pointer to a new vm_map_copy
 *		(returned) and an abort flag (abort if TRUE).
 */

#define VM_MAP_COPY_PAGE_LIST_MAX	64

struct vm_map_copy;
struct vm_map_copyin_args_data;
typedef kern_return_t (*vm_map_copy_cont_fn)(struct vm_map_copyin_args_data*, struct vm_map_copy**);

typedef struct vm_map_copy {
	int			type;
#define VM_MAP_COPY_ENTRY_LIST	1
#define VM_MAP_COPY_OBJECT	2
#define VM_MAP_COPY_PAGE_LIST	3
	vm_offset_t		offset;
	vm_size_t		size;
	union {
	    struct vm_map_header	hdr;	/* ENTRY_LIST */
	    struct {				/* OBJECT */
	    	vm_object_t		object;
	    } c_o;
	    struct {				/* PAGE_LIST */
		vm_page_t		page_list[VM_MAP_COPY_PAGE_LIST_MAX];
		int			npages;
		vm_map_copy_cont_fn cont;
		struct vm_map_copyin_args_data* cont_args;
	    } c_p;
	} c_u;
} *vm_map_copy_t;

#define cpy_hdr			c_u.hdr

#define cpy_object		c_u.c_o.object

#define cpy_page_list		c_u.c_p.page_list
#define cpy_npages		c_u.c_p.npages
#define cpy_cont		c_u.c_p.cont
#define cpy_cont_args		c_u.c_p.cont_args

#define	VM_MAP_COPY_NULL	((vm_map_copy_t) 0)

/*
 *	Useful macros for entry list copy objects
 */

#define vm_map_copy_to_entry(copy)		\
		((struct vm_map_entry *) &(copy)->cpy_hdr.links)
#define vm_map_copy_first_entry(copy)		\
		((copy)->cpy_hdr.links.next)
#define vm_map_copy_last_entry(copy)		\
		((copy)->cpy_hdr.links.prev)

/*
 *	Continuation macros for page list copy objects
 */

#define	vm_map_copy_invoke_cont(old_copy, new_copy, result)		\
MACRO_BEGIN								\
	vm_map_copy_page_discard(old_copy);				\
	*result = (*((old_copy)->cpy_cont))((old_copy)->cpy_cont_args,	\
					    new_copy);			\
	(old_copy)->cpy_cont = (kern_return_t (*)()) 0;			\
MACRO_END

#define	vm_map_copy_invoke_extend_cont(old_copy, new_copy, result)	\
MACRO_BEGIN								\
	*result = (*((old_copy)->cpy_cont))((old_copy)->cpy_cont_args,	\
					    new_copy);			\
	(old_copy)->cpy_cont = (kern_return_t (*)()) 0;			\
MACRO_END

#define vm_map_copy_abort_cont(old_copy)				\
MACRO_BEGIN								\
	vm_map_copy_page_discard(old_copy);				\
	(*((old_copy)->cpy_cont))((old_copy)->cpy_cont_args,		\
				  (vm_map_copy_t *) 0);			\
	(old_copy)->cpy_cont = (kern_return_t (*)()) 0;			\
	(old_copy)->cpy_cont_args = VM_MAP_COPYIN_ARGS_NULL;		\
MACRO_END

#define vm_map_copy_has_cont(copy)					\
    (((copy)->cpy_cont) != (kern_return_t (*)()) 0)

/*
 *	Continuation structures for vm_map_copyin_page_list.
 */

typedef	struct vm_map_copyin_args_data {
	vm_map_t	map;
	vm_offset_t	src_addr;
	vm_size_t	src_len;
	vm_offset_t	destroy_addr;
	vm_size_t	destroy_len;
	boolean_t	steal_pages;
} vm_map_copyin_args_data_t, *vm_map_copyin_args_t;

#define	VM_MAP_COPYIN_ARGS_NULL	((vm_map_copyin_args_t) 0)

/*
 *	Macros:		vm_map_lock, etc. [internal use only]
 *	Description:
 *		Perform locking on the data portion of a map.
 */

#define vm_map_lock_init(map)			\
MACRO_BEGIN					\
	lock_init(&(map)->lock, TRUE);		\
	(map)->timestamp = 0;			\
MACRO_END

void vm_map_lock(struct vm_map *map);
void vm_map_unlock(struct vm_map *map);

#define vm_map_lock_read(map)	lock_read(&(map)->lock)
#define vm_map_unlock_read(map)	lock_read_done(&(map)->lock)
#define vm_map_lock_write_to_read(map) \
		lock_write_to_read(&(map)->lock)
#define vm_map_lock_read_to_write(map) \
		(lock_read_to_write(&(map)->lock) || (((map)->timestamp++), 0))
#define vm_map_lock_set_recursive(map) \
		lock_set_recursive(&(map)->lock)
#define vm_map_lock_clear_recursive(map) \
		lock_clear_recursive(&(map)->lock)

/*
 *	Exported procedures that operate on vm_map_t.
 */

/* Initialize the module */
extern void		vm_map_init(void);

/* Initialize an empty map */
extern void		vm_map_setup(vm_map_t, pmap_t, vm_offset_t, vm_offset_t);
/* Create an empty map */
extern vm_map_t		vm_map_create(pmap_t, vm_offset_t, vm_offset_t);
/* Create a map in the image of an existing map */
extern vm_map_t		vm_map_fork(vm_map_t);

/* Gain a reference to an existing map */
extern void		vm_map_reference(vm_map_t);
/* Lose a reference */
extern void		vm_map_deallocate(vm_map_t);

/* Enter a mapping */
extern kern_return_t	vm_map_enter(vm_map_t, vm_offset_t *, vm_size_t,
				     vm_offset_t, boolean_t, vm_object_t,
				     vm_offset_t, boolean_t, vm_prot_t,
				     vm_prot_t, vm_inherit_t);
/* Enter a mapping primitive */
extern kern_return_t	vm_map_find_entry(vm_map_t, vm_offset_t *, vm_size_t,
					  vm_offset_t, vm_object_t,
					  vm_map_entry_t *);
/* Deallocate a region */
extern kern_return_t	vm_map_remove(vm_map_t, vm_offset_t, vm_offset_t);
/* Change protection */
extern kern_return_t	vm_map_protect(vm_map_t, vm_offset_t, vm_offset_t,
				       vm_prot_t, boolean_t);
/* Change inheritance */
extern kern_return_t	vm_map_inherit(vm_map_t, vm_offset_t, vm_offset_t,
				       vm_inherit_t);

/* Look up an address */
extern kern_return_t	vm_map_lookup(vm_map_t *, vm_offset_t, vm_prot_t, boolean_t,
				      vm_map_version_t *, vm_object_t *,
				      vm_offset_t *, vm_prot_t *, boolean_t *);
/* Find a map entry */
extern boolean_t	vm_map_lookup_entry(vm_map_t, vm_offset_t,
					    vm_map_entry_t *);
/* Verify that a previous lookup is still valid */
extern boolean_t	vm_map_verify(vm_map_t, vm_map_version_t *);
/* vm_map_verify_done is now a macro -- see below */
/* Make a copy of a region */
extern kern_return_t	vm_map_copyin(vm_map_t, vm_offset_t, vm_size_t,
				      boolean_t, vm_map_copy_t *);
/* Make a copy of a region using a page list copy */
extern kern_return_t	vm_map_copyin_page_list(vm_map_t, vm_offset_t,
						vm_size_t, boolean_t,
						boolean_t, vm_map_copy_t *,
						boolean_t);
/* Place a copy into a map */
extern kern_return_t	vm_map_copyout(vm_map_t, vm_offset_t *, vm_map_copy_t);
/* Overwrite existing memory with a copy */
extern kern_return_t	vm_map_copy_overwrite(vm_map_t, vm_offset_t,
					      vm_map_copy_t, boolean_t);
/* Discard a copy without using it */
extern void		vm_map_copy_discard(vm_map_copy_t);
extern void		vm_map_copy_page_discard(vm_map_copy_t);
extern vm_map_copy_t	vm_map_copy_copy(vm_map_copy_t);
/* Page list continuation version of previous */
extern kern_return_t	vm_map_copy_discard_cont(vm_map_copyin_args_t,
						 vm_map_copy_t *);

extern boolean_t	vm_map_coalesce_entry(vm_map_t, vm_map_entry_t);

/* Add or remove machine- dependent attributes from map regions */
extern kern_return_t	vm_map_machine_attribute(vm_map_t, vm_offset_t,
						 vm_size_t,
						 vm_machine_attribute_t,
						 vm_machine_attribute_val_t *);

extern kern_return_t	vm_map_msync(vm_map_t,
				     vm_offset_t, vm_size_t, vm_sync_t);

/* Delete entry from map */
extern void		vm_map_entry_delete(vm_map_t, vm_map_entry_t);

kern_return_t vm_map_delete(
    vm_map_t   	map,
    vm_offset_t    	start,
    vm_offset_t    	end);

kern_return_t vm_map_copyout_page_list(
    vm_map_t    	dst_map,
    vm_offset_t 	*dst_addr,  /* OUT */
    vm_map_copy_t   	copy);

void vm_map_copy_page_discard (vm_map_copy_t copy);

boolean_t vm_map_lookup_entry(
	vm_map_t	map,
	vm_offset_t	address,
	vm_map_entry_t	*entry); /* OUT */

static inline void vm_map_set_name(vm_map_t map, const char *name)
{
	map->name = name;
}


/*
 *	Functions implemented as macros
 */
#define		vm_map_min(map)		((map)->min_offset)
						/* Lowest valid address in
						 * a map */

#define		vm_map_max(map)		((map)->max_offset)
						/* Highest valid address */

#define		vm_map_pmap(map)	((map)->pmap)
						/* Physical map associated
						 * with this address map */

#define		vm_map_verify_done(map, version)    (vm_map_unlock_read(map))
						/* Operation that required
						 * a verified lookup is
						 * now complete */
/*
 *	Pageability functions.
 */
extern kern_return_t	vm_map_pageable(vm_map_t, vm_offset_t, vm_offset_t,
					vm_prot_t, boolean_t, boolean_t);

extern kern_return_t	vm_map_pageable_all(vm_map_t, vm_wire_t);

/*
 *	Submap object.  Must be used to create memory to be put
 *	in a submap by vm_map_submap.
 */
extern vm_object_t	vm_submap_object;

/*
 *  vm_map_copyin_object:
 *
 *  Create a copy object from an object.
 *  Our caller donates an object reference.
 */
extern kern_return_t vm_map_copyin_object(
    vm_object_t object,
    vm_offset_t offset,     /* offset of region in object */
    vm_size_t   size,       /* size of region in object */
    vm_map_copy_t   *copy_result);   /* OUT */

/*
 *  vm_map_submap:      [ kernel use only ]
 *
 *  Mark the given range as handled by a subordinate map.
 *
 *  This range must have been created with vm_map_find using
 *  the vm_submap_object, and no other operations may have been
 *  performed on this range prior to calling vm_map_submap.
 *
 *  Only a limited number of operations can be performed
 *  within this rage after calling vm_map_submap:
 *      vm_fault
 *  [Don't try vm_map_copyin!]
 *
 *  To remove a submapping, one must first remove the
 *  range from the superior map, and then destroy the
 *  submap (if desired).  [Better yet, don't try it.]
 */
extern kern_return_t vm_map_submap(
    vm_map_t   map,
    vm_offset_t    start,
    vm_offset_t    end,
    vm_map_t        submap);

/*
 *	Wait and wakeup macros for in_transition map entries.
 */
#define vm_map_entry_wait(map, interruptible)    	\
        MACRO_BEGIN                                     \
        assert_wait((event_t)&(map)->hdr, interruptible);	\
        vm_map_unlock(map);                             \
	thread_block((void (*)()) 0);			\
        MACRO_END

#define vm_map_entry_wakeup(map)        thread_wakeup((event_t)&(map)->hdr)

/*
 *      This routine is called only when it is known that
 *      the entry must be split.
 */
extern void _vm_map_clip_start(
        struct vm_map_header *map_header,
        vm_map_entry_t entry,
        vm_offset_t	start,
        boolean_t	link_gap);

/*
 *      vm_map_clip_end:        [ internal use only ]
 *
 *      Asserts that the given entry ends at or before
 *      the specified address; if necessary,
 *      it splits the entry into two.
 */
void _vm_map_clip_end(
	struct vm_map_header 	*map_header,
	vm_map_entry_t		entry,
	vm_offset_t		end,
	boolean_t		link_gap);

#endif	/* _VM_VM_MAP_H_ */
