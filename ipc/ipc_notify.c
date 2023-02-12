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
 *	File:	ipc/ipc_notify.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Notification-sending functions.
 */

#include <kern/printf.h>
#include <mach/port.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <kern/assert.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_mqueue.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_port.h>

#include <ipc/ipc_machdep.h>

mach_port_deleted_notification_t	ipc_notify_port_deleted_template;
mach_msg_accepted_notification_t	ipc_notify_msg_accepted_template;
mach_port_destroyed_notification_t	ipc_notify_port_destroyed_template;
mach_no_senders_notification_t		ipc_notify_no_senders_template;
mach_send_once_notification_t		ipc_notify_send_once_template;
mach_dead_name_notification_t		ipc_notify_dead_name_template;

#define NOTIFY_MSGH_SEQNO	0

/*
 *	Routine:	ipc_notify_init_port_deleted
 *	Purpose:
 *		Initialize a template for port-deleted notifications.
 */

static void
ipc_notify_init_port_deleted(mach_port_deleted_notification_t *n)
{
	mach_msg_header_t *m = &n->not_header;
	mach_msg_type_t *t = &n->not_type;

	m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_size = sizeof *n;
	m->msgh_seqno = NOTIFY_MSGH_SEQNO;
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_PORT_DELETED;

	t->msgt_name = MACH_MSG_TYPE_PORT_NAME;
	t->msgt_size = PORT_NAME_T_SIZE_IN_BITS;
	t->msgt_number = 1;
	t->msgt_inline = TRUE;
	t->msgt_longform = FALSE;
	t->msgt_deallocate = FALSE;
	t->msgt_unused = 0;

	n->not_port = MACH_PORT_NULL;
}

/*
 *	Routine:	ipc_notify_init_msg_accepted
 *	Purpose:
 *		Initialize a template for msg-accepted notifications.
 */

static void
ipc_notify_init_msg_accepted(mach_msg_accepted_notification_t *n)
{
	mach_msg_header_t *m = &n->not_header;
	mach_msg_type_t *t = &n->not_type;

	m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_size = sizeof *n;
	m->msgh_seqno = NOTIFY_MSGH_SEQNO;
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_MSG_ACCEPTED;

	t->msgt_name = MACH_MSG_TYPE_PORT_NAME;
	t->msgt_size = PORT_NAME_T_SIZE_IN_BITS;
	t->msgt_number = 1;
	t->msgt_inline = TRUE;
	t->msgt_longform = FALSE;
	t->msgt_deallocate = FALSE;
	t->msgt_unused = 0;

	n->not_port = MACH_PORT_NULL;
}

/*
 *	Routine:	ipc_notify_init_port_destroyed
 *	Purpose:
 *		Initialize a template for port-destroyed notifications.
 */

static void
ipc_notify_init_port_destroyed(mach_port_destroyed_notification_t *n)
{
	mach_msg_header_t *m = &n->not_header;
	mach_msg_type_t *t = &n->not_type;

	m->msgh_bits = MACH_MSGH_BITS_COMPLEX |
		MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_size = sizeof *n;
	m->msgh_seqno = NOTIFY_MSGH_SEQNO;
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_PORT_DESTROYED;

	t->msgt_name = MACH_MSG_TYPE_PORT_RECEIVE;
	t->msgt_size = PORT_T_SIZE_IN_BITS;
	t->msgt_number = 1;
	t->msgt_inline = TRUE;
	t->msgt_longform = FALSE;
	t->msgt_deallocate = FALSE;
	t->msgt_unused = 0;

	n->not_port = MACH_PORT_NULL;
}

/*
 *	Routine:	ipc_notify_init_no_senders
 *	Purpose:
 *		Initialize a template for no-senders notifications.
 */

static void
ipc_notify_init_no_senders(
	mach_no_senders_notification_t	*n)
{
	mach_msg_header_t *m = &n->not_header;
	mach_msg_type_t *t = &n->not_type;

	m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_size = sizeof *n;
	m->msgh_seqno = NOTIFY_MSGH_SEQNO;
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_NO_SENDERS;

	t->msgt_name = MACH_MSG_TYPE_INTEGER_32;
	t->msgt_size = 32;
	t->msgt_number = 1;
	t->msgt_inline = TRUE;
	t->msgt_longform = FALSE;
	t->msgt_deallocate = FALSE;
	t->msgt_unused = 0;

	n->not_count = 0;
}

/*
 *	Routine:	ipc_notify_init_send_once
 *	Purpose:
 *		Initialize a template for send-once notifications.
 */

static void
ipc_notify_init_send_once(
	mach_send_once_notification_t	*n)
{
	mach_msg_header_t *m = &n->not_header;

	m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_size = sizeof *n;
	m->msgh_seqno = NOTIFY_MSGH_SEQNO;
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_SEND_ONCE;
}

/*
 *	Routine:	ipc_notify_init_dead_name
 *	Purpose:
 *		Initialize a template for dead-name notifications.
 */

