/*
 * Copyright (c) 2013 Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _DEVICE_CHARIO_H_
#define _DEVICE_CHARIO_H_

#include <device/tty.h>

extern void chario_init(void);

void queue_delayed_reply(
	queue_t		qh,
	io_req_t	ior,
	boolean_t	(*io_done)(io_req_t));

void tty_output(struct tty *tp);

boolean_t char_open_done(io_req_t);
boolean_t char_read_done(io_req_t);
boolean_t char_write_done(io_req_t);

#endif /* _DEVICE_CHARIO_H_ */
