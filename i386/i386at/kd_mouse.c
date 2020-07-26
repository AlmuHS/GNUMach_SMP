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
 File:         kd_mouse.c
 Description:  mouse driver as part of keyboard/display driver

 $ Header: $

 Copyright Ing. C. Olivetti & C. S.p.A. 1989.
 All rights reserved.
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

/*
 * Hacked up support for serial mouse connected to COM1, using Mouse
 * Systems 5-byte protocol at 1200 baud.  This should work for
 * Mouse Systems, SummaMouse, and Logitek C7 mice.
 *
 * The interface provided by /dev/mouse is a series of events as
 * described in i386at/kd.h.
 */

#include <mach/boolean.h>
#include <sys/types.h>
#include <kern/printf.h>
#include <device/ds_routines.h>
#include <device/device_types.h>
#include <device/io_req.h>
#include <device/subrs.h>
#include <i386/ipl.h>
#include <i386/pic.h>
#include <i386/pio.h>
#include <chips/busses.h>
#include <i386at/com.h>
#include <i386at/kd.h>
#include <i386at/kd_queue.h>
#include <i386at/i8250.h>

#include "kd_mouse.h"

static void (*oldvect)();		/* old interrupt vector */
static int oldunit;
static spl_t oldspl;
extern	struct	bus_device *cominfo[];

kd_event_queue mouse_queue;		/* queue of mouse events */
boolean_t mouse_in_use = FALSE;
queue_head_t	mouse_read_queue = { &mouse_read_queue, &mouse_read_queue };


/*
 * The state of the 3 buttons is encoded in the low-order 3 bits (both
 * here and in other variables in the driver).
 */
u_char lastbuttons;		/* previous state of mouse buttons */
#define MOUSE_UP	1
#define MOUSE_DOWN	0
#define MOUSE_ALL_UP	0x7

int mouse_baud = BCNT1200;

boolean_t	mouse_char_cmd = FALSE;		/* mouse response is to cmd */
boolean_t	mouse_char_wanted = FALSE;	/* want mouse response */
int		mouse_char_index;		/* mouse response */


/*
 * init_mouse_hw - initialize the serial port.
 */
void
init_mouse_hw(dev_t unit, int mode)
{
	unsigned short base_addr  = cominfo[unit]->address;

	outb(base_addr + RIE, 0);
	outb(base_addr + RLC, LCDLAB);
	outb(base_addr + RDLSB, mouse_baud & 0xff);
	outb(base_addr + RDMSB, (mouse_baud >> 8) & 0xff);
	outb(base_addr + RLC, mode);
	outb(base_addr + RMC, MCDTR | MCRTS | MCOUT2);
	outb(base_addr + RIE, IERD | IELS);
}


/*
 * mouseopen - Verify that the request is read-only, initialize,
 * and remember process group leader.
 */
/*
 * Low 3 bits of minor are the com port #.
 * The high 5 bits of minor are the mouse type
 */
#define	MOUSE_SYSTEM_MOUSE	0
#define MICROSOFT_MOUSE		1
#define IBM_MOUSE		2
#define NO_MOUSE		3
#define LOGITECH_TRACKMAN	4
#define	MICROSOFT_MOUSE7	5
static int mouse_type;
static int mousebufsize;
static int mousebufindex = 0;
int track_man[10];

