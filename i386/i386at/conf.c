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

extern int	timeopen(), timeclose();
extern vm_offset_t timemmap();
#define	timename		"time"

#ifndef	MACH_HYP
extern int	kdopen(), kdclose(), kdread(), kdwrite();
extern int	kdgetstat(), kdsetstat(), kdportdeath();
extern vm_offset_t kdmmap();
#define	kdname			"kd"

#if	NCOM > 0
extern int	comopen(), comclose(), comread(), comwrite();
extern int	comgetstat(), comsetstat(), comportdeath();
#define	comname			"com"
#endif	/* NCOM > 0 */

#if	NLPR > 0
extern int	lpropen(), lprclose(), lprread(), lprwrite();
extern int	lprgetstat(), lprsetstat(), lprportdeath();
#define	lprname			"lpr"
#endif	/* NLPR > 0 */
#endif	/* MACH_HYP */

extern int	kbdopen(), kbdclose(), kbdread();
extern int	kbdgetstat(), kbdsetstat();
#define	kbdname			"kbd"

#ifndef	MACH_HYP
extern int	mouseopen(), mouseclose(), mouseread(), mousegetstat();
#define	mousename		"mouse"

extern vm_offset_t memmmap();
#define	memname			"mem"
#endif	/* MACH_HYP */

extern int	kmsgopen(), kmsgclose(), kmsgread(), kmsggetstat();
#define kmsgname		"kmsg"

#ifdef	MACH_HYP
extern int	hypcnopen(), hypcnclose(), hypcnread(), hypcnwrite();
extern int	hypcngetstat(), hypcnsetstat(), hypcnportdeath();
#define hypcnname		"hyp"
#endif	/* MACH_HYP */

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
	{ "cn",		nulldev,	nulldev,	nulldev,
	  nulldev,	nulldev,	nulldev,	nomap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

#ifndef	MACH_HYP
	{ kdname,	kdopen,		kdclose,	kdread,
	  kdwrite,	kdgetstat,	kdsetstat,	kdmmap,
	  nodev,	nulldev,	kdportdeath,	0,
	  nodev },
#endif	/* MACH_HYP */

	{ timename,	timeopen,	timeclose,	nulldev,
	  nulldev,	nulldev,	nulldev,	timemmap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

#ifndef	MACH_HYP
#if	NCOM > 0
	{ comname,	comopen,	comclose,	comread,
	  comwrite,	comgetstat,	comsetstat,	nomap,
	  nodev,	nulldev,	comportdeath,	0,
	  nodev },
#endif

#ifdef MACH_LPR
	{ lprname,	lpropen,	lprclose,	lprread,
	  lprwrite,	lprgetstat,	lprsetstat,	nomap,
	  nodev,	nulldev,	lprportdeath,	0,
	  nodev },
#endif

	{ mousename,	mouseopen,	mouseclose,	mouseread,
	  nodev,	mousegetstat,	nulldev,	nomap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

	{ kbdname,	kbdopen,	kbdclose,	kbdread,
	  nodev,	kbdgetstat,	kbdsetstat,	nomap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

	{ memname,	nulldev,	nulldev,	nodev,
	  nodev,	nodev,		nodev,		memmmap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },
#endif	/* MACH_HYP */

#ifdef	MACH_KMSG
        { kmsgname,     kmsgopen,       kmsgclose,       kmsgread,
          nodev,        kmsggetstat,    nodev,           nomap,
          nodev,        nulldev,        nulldev,         0,
          nodev },
#endif

#ifdef	MACH_HYP
	{ hypcnname,	hypcnopen,	hypcnclose,	hypcnread,
	  hypcnwrite,	hypcngetstat,	hypcnsetstat,	nomap,
	  nodev,	nulldev,	hypcnportdeath,	0,
	  nodev },
#endif	/* MACH_HYP */

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
