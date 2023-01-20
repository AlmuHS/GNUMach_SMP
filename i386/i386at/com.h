/*
 * Communication functions
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
 *     Communication functions.
 *
 */

#ifndef _COM_H_
#define _COM_H_

#include <mach/std_types.h>
#include <device/cons.h>
#include <device/tty.h>
#include <chips/busses.h>

/*
 * Set receive modem state from modem status register.
 */
extern void fix_modem_state(int unit, int modem_stat);

extern void comtimer(void * param);

/*
 * Modem change (input signals)
 */
extern void commodem_intr(int unit, int stat);

extern int comgetc(int unit);

extern int comcnprobe(struct consdev *cp);
extern int comcninit(struct consdev *cp);
extern int comcngetc(dev_t dev, int wait);
extern int comcnputc(dev_t dev, int c);
extern void comintr(int unit);

int comprobe(vm_offset_t port, struct bus_ctlr *dev);
int commctl(struct tty *tp, int bits, int how);
void comstart(struct tty *tp);
void comstop(struct tty *tp, int flags);
void comattach(struct bus_device *dev);

extern io_return_t
comgetstat(
	dev_t		dev,
	dev_flavor_t	flavor,
	dev_status_t	data,
	mach_msg_type_number_t	*count);

extern io_return_t
comsetstat(
	dev_t		dev,
	dev_flavor_t	flavor,
	dev_status_t	data,
	mach_msg_type_number_t	count);

#if MACH_KDB
extern void kdb_kintr(void);
extern void compr_addr(vm_offset_t addr);
extern int compr(int unit);
#endif /* MACH_KDB */

extern io_return_t comopen(dev_t dev, int flag, io_req_t ior);
extern void comclose(dev_t dev, int flag);
extern io_return_t comread(dev_t dev, io_req_t ior);
extern io_return_t comwrite(dev_t dev, io_req_t ior);
extern io_return_t comportdeath(dev_t dev, mach_port_t port);

#endif /* _COM_H_ */