/*ARGSUSED*/
int
mouseopen(dev, flags, ior)
	dev_t dev;
	int flags;
	io_req_t ior;
{
	if (mouse_in_use)
		return (D_ALREADY_OPEN);
	mouse_in_use = TRUE;		/* locking? */
	kdq_reset(&mouse_queue);
	lastbuttons = MOUSE_ALL_UP;

	switch (mouse_type = ((minor(dev) & 0xf8) >> 3)) {
	case MICROSOFT_MOUSE7:
		mousebufsize = 3;
		serial_mouse_open(dev);
		init_mouse_hw(dev&7, LC7);
		break;
	case MICROSOFT_MOUSE:
		mousebufsize = 3;
		serial_mouse_open(dev);
		init_mouse_hw(dev&7, LC8);
		break;
	case MOUSE_SYSTEM_MOUSE:
		mousebufsize = 5;
		serial_mouse_open(dev);
		init_mouse_hw(dev&7, LC8);
		break;
	case LOGITECH_TRACKMAN:
		mousebufsize = 3;
		serial_mouse_open(dev);
		init_mouse_hw(dev&7, LC7);
		track_man[0] = comgetc(dev&7);
		track_man[1] = comgetc(dev&7);
		if (track_man[0] != 0x4d &&
		    track_man[1] != 0x33) {
			printf("LOGITECH_TRACKMAN: NOT M3");
		}
		break;
	case IBM_MOUSE:
		mousebufsize = 3;
		kd_mouse_open(dev, 12);
		ibm_ps2_mouse_open(dev);
		break;
	case NO_MOUSE:
		break;
	}
	mousebufindex = 0;
	return(0);
}

void
serial_mouse_open(dev_t dev)
{
	int unit = minor(dev) & 0x7;
	int mouse_pic = cominfo[unit]->sysdep1;

	spl_t s = splhi();		/* disable interrupts */

	oldvect = ivect[mouse_pic];
	ivect[mouse_pic] = mouseintr;

	oldunit = iunit[mouse_pic];
	iunit[mouse_pic] = unit;

				/* XXX other arrays to init? */
	splx(s);		/* XXX - should come after init? */
}

int mouse_packets = 0;

void
kd_mouse_open(
	dev_t 	dev,
	int 	mouse_pic)
{
	spl_t s = splhi();	/* disable interrupts */

	oldvect = ivect[mouse_pic];
	ivect[mouse_pic] = kdintr;
	oldspl = intpri[mouse_pic];
	intpri[mouse_pic] = SPL6;
	form_pic_mask();
	splx(s);
}

/*
 * mouseclose - Disable interrupts on the serial port, reset driver flags,
 * and restore the serial port interrupt vector.
 */
void
mouseclose(
	dev_t 	dev,
	int 	flags)
{
	switch (mouse_type) {
	case MICROSOFT_MOUSE:
	case MICROSOFT_MOUSE7:
	case MOUSE_SYSTEM_MOUSE:
	case LOGITECH_TRACKMAN:
		serial_mouse_close(dev, flags);
		break;
	case IBM_MOUSE:
		ibm_ps2_mouse_close(dev);
		kd_mouse_close(dev, 12);
		{int i = 20000; for (;i--;); }
		kd_mouse_drain();
		break;
	case NO_MOUSE:
		break;
	}

	kdq_reset(&mouse_queue);		/* paranoia */
	mouse_in_use = FALSE;
}

/*ARGSUSED*/
void
serial_mouse_close(
	dev_t 	dev,
	int 	flags)
{
	spl_t o_pri = splhi();		/* mutex with open() */
	int unit = minor(dev) & 0x7;
	int mouse_pic = cominfo[unit]->sysdep1;
	unsigned short base_addr  = cominfo[unit]->address;

	assert(ivect[mouse_pic] == mouseintr);
	outb(base_addr + RIE, 0);	/* disable serial port */
	outb(base_addr + RMC, 0);	/* no rts */
	ivect[mouse_pic] = oldvect;
	iunit[mouse_pic] = oldunit;

	(void)splx(o_pri);
}

void
kd_mouse_close(
	dev_t 	dev,
	int 	mouse_pic)
{
	spl_t s = splhi();

	ivect[mouse_pic] = oldvect;
	intpri[mouse_pic] = oldspl;
	form_pic_mask();
	splx(s);
}

