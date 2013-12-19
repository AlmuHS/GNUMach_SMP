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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	ll/89
 */

#ifndef	_DEVICE_NET_IO_H_
#define	_DEVICE_NET_IO_H_

/*
 * Utilities for playing with network messages.
 */

#include <mach/machine/vm_types.h>
#include <ipc/ipc_kmsg.h>

#include <kern/macro_help.h>
#include <kern/lock.h>
#include <kern/kalloc.h>

#include <device/if_hdr.h>
#include <device/io_req.h>
#include <device/net_status.h>

struct net_rcv_port;
typedef struct net_rcv_port *net_rcv_port_t;

struct net_hash_entry;
typedef struct net_hash_entry *net_hash_entry_t;

struct net_hash_header;
typedef struct net_hash_header *net_hash_header_t;

/*
 * A network packet is wrapped in a kernel message while in
 * the kernel.
 */

#define	net_kmsg(kmsg)	((net_rcv_msg_t)&(kmsg)->ikm_header)

/*
 * Interrupt routines may allocate and free net_kmsgs with these
 * functions.  net_kmsg_get may return IKM_NULL.
 */

extern ipc_kmsg_t net_kmsg_get(void);
extern void net_kmsg_put(ipc_kmsg_t);

/*
 * Network utility routines.
 */

extern void net_ast(void);
extern void net_packet(struct ifnet *, ipc_kmsg_t, unsigned int, boolean_t);
extern void net_filter(ipc_kmsg_t, ipc_kmsg_queue_t);
extern io_return_t net_getstat(struct ifnet *, dev_flavor_t, dev_status_t,
			       natural_t *);
extern io_return_t net_write(struct ifnet *, int (*)(), io_req_t);

/*
 * Non-interrupt code may allocate and free net_kmsgs with these functions.
 */

extern vm_size_t net_kmsg_size;

extern void net_kmsg_collect (void);

extern void net_io_init(void);
extern void net_thread(void) __attribute__ ((noreturn));

#define net_kmsg_alloc()	((ipc_kmsg_t) kalloc(net_kmsg_size))
#define net_kmsg_free(kmsg)	kfree((vm_offset_t) (kmsg), net_kmsg_size)

extern unsigned int		ntohl(unsigned int);
extern unsigned short int	ntohs(unsigned short int);
extern unsigned int		htonl(unsigned int);
extern unsigned short int	htons(unsigned short int);

unsigned int bpf_hash(int n, const unsigned int *keys);

extern boolean_t
net_do_filter(
	net_rcv_port_t	infp,
	const char *	data,
	unsigned int	data_count,
	const char *	header); /* CSPF */

extern int
bpf_do_filter(
	net_rcv_port_t		infp,
	char *			p,
	unsigned int		wirelen,
	char *			header,
	unsigned int    	hlen,
	net_hash_entry_t	**hash_headpp,
	net_hash_entry_t	*entpp); /* BPF */

int hash_ent_remove(
	struct ifnet 		*ifp,
	net_hash_header_t 	hp,
	int			used,
	net_hash_entry_t 	*head,
	net_hash_entry_t 	entp,
	queue_entry_t 		*dead_p);

void net_free_dead_infp(queue_entry_t dead_infp);
void net_free_dead_entp (queue_entry_t dead_entp);

int bpf_validate(
	bpf_insn_t 	f,
	int 		bytes,
	bpf_insn_t 	*match);

int bpf_eq(
	bpf_insn_t 	f1,
	bpf_insn_t 	f2,
	int 		bytes);

int net_add_q_info(ipc_port_t rcv_port);

int bpf_match (
	net_hash_header_t 	hash,
	int 			n_keys,
	const unsigned int 	*keys,
	net_hash_entry_t 	**hash_headpp,
	net_hash_entry_t 	*entpp);

#endif	/* _DEVICE_NET_IO_H_ */
