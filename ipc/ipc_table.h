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
 */
/*
 *	File:	ipc/ipc_table.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Definitions for tables, used for dead-name requests
 *	(ipc_port_request_t).
 */

#ifndef	_IPC_IPC_TABLE_H_
#define	_IPC_IPC_TABLE_H_

#include <mach/boolean.h>
#include <mach/vm_param.h>

/*
 *	Every its_size value must must be a power of two.
 *
 *	The ipr_size field of the first element in a table of
 *	dead-name requests (ipc_port_request_t) points to the
 *	ipc_table_size structure.  The structures must be elements
 *	of ipc_table_dnrequests.  ipc_table_dnrequests must end
 *	with an element with zero its_size, and except for this last
 *	element, the its_size values must be strictly increasing.
 *
 *	The ipr_size field points to the currently used ipc_table_size.
 */

typedef unsigned int ipc_table_index_t;	/* index into tables */
typedef unsigned int ipc_table_elems_t;	/* size of tables */

typedef struct ipc_table_size {
	ipc_table_elems_t its_size;	/* number of elements in table */
} *ipc_table_size_t;

#define	ITS_NULL	((ipc_table_size_t) 0)

extern ipc_table_size_t ipc_table_dnrequests;

extern void
ipc_table_init(void);

/*
 *	Note that ipc_table_alloc, and ipc_table_free all potentially
 *	use the VM system.  Hence simple locks can't be held across
 *	them.
 */

/* Allocate a table */
extern vm_offset_t ipc_table_alloc(
	vm_size_t	size);

/* Free a table */
extern void ipc_table_free(
	vm_size_t	size,
	vm_offset_t	table);

void ipc_table_fill(
	ipc_table_size_t	its,
	unsigned int		num,
	unsigned int		min,
	vm_size_t		elemsize);

#define	it_dnrequests_alloc(its)					\
	((ipc_port_request_t)						\
	 ipc_table_alloc((its)->its_size *				\
			 sizeof(struct ipc_port_request)))

#define	it_dnrequests_free(its, table)					\
	ipc_table_free((its)->its_size *				\
		       sizeof(struct ipc_port_request),			\
		       (vm_offset_t)(table))

#endif	/* _IPC_IPC_TABLE_H_ */
