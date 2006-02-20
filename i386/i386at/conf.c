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

extern vm_offset_t block_io_mmap();

extern int	timeopen(), timeclose();
extern vm_offset_t timemmap();
#define	timename		"time"

#include <par.h>
#if	NPAR > 0
extern int	paropen(), paroutput(), pargetstat(), parsetstat(),
		parsetinput();
#define	parname		"par"
#endif /* NPAR > 0 */

#include <de6c.h>
#if	NDE6C > 0
extern int	de6copen(), de6coutput(), de6cgetstat(), de6csetstat(),
		de6csetinput();
#define	de6cname		"de"
#endif /* NDE6C > 0 */

extern int	kdopen(), kdclose(), kdread(), kdwrite();
extern int	kdgetstat(), kdsetstat(), kdportdeath();
extern vm_offset_t kdmmap();
#define	kdname			"kd"

#include <com.h>
#if	NCOM > 0
extern int	comopen(), comclose(), comread(), comwrite();
extern int	comgetstat(), comsetstat(), comportdeath();
#define	comname			"com"
#endif	/* NCOM > 0 */

#include <lpr.h>
#if	NLPR > 0
extern int	lpropen(), lprclose(), lprread(), lprwrite();
extern int	lprgetstat(), lprsetstat(), lprportdeath();
#define	lprname			"lpr"
#endif	/* NLPR > 0 */

#include <blit.h>
#if NBLIT > 0
extern int	blitopen(), blitclose(), blit_get_stat();
extern vm_offset_t blitmmap();
#define	blitname		"blit"

extern int	mouseinit(), mouseopen(), mouseclose();
extern int	mouseioctl(), mouseselect(), mouseread();
#endif

extern int	kbdopen(), kbdclose(), kbdread();
extern int	kbdgetstat(), kbdsetstat();
#define	kbdname			"kbd"

extern int	mouseopen(), mouseclose(), mouseread(), mousegetstat();
#define	mousename		"mouse"

extern int	ioplopen(), ioplclose();
extern vm_offset_t ioplmmap();
#define	ioplname		"iopl"

extern int	kmsgopen(), kmsgclose(), kmsgread(), kmsggetstat();
#define kmsgname		"kmsg"

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
	  nulldev,	nulldev,	nulldev,	nulldev,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

	{ kdname,	kdopen,		kdclose,	kdread,
	  kdwrite,	kdgetstat,	kdsetstat,	kdmmap,
	  nodev,	nulldev,	kdportdeath,	0,
	  nodev },

	{ timename,	timeopen,	timeclose,	nulldev,
	  nulldev,	nulldev,	nulldev,	timemmap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

#ifndef LINUX_DEV
#if	NPAR > 0
	{ parname,	paropen,	nulldev,	nulldev,
	  paroutput,	pargetstat,	parsetstat,	nomap,
	  parsetinput,	nulldev,	nulldev, 	0,
	  nodev },
#endif

#if	NDE6C > 0
	{ de6cname,	de6copen,	nulldev,	nulldev,
	  de6coutput,	de6cgetstat,	de6csetstat,	nomap,
	  de6csetinput,	nulldev,	nulldev, 	0,
	  nodev },
#endif
#endif /* ! LINUX_DEV */

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

#if	NBLIT > 0
	{ blitname,	blitopen,	blitclose,	nodev,
	  nodev,	blit_get_stat,	nodev,		blitmmap,
	  nodev,	nodev,		nodev,		0,
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

	{ ioplname,	ioplopen,	ioplclose,	nodev,
	  nodev,	nodev,		nodev,		ioplmmap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

#ifdef	MACH_KMSG
        { kmsgname,     kmsgopen,       kmsgclose,       kmsgread,
          nodev,        kmsggetstat,    nodev,           nomap,
          nodev,        nulldev,        nulldev,         0,
          nodev },
#endif

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
