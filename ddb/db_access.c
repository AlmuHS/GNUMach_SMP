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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#if MACH_KDB

#include <mach/boolean.h>
#include <machine/db_machdep.h>		/* type definitions */
#include <machine/db_interface.h>	/* function definitions */
#include <machine/setjmp.h>
#include <kern/task.h>
#include <ddb/db_access.h>



/*
 * Access unaligned data items on aligned (longword)
 * boundaries.
 */

int db_access_level = DB_ACCESS_LEVEL;

/*
 * This table is for sign-extending things.
 * Therefore its entries are signed, and yes
 * they are infact negative numbers.
 * So don't you put no more Us in it. Or Ls either.
 * Otherwise there is no point having it, n'est pas ?
 */
static int db_extend[sizeof(int)+1] = {	/* table for sign-extending */
	0,
	0xFFFFFF80,
	0xFFFF8000,
	0xFF800000,
	0x80000000
};

db_expr_t
db_get_task_value(
	db_addr_t	addr,
	int		size,
	boolean_t	is_signed,
	task_t		task)
{
	char		data[sizeof(db_expr_t)];
	db_expr_t 	value;
	int		i;

	if (!db_read_bytes(addr, size, data, task))
	    return 0;

	value = 0;
#if	BYTE_MSF
	for (i = 0; i < size; i++)
#else	/* BYTE_LSF */
	for (i = size - 1; i >= 0; i--)
#endif
	{
	    value = (value << 8) + (data[i] & 0xFF);
	}

	if (size <= sizeof(int)) {
	    if (is_signed && (value & db_extend[size]) != 0)
		value |= db_extend[size];
	}
	return (value);
}

void
db_put_task_value(
	db_addr_t	addr,
	int		size,
	db_expr_t 	value,
	task_t		task)
{
	char		data[sizeof(db_expr_t)];
	int		i;

#if	BYTE_MSF
	for (i = size - 1; i >= 0; i--)
#else	/* BYTE_LSF */
	for (i = 0; i < size; i++)
#endif
	{
	    data[i] = value & 0xFF;
	    value >>= 8;
	}

	db_write_bytes(addr, size, data, task);
}

db_expr_t
db_get_value(
	db_addr_t	addr,
	int		size,
	boolean_t	is_signed)
{
	return(db_get_task_value(addr, size, is_signed, TASK_NULL));
}

void
db_put_value(
	db_addr_t	addr,
	int		size,
	db_expr_t	value)
{
	db_put_task_value(addr, size, value, TASK_NULL);
}

#endif /* MACH_KDB */
