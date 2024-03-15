/*
 * Copyright (C) 2023 Free Software Foundation, Inc.
 *
 * This file is part of GNU Mach.
 *
 * GNU Mach is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _DEVICE_INPUT_H
#define _DEVICE_INPUT_H

#include <mach/boolean.h>
#include <mach/time_value.h>

/*
 * Ioctl's have the command encoded in the lower word, and the size of
 * any in or out parameters in the upper word.  The high 3 bits of the
 * upper word are used to encode the in/out status of the parameter.
 */
#define	IOCPARM_MASK	0x1fff		/* parameter length, at most 13 bits */
#define	IOC_VOID	0x20000000	/* no parameters */
#define	IOC_OUT		0x40000000	/* copy out parameters */
#define	IOC_IN		0x80000000U	/* copy in parameters */
#define	IOC_INOUT	(IOC_IN|IOC_OUT)

#define _IOC(inout,group,num,len) \
	(inout | ((len & IOCPARM_MASK) << 16) | ((group) << 8) | (num))
#define	_IO(g,n)	_IOC(IOC_VOID,	(g), (n), 0)
#define	_IOR(g,n,t)	_IOC(IOC_OUT,	(g), (n), sizeof(t))
#define	_IOW(g,n,t)	_IOC(IOC_IN,	(g), (n), sizeof(t))
#define	_IOWR(g,n,t)	_IOC(IOC_INOUT,	(g), (n), sizeof(t))

typedef uint8_t Scancode;
typedef uint16_t kev_type;		/* kd event type */

/* (used for event records) */
struct mouse_motion {		
	short mm_deltaX;		/* units? */
	short mm_deltaY;
};

typedef struct {
	kev_type type;			/* see below */
	/*
	 * This is not used anymore but is kept for backwards compatibility.
	 * Note the use of rpc_time_value to ensure compatibility for a 64 bit kernel and
	 * 32 bit user land.
	 */
	struct rpc_time_value unused_time;	/* timestamp*/
	union {				/* value associated with event */
		boolean_t up;		/* MOUSE_LEFT .. MOUSE_RIGHT */
		Scancode sc;		/* KEYBD_EVENT */
		struct mouse_motion mmotion;	/* MOUSE_MOTION */
	} value;
} kd_event;
#define m_deltaX	mmotion.mm_deltaX
#define m_deltaY	mmotion.mm_deltaY

/* 
 * kd_event ID's.
 */
#define MOUSE_LEFT	1		/* mouse left button up/down */
#define MOUSE_MIDDLE	2
#define MOUSE_RIGHT	3
#define MOUSE_MOTION	4		/* mouse motion */
#define KEYBD_EVENT	5		/* key up/down */

/* Keyboard ioctls */

/*
 * KDSKBDMODE - When the console is in "ascii" mode, keyboard events are
 * converted to Ascii characters that are readable from /dev/console.
 * When the console is in "event" mode, keyboard events are
 * timestamped and queued up on /dev/kbd as kd_events.  When the last
 * close is done on /dev/kbd, the console automatically reverts to ascii
 * mode.
 * When /dev/mouse is opened, mouse events are timestamped and queued
 * on /dev/mouse, again as kd_events.
 *
 * KDGKBDTYPE - Returns the type of keyboard installed.  Currently
 * there is only one type, KB_VANILLAKB, which is your standard PC-AT
 * keyboard.
 */

#define KDSKBDMODE	_IOW('K', 1, int)	/* set keyboard mode */
#define KB_EVENT	1
#define KB_ASCII	2

#define KDGKBDTYPE	_IOR('K', 2, int)	/* get keyboard type */
#define KB_VANILLAKB	0

#define KDSETLEDS	_IOW('K', 5, int)	/* set the keyboard ledstate */

#endif /* _DEVICE_INPUT_H */