static void
ipc_notify_init_dead_name(
	mach_dead_name_notification_t	*n)
{
	mach_msg_header_t *m = &n->not_header;
	mach_msg_type_t *t = &n->not_type;

	m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_size = sizeof *n;
	m->msgh_seqno = NOTIFY_MSGH_SEQNO;
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_DEAD_NAME;

	t->msgt_name = MACH_MSG_TYPE_PORT_NAME;
	t->msgt_size = PORT_NAME_T_SIZE_IN_BITS;
	t->msgt_number = 1;
	t->msgt_inline = TRUE;
	t->msgt_longform = FALSE;
	t->msgt_deallocate = FALSE;
	t->msgt_unused = 0;

	n->not_port = MACH_PORT_NULL;
}

/*
 *	Routine:	ipc_notify_init
 *	Purpose:
 *		Initialize the notification subsystem.
 */

void
ipc_notify_init(void)
{
	ipc_notify_init_port_deleted(&ipc_notify_port_deleted_template);
	ipc_notify_init_msg_accepted(&ipc_notify_msg_accepted_template);
	ipc_notify_init_port_destroyed(&ipc_notify_port_destroyed_template);
	ipc_notify_init_no_senders(&ipc_notify_no_senders_template);
	ipc_notify_init_send_once(&ipc_notify_send_once_template);
	ipc_notify_init_dead_name(&ipc_notify_dead_name_template);
}

/*
 *	Routine:	ipc_notify_port_deleted
 *	Purpose:
 *		Send a port-deleted notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 */

void
ipc_notify_port_deleted(
	ipc_port_t 		port,
	mach_port_name_t 	name)
{
	ipc_kmsg_t kmsg;
	mach_port_deleted_notification_t *n;

	kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped port-deleted (0x%p, 0x%x)\n", port, name);
		ipc_port_release_sonce(port);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_port_deleted_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_port_deleted_template;

	n->not_header.msgh_remote_port = (mach_port_t) port;
	n->not_port = name;

	ipc_mqueue_send_always(kmsg);
}

/*
 *	Routine:	ipc_notify_msg_accepted
 *	Purpose:
 *		Send a msg-accepted notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 */

void
ipc_notify_msg_accepted(
	ipc_port_t 		port,
	mach_port_name_t 	name)
{
	ipc_kmsg_t kmsg;
	mach_msg_accepted_notification_t *n;

	kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped msg-accepted (0x%p, 0x%x)\n", port, name);
		ipc_port_release_sonce(port);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_msg_accepted_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_msg_accepted_template;

	n->not_header.msgh_remote_port = (mach_port_t) port;
	n->not_port = name;

	ipc_mqueue_send_always(kmsg);
}

/*
 *	Routine:	ipc_notify_port_destroyed
 *	Purpose:
 *		Send a port-destroyed notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 *		Consumes a ref for right, which should be a receive right
 *		prepped for placement into a message.  (In-transit,
 *		or in-limbo if a circularity was detected.)
 */

void
ipc_notify_port_destroyed(
	ipc_port_t 	port,
	ipc_port_t 	right)
{
	ipc_kmsg_t kmsg;
	mach_port_destroyed_notification_t *n;

	kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped port-destroyed (0x%p, 0x%p)\n",
		       port, right);
		ipc_port_release_sonce(port);
		ipc_port_release_receive(right);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_port_destroyed_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_port_destroyed_template;

	n->not_header.msgh_remote_port = (mach_port_t) port;
	n->not_port = (mach_port_t) right;

	ipc_mqueue_send_always(kmsg);
}

/*
 *	Routine:	ipc_notify_no_senders
 *	Purpose:
 *		Send a no-senders notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 */

void
ipc_notify_no_senders(
	ipc_port_t 		port,
	mach_port_mscount_t 	mscount)
{
	ipc_kmsg_t kmsg;
	mach_no_senders_notification_t *n;

	kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped no-senders (0x%p, %u)\n", port, mscount);
		ipc_port_release_sonce(port);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_no_senders_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_no_senders_template;

	n->not_header.msgh_remote_port = (mach_port_t) port;
	n->not_count = mscount;

	ipc_mqueue_send_always(kmsg);
}

/*
 *	Routine:	ipc_notify_send_once
 *	Purpose:
 *		Send a send-once notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 */

void
ipc_notify_send_once(ipc_port_t port)
{
	ipc_kmsg_t kmsg;
	mach_send_once_notification_t *n;

	kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped send-once (0x%p)\n", port);
		ipc_port_release_sonce(port);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_send_once_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_send_once_template;

	n->not_header.msgh_remote_port = (mach_port_t) port;

	ipc_mqueue_send_always(kmsg);
}

/*
 *	Routine:	ipc_notify_dead_name
 *	Purpose:
 *		Send a dead-name notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 */

void
ipc_notify_dead_name(
	ipc_port_t 		port,
	mach_port_name_t 	name)
{
	ipc_kmsg_t kmsg;
	mach_dead_name_notification_t *n;

	kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped dead-name (0x%p, 0x%x)\n", port, name);
		ipc_port_release_sonce(port);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_dead_name_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_dead_name_template;

	n->not_header.msgh_remote_port = (mach_port_t) port;
	n->not_port = name;

	ipc_mqueue_send_always(kmsg);
}
