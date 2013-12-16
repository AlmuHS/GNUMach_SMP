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
/* **********************************************************************
 File:         kd_event.c
 Description:  Driver for event interface to keyboard.

 $ Header: $

 Copyright Ing. C. Olivetti & C. S.p.A. 1989.  All rights reserved.
********************************************************************** */
/*
  Copyright 1988, 1989 by Olivetti Advanced Technology Center, Inc.,
Cupertino, California.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Olivetti
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

  OLIVETTI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OLIVETTI BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <mach/boolean.h>
#include <sys/types.h>
#include <kern/printf.h>
#include <string.h>

#include <device/ds_routines.h>
#include <device/device_types.h>
#include <device/io_req.h>
#include <i386/machspl.h>
#include <i386/pio.h>
#include <i386at/kd.h>
#include <i386at/kd_queue.h>

#include "kd_event.h"

/*
 * Code for /dev/kbd.   The interrupt processing is done in kd.c,
 * which calls into this module to enqueue scancode events when
 * the keyboard is in Event mode.
 */

/*
 * Note: These globals are protected by raising the interrupt level
 * via SPLKD.
 */

kd_event_queue kbd_queue;		/* queue of keyboard events */
queue_head_t	kbd_read_queue = { &kbd_read_queue, &kbd_read_queue };

static boolean_t initialized = FALSE;


/*
 * kbdinit - set up event queue.
 */

void
kbdinit(void)
{
	spl_t s = SPLKD();

	if (!initialized) {
		kdq_reset(&kbd_queue);
		initialized = TRUE;
	}
	splx(s);
}


/*
 * kbdopen - Verify that open is read-only and remember process
 * group leader.
 */

/*ARGSUSED*/
int
kbdopen(dev, flags)
	dev_t dev;
	int flags;
{
	spl_t o_pri = spltty();
	kdinit();
	splx(o_pri);
	kbdinit();

	return(0);
}


/*
 * kbdclose - Make sure that the kd driver is in Ascii mode and
 * reset various flags.
 */

/*ARGSUSED*/
void
kbdclose(dev, flags)
	dev_t dev;
	int flags;
{
	spl_t s = SPLKD();

	kb_mode = KB_ASCII;
	kdq_reset(&kbd_queue);
	splx(s);
}


