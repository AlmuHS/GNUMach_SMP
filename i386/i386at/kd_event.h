/*
 * Keyboard event handlers
 * Copyright (C) 2006 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Barry deFreese.
 */
/*
 *     Keyboard event handling functions.
 *
 */

#ifndef _KD_EVENT_H_
#define _KD_EVENT_H_

#include <sys/types.h>
#include <device/io_req.h>
#include <i386at/kd.h>

extern void X_kdb_enter (void);

extern void X_kdb_exit (void);

extern int kbdopen(dev_t dev, int flags);
extern void kbdclose(dev_t dev, int flags);
extern int kbdread(dev_t dev, io_req_t ior);

extern io_return_t kbdgetstat(
	dev_t		dev,
	int		flavor,
	int 		*data,
	unsigned int	*count);

extern io_return_t kbdsetstat(
	dev_t		dev,
	int		flavor,
	int 		*data,
	unsigned int	count);

extern void kd_enqsc(Scancode sc);

void kbd_enqueue(kd_event *ev);

io_return_t X_kdb_enter_init(u_int *data, u_int count);
io_return_t X_kdb_exit_init(u_int *data, u_int count);

boolean_t kbd_read_done(io_req_t ior);

#endif /* _KD_EVENT_H_ */
