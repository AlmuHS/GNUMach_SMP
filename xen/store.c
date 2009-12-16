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

#include <sys/types.h>
#include <mach/mig_support.h>
#include <machine/pmap.h>
#include <machine/ipl.h>
#include <stdarg.h>
#include <string.h>
#include <alloca.h>
#include <xen/public/xen.h>
#include <xen/public/io/xs_wire.h>
#include <util/atoi.h>
#include "store.h"
#include "ring.h"
#include "evt.h"
#include "xen.h"

/* TODO use events instead of just yielding */

/* Hypervisor part */

decl_simple_lock_data(static, lock);

static struct xenstore_domain_interface *store;

struct store_req {
	const char *data;
	unsigned len;
};

/* Send a request */
static void store_put(hyp_store_transaction_t t, unsigned32_t type, struct store_req *req, unsigned nr_reqs) {
	struct xsd_sockmsg head = {
		.type = type,
		.req_id = 0,
		.tx_id = t,
	};
	unsigned totlen, len;
	unsigned i;

	totlen = 0;
	for (i = 0; i < nr_reqs; i++)
		totlen += req[i].len;
	head.len = totlen;
	totlen += sizeof(head);

	if (totlen > sizeof(store->req) - 1)
		panic("too big store message %d, max %d", totlen, sizeof(store->req));

	while (hyp_ring_available(store->req, store->req_prod, store->req_cons) < totlen)
		hyp_yield();

	mb();
	hyp_ring_store(&hyp_ring_cell(store->req, store->req_prod), &head, sizeof(head), store->req, store->req + sizeof(store->req));
	len = sizeof(head);
	for (i=0; i<nr_reqs; i++) {
		hyp_ring_store(&hyp_ring_cell(store->req, store->req_prod + len), req[i].data, req[i].len, store->req, store->req + sizeof(store->req));
		len += req[i].len;
	}

	wmb();
	store->req_prod += totlen;
	hyp_event_channel_send(boot_info.store_evtchn);
}

static const char *errors[] = {
	"EINVAL",
	"EACCES",
	"EEXIST",
	"EISDIR",
	"ENOENT",
	"ENOMEM",
	"ENOSPC",
	"EIO",
	"ENOTEMPTY",
	"ENOSYS",
	"EROFS",
	"EBUSY",
	"EAGAIN",
	"EISCONN",
	NULL,
};

/* Send a request and wait for a reply, whose header is put in head, and
 * data is returned (beware, that's in the ring !)
 * On error, returns NULL.  Else takes the lock and return pointer on data and
 * store_put_wait_end shall be called after reading it.  */
static struct xsd_sockmsg head;
const char *hyp_store_error;

static void *store_put_wait(hyp_store_transaction_t t, unsigned32_t type, struct store_req *req, unsigned nr_reqs) {
	unsigned len;
	const char **error;
	void *data;

	simple_lock(&lock);
	store_put(t, type, req, nr_reqs);
again:
	while (store->rsp_prod - store->rsp_cons < sizeof(head))
		hyp_yield();
	rmb();
	hyp_ring_fetch(&head, &hyp_ring_cell(store->rsp, store->rsp_cons), sizeof(head), store->rsp, store->rsp + sizeof(store->rsp));
	len = sizeof(head) + head.len;
	while (store->rsp_prod - store->rsp_cons < len)
		hyp_yield();
	rmb();
	if (head.type == XS_WATCH_EVENT) {
		/* Spurious watch event, drop */
		store->rsp_cons += sizeof(head) + head.len;
		hyp_event_channel_send(boot_info.store_evtchn);
		goto again;
	}
	data = &hyp_ring_cell(store->rsp, store->rsp_cons + sizeof(head));
	if (head.len <= 10) {
		char c[10];
		hyp_ring_fetch(c, data, head.len, store->rsp, store->rsp + sizeof(store->rsp));
		for (error = errors; *error; error++) {
			if (head.len == strlen(*error) + 1 && !memcmp(*error, c, head.len)) {
				hyp_store_error = *error;
				store->rsp_cons += len;
				hyp_event_channel_send(boot_info.store_evtchn);
				simple_unlock(&lock);
				return NULL;
			}
		}
	}
	return data;
}

/* Must be called after each store_put_wait.  Releases lock.  */
static void store_put_wait_end(void) {
	mb();
	store->rsp_cons += sizeof(head) + head.len;
	hyp_event_channel_send(boot_info.store_evtchn);
	simple_unlock(&lock);
}

/* Start a transaction.  */
hyp_store_transaction_t hyp_store_transaction_start(void) {
	struct store_req req = {
		.data = "",
		.len = 1,
	};
	char *rep;
	char *s;
	int i;

	rep = store_put_wait(0, XS_TRANSACTION_START, &req, 1);
	if (!rep)
		panic("couldn't start transaction (%s)", hyp_store_error);
	s = alloca(head.len);
	hyp_ring_fetch(s, rep, head.len, store->rsp, store->rsp + sizeof(store->rsp));
	mach_atoi((u_char*) s, &i);
	if (i == MACH_ATOI_DEFAULT)
		panic("bogus transaction id len %d '%s'", head.len, s);
	store_put_wait_end();
	return i;
}

