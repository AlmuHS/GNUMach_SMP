/*
 *  Copyright (C) 2006 Samuel Thibault <samuel.thibault@ens-lyon.org>
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef XEN_STORE_H
#define XEN_STORE_H
#include <machine/xen.h>
#include <xen/public/io/xenbus.h>

typedef unsigned32_t hyp_store_transaction_t;

#define hyp_store_state_unknown		"0"
#define hyp_store_state_initializing	"1"
#define hyp_store_state_init_wait	"2"
#define hyp_store_state_initialized	"3"
#define hyp_store_state_connected	"4"
#define hyp_store_state_closing		"5"
#define hyp_store_state_closed		"6"

void hyp_store_init(void);

extern const char *hyp_store_error;

/* Start a transaction.  */
hyp_store_transaction_t hyp_store_transaction_start(void);
/* Stop a transaction. Returns 1 if the transactions succeeded, 0 else.  */
int hyp_store_transaction_stop(hyp_store_transaction_t t);

/* List a directory: returns an array to file names, terminated by NULL.  Free
 * with kfree.  */
char **hyp_store_ls(hyp_store_transaction_t t, int n, ...);

/* Get the value of an entry.  Free with kfree.  */
void *hyp_store_read(hyp_store_transaction_t t, int n, ...);
/* Get the integer value of an entry, -1 on error.  */
int hyp_store_read_int(hyp_store_transaction_t t, int n, ...);
/* Set the value of an entry.  */
char *hyp_store_write(hyp_store_transaction_t t, const char *data, int n, ...);

#endif /* XEN_STORE_H */
