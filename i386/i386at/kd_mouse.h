/*
 * Mouse event handlers
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
 *     Mouse event handling functions.
 *
 */

#ifndef _KD_MOUSE_H_
#define _KD_MOUSE_H_

#include <sys/types.h>

extern void mouse_button (kev_type which, u_char direction);

extern void mouse_enqueue (kd_event *ev);

extern void mouse_moved (struct mouse_motion where);

extern void mouse_handle_byte (u_char ch);

extern void serial_mouse_open (dev_t dev);

extern void serial_mouse_close (dev_t dev, int flags);

extern void kd_mouse_open (dev_t dev, int mouse_pic);

extern void kd_mouse_close (dev_t dev, int mouse_pic);

extern void ibm_ps2_mouse_open (dev_t dev);

extern void ibm_ps2_mouse_close (dev_t dev);

extern void mouse_packet_microsoft_mouse (u_char *mousebuf);

extern void mouse_packet_mouse_system_mouse (u_char *mousebuf);

extern void mouse_packet_ibm_ps2_mouse (u_char *mousebuf);

#endif /* _KD_MOUSE_H_ */