io_return_t mousegetstat(
	dev_t		  dev,
	dev_flavor_t	  flavor,
	dev_status_t	  data,		/* pointer to OUT array */
	mach_msg_type_number_t	*count)	/* OUT */
{
	switch (flavor) {
	    case DEV_GET_SIZE:
		data[DEV_GET_SIZE_DEVICE_SIZE] = 0;
		data[DEV_GET_SIZE_RECORD_SIZE] = sizeof(kd_event);
		*count = DEV_GET_SIZE_COUNT;
		break;
	    default:
		return D_INVALID_OPERATION;
	}
	return D_SUCCESS;
}


/*
 * mouseread - dequeue and return any queued events.
 */
int
mouseread(
	dev_t		dev,
	io_req_t	ior)
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
	if (kdq_empty(&mouse_queue)) {
	    if (ior->io_mode & D_NOWAIT) {
		splx(s);
		return (D_WOULD_BLOCK);
	    }
	    ior->io_done = mouse_read_done;
	    enqueue_tail(&mouse_read_queue, (queue_entry_t)ior);
	    splx(s);
	    return (D_IO_QUEUED);
	}
	count = 0;
	while (!kdq_empty(&mouse_queue) && count < ior->io_count) {
	    kd_event *ev;

	    ev = kdq_get(&mouse_queue);
	    *(kd_event *)(&ior->io_data[count]) = *ev;
	    count += sizeof(kd_event);
	}
	splx(s);
	ior->io_residual = ior->io_count - count;
	return (D_SUCCESS);
}

boolean_t mouse_read_done(io_req_t ior)
{
	int	count;
	spl_t	s;

	s = SPLKD();
	if (kdq_empty(&mouse_queue)) {
	    ior->io_done = mouse_read_done;
	    enqueue_tail(&mouse_read_queue, (queue_entry_t)ior);
	    splx(s);
	    return (FALSE);
	}

	count = 0;
	while (!kdq_empty(&mouse_queue) && count < ior->io_count) {
	    kd_event *ev;

	    ev = kdq_get(&mouse_queue);
	    *(kd_event *)(&ior->io_data[count]) = *ev;
	    count += sizeof(kd_event);
	}
	splx(s);

	ior->io_residual = ior->io_count - count;
	ds_read_done(ior);

	return (TRUE);
}



/*
 * mouseintr - Get a byte and pass it up for handling.  Called at SPLKD.
 */
void
mouseintr(int unit)
{
	unsigned short base_addr  = cominfo[unit]->address;
	unsigned char id, ls;

	/* get reason for interrupt and line status */
	id = inb(base_addr + RID);
	ls = inb(base_addr + RLS);

	/* handle status changes */
	if (id == IDLS) {
		if (ls & LSDR) {
			inb(base_addr + RDAT);	/* flush bad character */
		}
		return;			/* ignore status change */
	}

	if (id & IDRD) {
		mouse_handle_byte((u_char)(inb(base_addr + RDAT) & 0xff));
	}
}


/*
 * handle_byte - Accumulate bytes until we have an entire packet.
 * If the mouse has moved or any of the buttons have changed state (up
 * or down), enqueue the corresponding events.
 * Called at SPLKD.
 * XXX - magic numbers.
 */
int show_mouse_byte = 0;
/*
   X down; middle down; middle up; X up		50 0 0; 50 0 0 22; 50 0 0 02; 40 0 0
   X down; middle down; X up; middle up		50 0 0; 50 0 0 22; 40 0 0 22; 40 0 0 2
 *
 * The trick here is that all the while the middle button is down you get 4 byte
 * packets with the last byte 0x22.  When the middle button goes up you get a
 * last packet with 0x02.
 */
int lastgitech = 0x40;		/* figure whether the first 3 bytes imply */
 				/* its time to expect a fourth */
int fourthgitech = 0;		/* look for the 4th byte; we must process it */
int middlegitech = 0;		/* what should the middle button be */

#define MOUSEBUFSIZE	5		/* num bytes def'd by protocol */
static u_char mousebuf[MOUSEBUFSIZE];	/* 5-byte packet from mouse */

