/*
 * QLogic ISP1020 Intelligent SCSI Processor Driver (PCI)
 * Written by Erik H. Moe, ehm@cris.com
 * Copyright 1995, Erik H. Moe
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

/* Renamed and updated to 1.3.x by Michael Griffith <grif@cs.ucr.edu> */

/*
 * $Date: 2005/06/02 18:52:38 $
 * $Revision: 1.1.4.1 $
 *
 * $Log: qlogicisp.h,v $
 * Revision 1.1  1999/04/26 05:54:53  tb
 * 1998-11-30  OKUJI Yoshinori  <okuji@kuicr.kyoto-u.ac.jp>
 *
 * 	Clean up linux emulation code to make it architecture-independent
 * 	as much as possible.
 *
 * 	* linux: Renamed from linuxdev.
 * 	* Makefile.in (objfiles): Add linux.o instead of linuxdev.o.
 * 	(MAKE): New variable. Used for the linux.o target.
 * 	* configure.in: Add AC_CHECK_TOOL(MAKE, make).
 * 	* i386/i386/spl.h: Include <i386/ipl.h>, for compatibility with
 * 	OSF Mach 3.0. Suggested by Elgin Lee <ehl@funghi.com>.
 * 	* linux/src: Renamed from linux/linux.
 * 	* linux/dev: Renamed from linux/mach.
 * 	* linux/Drivers.in (AC_INIT): Use dev/include/linux/autoconf.h,
 * 	instead of mach/include/linux/autoconf.h.
 * 	* Makefile.in (all): Target ../linux.o instead of ../linuxdev.o.
 * 	* linux/dev/drivers/block/genhd.c: Include <machine/spl.h> instead
 * 	of <i386/ipl.h>.
 * 	* linux/dev/drivers/net/auto_irq.c: Remove unneeded header files,
 * 	<i386/ipl.h> and <i386/pic.h>.
 * 	* linux/dev/init/main.c: Many i386-dependent codes moved to ...
 * 	* linux/dev/arch/i386/irq.c: ... here.
 * 	* linux/dev/arch/i386/setup.c: New file.
 * 	* linux/dev/arch/i386/linux_emul.h: Likewise.
 * 	* linux/dev/arch/i386/glue/timer.c: Merged into sched.c.
 * 	* linux/dev/arch/i386/glue/sched.c: Include <machine/spl.h> instead
 * 	of <i386/ipl.h>, and moved to ...
 * 	* linux/dev/kernel/sched.c: ... here.
 * 	* linux/dev/arch/i386/glue/block.c: Include <machine/spl.h> and
 * 	<linux_emul.h>, instead of i386-dependent header files, and
 * 	moved to ...
 * 	* linux/dev/glue/blocl.c: ... here.
 * 	* linux/dev/arch/i386/glue/net.c: Include <machine/spl.h> and
 * 	<linux_emul.h>, instead of i386-dependent header files, and
 * 	moved to ...
 * 	* linux/dev/glue/net.c: ... here.
 * 	* linux/dev/arch/i386/glue/misc.c: Remove `x86' and moved to ...
 * 	* linux/dev/glue/misc.c: ... here.
 * 	* linux/dev/arch/i386/glue/kmem.c: Moved to ...
 * 	* linux/dev/glue/kmem.c: ... here.
 *
 * Revision 0.5  1995/09/22  02:32:56  root
 * do auto request sense
 *
 * Revision 0.4  1995/08/07  04:48:28  root
 * supply firmware with driver.
 * numerous bug fixes/general cleanup of code.
 *
 * Revision 0.3  1995/07/16  16:17:16  root
 * added reset/abort code.
 *
 * Revision 0.2  1995/06/29  03:19:43  root
 * fixed biosparam.
 * added queue protocol.
 *
 * Revision 0.1  1995/06/25  01:56:13  root
 * Initial release.
 *
 */

#ifndef _QLOGICISP_H
#define _QLOGICISP_H

/*
 * With the qlogic interface, every queue slot can hold a SCSI
 * command with up to 4 scatter/gather entries.  If we need more
 * than 4 entries, continuation entries can be used that hold
 * another 7 entries each.  Unlike for other drivers, this means
 * that the maximum number of scatter/gather entries we can
 * support at any given time is a function of the number of queue
 * slots available.  That is, host->can_queue and host->sg_tablesize
 * are dynamic and _not_ independent.  This all works fine because
 * requests are queued serially and the scatter/gather limit is
 * determined for each queue request anew.
 */
#define QLOGICISP_REQ_QUEUE_LEN	63	/* must be power of two - 1 */
#define QLOGICISP_MAX_SG(ql)	(4 + ((ql) > 0) ? 7*((ql) - 1) : 0)

int isp1020_detect(Scsi_Host_Template *);
int isp1020_release(struct Scsi_Host *);
const char * isp1020_info(struct Scsi_Host *);
int isp1020_queuecommand(Scsi_Cmnd *, void (* done)(Scsi_Cmnd *));
int isp1020_abort(Scsi_Cmnd *);
int isp1020_reset(Scsi_Cmnd *, unsigned int);
int isp1020_biosparam(Disk *, kdev_t, int[]);

#ifndef NULL
#define NULL (0)
#endif

static struct proc_dir_entry proc_scsi_isp1020;

#define QLOGICISP {							   \
	/* next */		NULL,					   \
	/* usage_count */	NULL,					   \
	/* proc dir */		NULL,					   \
	/* procfs info */	NULL,					   \
	/* name */		NULL,					   \
	/* detect */		isp1020_detect,				   \
	/* release */		isp1020_release,			   \
	/* info */		isp1020_info,				   \
	/* command */		NULL,					   \
	/* queuecommand */	isp1020_queuecommand,			   \
	/* abort */		isp1020_abort,				   \
	/* reset */		isp1020_reset,				   \
	/* slave_attach */	NULL,					   \
	/* bios_param */	isp1020_biosparam,			   \
	/* can_queue */		QLOGICISP_REQ_QUEUE_LEN,		   \
	/* this_id */		-1,					   \
	/* sg_tablesize */	QLOGICISP_MAX_SG(QLOGICISP_REQ_QUEUE_LEN), \
	/* cmd_per_lun */	1,					   \
	/* present */		0,					   \
	/* unchecked_isa_dma */	0,					   \
	/* use_clustering */	DISABLE_CLUSTERING			   \
}

#endif /* _QLOGICISP_H */