io_return_t kbdgetstat(dev, flavor, data, count)
	dev_t		dev;
	int		flavor;
	int *		data;		/* pointer to OUT array */
	unsigned int	*count;		/* OUT */
{
	switch (flavor) {
	    case KDGKBDTYPE:
		*data = KB_VANILLAKB;
		*count = 1;
		break;
	    case DEV_GET_SIZE:
		data[DEV_GET_SIZE_DEVICE_SIZE] = 0;
		data[DEV_GET_SIZE_RECORD_SIZE] = sizeof(kd_event);
		*count = DEV_GET_SIZE_COUNT;
		break;
	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

io_return_t kbdsetstat(dev, flavor, data, count)
	dev_t		dev;
	int		flavor;
	int *		data;
	unsigned int	count;
{
	switch (flavor) {
	    case KDSKBDMODE:
		kb_mode = *data;
		/* XXX - what to do about unread events? */
		/* XXX - should check that 'data' contains an OK valud */
		break;
   	   case KDSETLEDS:
	        if (count != 1)
	            return (D_INVALID_OPERATION);
		kd_setleds1 (*data);
		break;
	    case K_X_KDB_ENTER:
		return X_kdb_enter_init((unsigned int *)data, count);
	    case K_X_KDB_EXIT:
		return X_kdb_exit_init((unsigned int *)data, count);
	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}



/*
 * kbdread - dequeue and return any queued events.
 */
int
kbdread(dev, ior)
	dev_t		dev;
	io_req_t	ior;
{
	int		err, count;
	spl_t		s;

	/* Check if IO_COUNT is a multiple of the record size. */
	if (ior->io_count % sizeof(kd_event) != 0)
	    return D_INVALID_SIZE;

	err = device_read_alloc(ior, (vm_size_t)ior->io_count);
	if (err != KERN_SUCCESS)
	    return (err);

	s = SPLKD();
	if (kdq_empty(&kbd_queue)) {
	    if (ior->io_mode & D_NOWAIT) {
		splx(s);
		return (D_WOULD_BLOCK);
	    }
	    ior->io_done = kbd_read_done;
	    enqueue_tail(&kbd_read_queue, (queue_entry_t) ior);
	    splx(s);
	    return (D_IO_QUEUED);
	}
	count = 0;
	while (!kdq_empty(&kbd_queue) && count < ior->io_count) {
	    kd_event *ev;

	    ev = kdq_get(&kbd_queue);
	    *(kd_event *)(&ior->io_data[count]) = *ev;
	    count += sizeof(kd_event);
	}
	splx(s);
	ior->io_residual = ior->io_count - count;
	return (D_SUCCESS);
}

boolean_t kbd_read_done(ior)
	io_req_t	ior;
{
	int		count;
	spl_t		s;

	s = SPLKD();
	if (kdq_empty(&kbd_queue)) {
	    ior->io_done = kbd_read_done;
	    enqueue_tail(&kbd_read_queue, (queue_entry_t)ior);
	    splx(s);
	    return (FALSE);
	}

	count = 0;
	while (!kdq_empty(&kbd_queue) && count < ior->io_count) {
	    kd_event *ev;

	    ev = kdq_get(&kbd_queue);
	    *(kd_event *)(&ior->io_data[count]) = *ev;
	    count += sizeof(kd_event);
	}
	splx(s);

	ior->io_residual = ior->io_count - count;
	ds_read_done(ior);

	return (TRUE);
}



/*
 * kd_enqsc - enqueue a scancode.  Should be called at SPLKD.
 */

void
kd_enqsc(sc)
	Scancode sc;
{
	kd_event ev;

	ev.type = KEYBD_EVENT;
	ev.time = time;
	ev.value.sc = sc;
	kbd_enqueue(&ev);
}


/*
 * kbd_enqueue - enqueue an event and wake up selecting processes, if
 * any.  Should be called at SPLKD.
 */

void
kbd_enqueue(ev)
	kd_event *ev;
{
	if (kdq_full(&kbd_queue))
		printf("kbd: queue full\n");
	else
		kdq_put(&kbd_queue, ev);

	{
	    io_req_t	ior;
	    while ((ior = (io_req_t)dequeue_head(&kbd_read_queue)) != 0)
		iodone(ior);
	}
}

u_int X_kdb_enter_str[512], X_kdb_exit_str[512];
int   X_kdb_enter_len = 0,  X_kdb_exit_len = 0;

void
kdb_in_out(p)
const u_int *p;
{
	int t = p[0];

	switch (t & K_X_TYPE) {
		case K_X_IN|K_X_BYTE:
			inb(t & K_X_PORT);
			break;

		case K_X_IN|K_X_WORD:
			inw(t & K_X_PORT);
			break;

		case K_X_IN|K_X_LONG:
			inl(t & K_X_PORT);
			break;

		case K_X_OUT|K_X_BYTE:
			outb(t & K_X_PORT, p[1]);
			break;

		case K_X_OUT|K_X_WORD:
			outw(t & K_X_PORT, p[1]);
			break;

		case K_X_OUT|K_X_LONG:
			outl(t & K_X_PORT, p[1]);
			break;
	}
}

void
X_kdb_enter(void)
{
	u_int *u_ip, *endp;

	for (u_ip = X_kdb_enter_str, endp = &X_kdb_enter_str[X_kdb_enter_len];
	     u_ip < endp;
	     u_ip += 2)
	    kdb_in_out(u_ip);
}

void
X_kdb_exit(void)
{
	u_int *u_ip, *endp;

	for (u_ip = X_kdb_exit_str, endp = &X_kdb_exit_str[X_kdb_exit_len];
	     u_ip < endp;
	     u_ip += 2)
	   kdb_in_out(u_ip);
}

io_return_t
X_kdb_enter_init(data, count)
    u_int *data;
    u_int count;
{
    if (count * sizeof X_kdb_enter_str[0] > sizeof X_kdb_enter_str)
	return D_INVALID_OPERATION;

    memcpy(X_kdb_enter_str, data, count * sizeof X_kdb_enter_str[0]);
    X_kdb_enter_len = count;
    return D_SUCCESS;
}

io_return_t
X_kdb_exit_init(data, count)
    u_int *data;
    u_int count;
{
    if (count * sizeof X_kdb_exit_str[0] > sizeof X_kdb_exit_str)
	return D_INVALID_OPERATION;

    memcpy(X_kdb_exit_str, data, count * sizeof X_kdb_exit_str[0]);
    X_kdb_exit_len = count;
    return D_SUCCESS;
}