void
mouse_handle_byte(u_char ch)
{
	if (show_mouse_byte) {
		printf("%x(%c) ", ch, ch);
	}

	if (mouse_char_cmd) {
	    /*
	     *	Mouse character is response to command
	     */
	    if (mousebufindex < mousebufsize)
	        mousebuf[mousebufindex++] = ch;
	    if (mouse_char_wanted) {
		mouse_char_wanted = FALSE;
		wakeup((vm_offset_t)&mousebuf);
	    }
	    return;
	}

	if (mousebufindex == 0) {
		switch (mouse_type) {
		case MICROSOFT_MOUSE7:
			if ((ch & 0x40) != 0x40)
				return;
			break;
		case MICROSOFT_MOUSE:
			if ((ch & 0xc0) != 0xc0)
				return;
			break;
		case MOUSE_SYSTEM_MOUSE:
			if ((ch & 0xf8) != 0x80)
				return;
			break;
		case LOGITECH_TRACKMAN:
			if (fourthgitech == 1) {
				fourthgitech = 0;
				if (ch & 0xf0)
					middlegitech = 0x4;
				else
					middlegitech = 0x0;
				mouse_packet_microsoft_mouse(mousebuf);
				return;
			} else if ((ch & 0xc0) != 0x40)
				return;
			break;
		case IBM_MOUSE:
			break;
		}
	}

	mousebuf[mousebufindex++] = ch;
	if (mousebufindex < mousebufsize)
		return;

	/* got a packet */
	mousebufindex = 0;

	switch (mouse_type) {
	case MICROSOFT_MOUSE7:
	case MICROSOFT_MOUSE:
		mouse_packet_microsoft_mouse(mousebuf);
		break;
	case MOUSE_SYSTEM_MOUSE:
		mouse_packet_mouse_system_mouse(mousebuf);
		break;
	case LOGITECH_TRACKMAN:
		if ( mousebuf[1] || mousebuf[2] ||
		     mousebuf[0] != lastgitech) {
		     	mouse_packet_microsoft_mouse(mousebuf);
			lastgitech = mousebuf[0] & 0xf0;
		} else {
			fourthgitech = 1;
		}
		break;
	case IBM_MOUSE:
		mouse_packet_ibm_ps2_mouse(mousebuf);
		break;
	}
}

void
mouse_packet_mouse_system_mouse(u_char mousebuf[MOUSEBUFSIZE])
{
	u_char buttons, buttonchanges;
	struct mouse_motion moved;

	buttons = mousebuf[0] & 0x7;	/* get current state of buttons */
	buttonchanges = buttons ^ lastbuttons;
	moved.mm_deltaX = (char)mousebuf[1] + (char)mousebuf[3];
	moved.mm_deltaY = (char)mousebuf[2] + (char)mousebuf[4];

	if (moved.mm_deltaX != 0 || moved.mm_deltaY != 0)
		mouse_moved(moved);

	if (buttonchanges != 0) {
		lastbuttons = buttons;
		if (buttonchanges & 1)
			mouse_button(MOUSE_RIGHT, buttons & 1);
		if (buttonchanges & 2)
			mouse_button(MOUSE_MIDDLE, (buttons & 2) >> 1);
		if (buttonchanges & 4)
			mouse_button(MOUSE_LEFT, (buttons & 4) >> 2);
	}
}

/* same as above for microsoft mouse */
/*
 * 3 byte microsoft format used
 *
 * 7  6  5  4  3  2  1  0
 * 1  1  L  R  Y7 Y6 X7 X6
 * 1  0  X5 X4 X3 X3 X1 X0
 * 1  0  Y5 Y4 Y3 Y2 Y1 Y0
 *
 */
