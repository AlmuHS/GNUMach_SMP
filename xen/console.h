/*
 *  Copyright (C) 2006-2009, 2011 Free Software Foundation
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef XEN_CONSOLE_H
#define XEN_CONSOLE_H
#include <machine/xen.h>
#include <string.h>

#include <mach/port.h>
#include <device/cons.h>
#include <device/io_req.h>

static inline void hyp_console_write(const char *str, int len)
{
	hyp_console_io (CONSOLEIO_write, len, kvtolin(str));
}

#define hyp_console_put(str) ({ \
	const char *__str = (void*) (str); \
	hyp_console_write (__str, strlen (__str)); \
})

extern void hyp_console_init(void);

extern int hypcnputc(dev_t dev, int c);
extern int hypcngetc(dev_t dev, int wait);
extern int hypcnprobe(struct consdev *cp);
extern int hypcninit(struct consdev *cp);

extern int hypcnopen(dev_t dev, int flag, io_req_t ior);
extern int hypcnread(dev_t dev, io_req_t ior);
extern int hypcnwrite(dev_t dev, io_req_t ior);
extern void hypcnclose(dev_t dev, int flag);
extern io_return_t hypcngetstat(dev_t dev, dev_flavor_t flavor, dev_status_t data, mach_msg_type_number_t *count);
extern io_return_t hypcnsetstat(dev_t dev, dev_flavor_t flavor, dev_status_t data, mach_msg_type_number_t count);
extern int hypcnportdeath(dev_t dev, mach_port_t port);

#endif /* XEN_CONSOLE_H */
