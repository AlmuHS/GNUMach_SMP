/*
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University
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
 * Device switch for i386 AT bus.
 */

#include <mach/machine/vm_types.h>
#include <device/conf.h>
#include <kern/mach_clock.h>
#include <i386at/model_dep.h>

#define	timename		"time"

#ifndef	MACH_HYP
#include <i386at/kd.h>
#define	kdname			"kd"

#if	NCOM > 0
#include <i386at/com.h>
#define	comname			"com"
#endif	/* NCOM > 0 */

#if	NLPR > 0
#include <i386at/lpr.h>
#define	lprname			"lpr"
#endif	/* NLPR > 0 */
#endif	/* MACH_HYP */

#include <i386at/kd_event.h>
#define	kbdname			"kbd"

#ifndef	MACH_HYP
#include <i386at/kd_mouse.h>
#define	mousename		"mouse"

#include <i386at/mem.h>
#define	memname			"mem"
#endif	/* MACH_HYP */

#include <device/kmsg.h>
#define kmsgname		"kmsg"

#ifdef	MACH_HYP
#include <xen/console.h>
#define hypcnname		"hyp"
#endif	/* MACH_HYP */

#include <device/intr.h>
#define irqname			"irq"

/*
 * List of devices - console must be at slot 0
 */
struct dev_ops	dev_name_list[] =
{
	/*name,		open,		close,		read,
	  write,	getstat,	setstat,	mmap,
	  async_in,	reset,		port_death,	subdev,
	  dev_info */

	/* We don't assign a console here, when we find one via
	   cninit() we stick something appropriate here through the
	   indirect list */
	{ "cn",		nulldev_open,	nulldev_close,	nulldev_read,
	  nulldev_write,	nulldev_getstat,	nulldev_setstat,	nomap,
	  nodev_async_in,	nulldev_reset,	nulldev_portdeath,	0,
	  nodev_info},

#ifndef	MACH_HYP
#if	ENABLE_IMMEDIATE_CONSOLE
	{ "immc",	nulldev_open,	nulldev_close,	nulldev_read,
	  nulldev_write,	nulldev_getstat,	nulldev_setstat,
	  nomap,	nodev_async_in,	nulldev_reset,	nulldev_portdeath,	0,
	  nodev_info },
#endif	/* ENABLE_IMMEDIATE_CONSOLE */
	{ kdname,	kdopen,		kdclose,	kdread,
	  kdwrite,	kdgetstat,	kdsetstat,	kdmmap,
	  nodev_async_in,	nulldev_reset,	kdportdeath,	0,
	  nodev_info },
#endif	/* MACH_HYP */

	{ timename,	timeopen,	timeclose,	nulldev_read,
	  nulldev_write,	nulldev_getstat,	nulldev_setstat,	timemmap,
	  nodev_async_in,	nulldev_reset,	nulldev_portdeath,	0,
	  nodev_info },

#ifndef	MACH_HYP
#if	NCOM > 0
	{ comname,	comopen,	comclose,	comread,
	  comwrite,	comgetstat,	comsetstat,	nomap,
	  nodev_async_in,	nulldev_reset,	comportdeath,	0,
	  nodev_info },
#endif

#ifdef MACH_LPR
	{ lprname,	lpropen,	lprclose,	lprread,
	  lprwrite,	lprgetstat,	lprsetstat,	nomap,
	  nodev_async_in,	nulldev_reset,	lprportdeath,	0,
	  nodev_info },
#endif

	{ mousename,	mouseopen,	mouseclose,	mouseread,
	  nulldev_write,	mousegetstat,	nulldev_setstat,	nomap,
	  nodev_async_in,	nulldev_reset,	nulldev_portdeath,	0,
	  nodev_info },

	{ kbdname,	kbdopen,	kbdclose,	kbdread,
	  nulldev_write,	kbdgetstat,	kbdsetstat,	nomap,
	  nodev_async_in,	nulldev_reset,	nulldev_portdeath,	0,
	  nodev_info },

	{ memname,	nulldev_open,	nulldev_close,	nulldev_read,
	  nulldev_write,	nulldev_getstat,	nulldev_setstat,		memmmap,
	  nodev_async_in,	nulldev_reset,	nulldev_portdeath,	0,
	  nodev_info },
#endif	/* MACH_HYP */

#ifdef	MACH_KMSG
        { kmsgname,     kmsgopen,       kmsgclose,       kmsgread,
          nulldev_write,        kmsggetstat,    nulldev_setstat,           nomap,
          nodev_async_in,        nulldev_reset,        nulldev_portdeath,         0,
          nodev_info },
#endif

#ifdef	MACH_HYP
	{ hypcnname,	hypcnopen,	hypcnclose,	hypcnread,
	  hypcnwrite,	hypcngetstat,	hypcnsetstat,	nomap,
	  nodev_async_in,	nulldev_reset,	hypcnportdeath,	0,
	  nodev_info },
#endif	/* MACH_HYP */

        { irqname,      nulldev_open,   nulldev_close,    nulldev_read,
          nulldev_write,nulldev_getstat,nulldev_setstat,  nomap,
          nodev_async_in,        nulldev_reset,        nulldev_portdeath,0,
          nodev_info },

};
int	dev_name_count = sizeof(dev_name_list)/sizeof(dev_name_list[0]);

/*
 * Indirect list.
 */
struct dev_indirect dev_indirect_list[] = {

	/* console */
	{ "console",	&dev_name_list[0],	0 }
};
int	dev_indirect_count = sizeof(dev_indirect_list)
				/ sizeof(dev_indirect_list[0]);