void
mouse_packet_microsoft_mouse(u_char mousebuf[MOUSEBUFSIZE])
{
	u_char buttons, buttonchanges;
	struct mouse_motion moved;

	buttons = ((mousebuf[0] & 0x30) >> 4);
	buttons |= middlegitech;
			/* get current state of buttons */
#ifdef	gross_hack
	if (buttons == 0x03)	/* both buttons down */
		buttons = 0x04;
#endif	/* gross_hack */
	buttons = (~buttons) & 0x07;	/* convert to not pressed */

	buttonchanges = buttons ^ lastbuttons;
	moved.mm_deltaX = ((mousebuf[0] & 0x03) << 6) | (mousebuf[1] & 0x3F);
	moved.mm_deltaY = ((mousebuf[0] & 0x0c) << 4) | (mousebuf[2] & 0x3F);
	if (moved.mm_deltaX & 0x80)	/* negative, in fact */
		moved.mm_deltaX = moved.mm_deltaX - 0x100;
	if (moved.mm_deltaY & 0x80)	/* negative, in fact */
		moved.mm_deltaY = moved.mm_deltaY - 0x100;
	/* and finally the Y orientation is different for the microsoft mouse */
	moved.mm_deltaY = -moved.mm_deltaY;

	if (moved.mm_deltaX != 0 || moved.mm_deltaY != 0)
		mouse_moved(moved);

	if (buttonchanges != 0) {
		lastbuttons = buttons;
		if (buttonchanges & 1)
			mouse_button(MOUSE_RIGHT, (buttons & 1) ?
						MOUSE_UP : MOUSE_DOWN);
		if (buttonchanges & 2)
			mouse_button(MOUSE_LEFT, (buttons & 2) ?
						MOUSE_UP : MOUSE_DOWN);
		if (buttonchanges & 4)
			mouse_button(MOUSE_MIDDLE, (buttons & 4) ?
						MOUSE_UP : MOUSE_DOWN);
	}
}

/*
 *	AUX device (PS2) open/close
 */

/*
 *	Write character to mouse.  Called at spltty.
 */
void kd_mouse_write(
	unsigned char	ch)
{
	while (inb(K_STATUS) & K_IBUF_FUL)
	    continue;		/* wait for 'input' port empty */
	outb(K_CMD, 0xd4);	/* send next character to mouse */

	while (inb(K_STATUS) & K_IBUF_FUL)
	    continue;		/* wait for 'input' port empty */
	outb(K_RDWR, ch);	/* send command to mouse */
}

/*
 *	Read next character from mouse, waiting for interrupt
 *	to deliver it.  Called at spltty.
 */
int kd_mouse_read(void)
{
	int	ch;

	if (mouse_char_index >= mousebufsize)
	    return -1;

	while (mousebufindex <= mouse_char_index) {
	    mouse_char_wanted = TRUE;
	    assert_wait((event_t) &mousebuf, FALSE);
	    /* We are at tty SPL level, interrupts can not happen between
	     * assert_wait and thread_block.  */
	    thread_block((void (*)()) 0);
	}

	ch = mousebuf[mouse_char_index++];

	return ch;
}

/*
 *	Prepare buffer for receiving next packet from mouse.
 */
void kd_mouse_read_reset(void)
{
	mousebufindex = 0;
	mouse_char_index = 0;
}

void
ibm_ps2_mouse_open(dev_t dev)
{
	spl_t	s = spltty();

	lastbuttons = 0;
	mouse_char_cmd = TRUE;	/* responses are to commands */

	kd_sendcmd(0xa8);	/* enable mouse in kbd */

	kd_cmdreg_write(0x47);	/* allow mouse interrupts */
				/* magic number for ibm? */

	kd_mouse_read_reset();
	kd_mouse_write(0xff);	/* reset mouse */
	if (kd_mouse_read() != 0xfa) {
	    splx(s);
	    return;		/* need ACK */
	}

	(void) kd_mouse_read();	/* discard 2-character mouse ID */
	(void) kd_mouse_read();

	kd_mouse_read_reset();
	kd_mouse_write(0xea);	/* set stream mode */
	if (kd_mouse_read() != 0xfa) {
	    splx(s);
	    return;		/* need ACK */
	}

	kd_mouse_read_reset();
	kd_mouse_write(0xf4);	/* enable */
	if (kd_mouse_read() != 0xfa) {
	    splx(s);
	    return;		/* need ACK */
	}

	kd_mouse_read_reset();
	mouse_char_cmd = FALSE;	/* now we get mouse packets */

	splx(s);
}

