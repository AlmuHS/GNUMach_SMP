/*
 * Copyright (C) 2008 Free Software Foundation, Inc.
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
 *  Author: Barry deFreese.
 */
/*
 *     Device auto configuration.
 *
 */

#ifndef _AUTOCONF_H_
#define _AUTOCONF_H_

#include <mach/std_types.h>
#include <chips/busses.h>

/*
 * probeio:
 *
 *  Probe and subsequently attach devices out on the AT bus.
 *
 *
 */
void probeio(void);

extern void take_dev_irq (
    struct bus_device *dev);

#endif /* _AUTOCONF_H_ */