/* Stop a transaction.  */
int hyp_store_transaction_stop(hyp_store_transaction_t t) {
	struct store_req req = {
		.data = "T",
		.len = 2,
	};
	int ret = 1;
	void *rep;
	rep = store_put_wait(t, XS_TRANSACTION_END, &req, 1);
	if (!rep)
		return 0;
	store_put_wait_end();
	return ret;
}

/* List a directory: returns an array to file names, terminated by NULL.  Free
 * with kfree.  */
char **hyp_store_ls(hyp_store_transaction_t t, int n, ...) {
	struct store_req req[n];
	va_list listp;
	int i;
	char *rep;
	char *c;
	char **res, **rsp;

	va_start (listp, n);
	for (i = 0; i < n; i++) {
		req[i].data = va_arg(listp, char *);
		req[i].len = strlen(req[i].data);
	}
	req[n - 1].len++;
	va_end (listp);

	rep = store_put_wait(t, XS_DIRECTORY, req, n);
	if (!rep)
		return NULL;
	i = 0;
	for (	c = rep, n = 0;
		n < head.len;
		n += hyp_ring_next_word(&c, store->rsp, store->rsp + sizeof(store->rsp)) + 1)
		i++;
	res = (void*) kalloc((i + 1) * sizeof(char*) + head.len);
	if (!res)
		hyp_store_error = "ENOMEM";
	else {
		hyp_ring_fetch(res + (i + 1), rep, head.len, store->rsp, store->rsp + sizeof(store->rsp));
		rsp = res;
		for (c = (char*) (res + (i + 1)); i; i--, c += strlen(c) + 1)
			*rsp++ = c;
		*rsp = NULL;
	}
	store_put_wait_end();
	return res;
}

/* Get the value of an entry, va version.  */
static void *hyp_store_read_va(hyp_store_transaction_t t, int n, va_list listp) {
	struct store_req req[n];
	int i;
	void *rep;
	char *res;

	for (i = 0; i < n; i++) {
		req[i].data = va_arg(listp, char *);
		req[i].len = strlen(req[i].data);
	}
	req[n - 1].len++;

	rep = store_put_wait(t, XS_READ, req, n);
	if (!rep)
		return NULL;
	res = (void*) kalloc(head.len + 1);
	if (!res)
		hyp_store_error = "ENOMEM";
	else {
		hyp_ring_fetch(res, rep, head.len, store->rsp, store->rsp + sizeof(store->rsp));
		res[head.len] = 0;
	}
	store_put_wait_end();
	return res;
}

/* Get the value of an entry.  Free with kfree.  */
void *hyp_store_read(hyp_store_transaction_t t, int n, ...) {
	va_list listp;
	char *res;

	va_start(listp, n);
	res = hyp_store_read_va(t, n, listp);
	va_end(listp);
	return res;
}

/* Get the integer value of an entry, -1 on error.  */
int hyp_store_read_int(hyp_store_transaction_t t, int n, ...) {
	va_list listp;
	char *res;
	int i;

	va_start(listp, n);
	res = hyp_store_read_va(t, n, listp);
	va_end(listp);
	if (!res)
		return -1;
	mach_atoi((u_char *) res, &i);
	if (i == MACH_ATOI_DEFAULT)
		printf("bogus integer '%s'\n", res);
	kfree((vm_offset_t) res, strlen(res)+1);
	return i;
}

/* Set the value of an entry.  */
char *hyp_store_write(hyp_store_transaction_t t, const char *data, int n, ...) {
	struct store_req req[n + 1];
	va_list listp;
	int i;
	void *rep;
	char *res;

	va_start (listp, n);
	for (i = 0; i < n; i++) {
		req[i].data = va_arg(listp, char *);
		req[i].len = strlen(req[i].data);
	}
	req[n - 1].len++;
	req[n].data = data;
	req[n].len = strlen (data);
	va_end (listp);

	rep = store_put_wait (t, XS_WRITE, req, n + 1);
	if (!rep)
		return NULL;
	res = (void*) kalloc(head.len + 1);
	if (!res)
		hyp_store_error = NULL;
	else {
		hyp_ring_fetch(res, rep, head.len, store->rsp, store->rsp + sizeof(store->rsp));
		res[head.len] = 0;
	}
	store_put_wait_end();
	return res;
}

static void hyp_store_handler(int unit)
{
	thread_wakeup(&boot_info.store_evtchn);
}

/* Map store's shared page.  */
void hyp_store_init(void)
{
	if (store)
		return;
	simple_lock_init(&lock);
	store = (void*) mfn_to_kv(boot_info.store_mfn);
	pmap_set_page_readwrite(store);
	/* SPL sched */
	hyp_evt_handler(boot_info.store_evtchn, hyp_store_handler, 0, SPL7);
}