void
ibm_ps2_mouse_close(dev_t dev)
{
	spl_t	s = spltty();

	mouse_char_cmd = TRUE;	/* responses are to commands */

	kd_mouse_read_reset();
	kd_mouse_write(0xff);	/* reset mouse */
	if (kd_mouse_read() == 0xfa) {
	    /* got ACK: discard 2-char mouse ID */
	    (void) kd_mouse_read();
	    (void) kd_mouse_read();
	}

	kd_sendcmd(0xa7);	/* disable mouse in kbd */
	kd_cmdreg_write(0x65);	/* disallow mouse interrupts */
				/* magic number for ibm? */

	splx(s);
}

/*
 * 3 byte ibm ps2 format used
 *
 * 7  6  5  4  3  2  1  0
 * YO XO YS XS 1  M  R  L
 * X7 X6 X5 X4 X3 X3 X1 X0
 * Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
 *
 */
void
mouse_packet_ibm_ps2_mouse(u_char mousebuf[MOUSEBUFSIZE])
{
	u_char buttons, buttonchanges;
	struct mouse_motion moved;

	buttons = mousebuf[0] & 0x7;	/* get current state of buttons */
	buttonchanges = buttons ^ lastbuttons;
	moved.mm_deltaX = ((mousebuf[0]&0x10) ? 0xffffff00 : 0 ) | (u_char)mousebuf[1];
	moved.mm_deltaY = ((mousebuf[0]&0x20) ? 0xffffff00 : 0 ) | (u_char)mousebuf[2];
	if (mouse_packets) {
		printf("(%x:%x:%x)", mousebuf[0], mousebuf[1], mousebuf[2]);
		return;
	}

	if (moved.mm_deltaX != 0 || moved.mm_deltaY != 0)
		mouse_moved(moved);

	if (buttonchanges != 0) {
		lastbuttons = buttons;
		if (buttonchanges & 1)
			mouse_button(MOUSE_LEFT,   !(buttons & 1));
		if (buttonchanges & 2)
			mouse_button(MOUSE_RIGHT,  !((buttons & 2) >> 1));
		if (buttonchanges & 4)
			mouse_button(MOUSE_MIDDLE, !((buttons & 4) >> 2));
	}
}

/*
 * Enqueue a mouse-motion event.  Called at SPLKD.
 */
void
mouse_moved(struct mouse_motion where)
{
	kd_event ev;

	ev.type = MOUSE_MOTION;
	ev.time = time;
	ev.value.mmotion = where;
	mouse_enqueue(&ev);
}

/*
 * Enqueue an event for mouse button press or release.  Called at SPLKD.
 */
void
mouse_button(
	kev_type 	which,
	u_char 		direction)
{
	kd_event ev;

	ev.type = which;
	ev.time = time;
	ev.value.up = (direction == MOUSE_UP) ? TRUE : FALSE;
	mouse_enqueue(&ev);
}

/*
 * mouse_enqueue - enqueue an event and wake up selecting processes, if
 * any.  Called at SPLKD.
 */

void
mouse_enqueue(kd_event *ev)
{
	if (kdq_full(&mouse_queue))
		printf_once("mouse: queue full\n");
	else
		kdq_put(&mouse_queue, ev);

	{
	    io_req_t	ior;
	    while ((ior = (io_req_t)dequeue_head(&mouse_read_queue)) != 0)
		iodone(ior);
	}
}
