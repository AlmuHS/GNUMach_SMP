/***********************************************************************
 *	FILE NAME : DC390W.H					       *
 *	     BY   : C.L. Huang					       *
 *	Description: Device Driver for Tekram DC-390W/U/F (T) PCI SCSI *
 *		     Bus Master Host Adapter			       *
 ***********************************************************************/

/* Kernel version autodetection */

#include <linux/version.h>
/* Convert Linux Version, Patch-level, Sub-level to LINUX_VERSION_CODE. */
#define ASC_LINUX_VERSION(V, P, S)	(((V) * 65536) + ((P) * 256) + (S))

#if LINUX_VERSION_CODE < ASC_LINUX_VERSION(1,3,50)
#define VERSION_ELF_1_2_13
#elseif LINUX_VERSION_CODE < ASC_LINUX_VERSION(1,3,95)
#define VERSION_1_3_85
#else
#define VERSION_2_0_0
#endif

/*
 * NCR 53c825A,875 driver, header file
 */

#ifndef DC390W_H
#define DC390W_H

#if defined(HOSTS_C) || defined(MODULE)

#ifdef VERSION_2_0_0
#include <scsi/scsicam.h>
#else
#include <linux/scsicam.h>
#endif

extern int DC390W_detect(Scsi_Host_Template *psht);
extern int DC390W_queue_command(Scsi_Cmnd *cmd, void (*done)(Scsi_Cmnd *));
extern int DC390W_abort(Scsi_Cmnd *cmd);

#ifdef	VERSION_2_0_0
extern int DC390W_reset(Scsi_Cmnd *cmd, unsigned int resetFlags);
#else
extern int DC390W_reset(Scsi_Cmnd *cmd);
#endif

#ifdef	VERSION_ELF_1_2_13
extern int DC390W_bios_param(Disk *disk, int devno, int geom[]);
#else
extern int DC390W_bios_param(Disk *disk, kdev_t devno, int geom[]);
#endif

#ifdef MODULE
static int DC390W_release(struct Scsi_Host *);
#else
#define DC390W_release NULL
#endif

#ifndef VERSION_ELF_1_2_13
extern struct proc_dir_entry proc_scsi_tmscsiw;
extern int tmscsiw_proc_info(char*, char**, off_t, int, int, int);
#endif

#ifdef	VERSION_2_0_0

#define DC390WUF   {			\
	NULL,	/* *next */		\
	NULL,	/* *usage_count */	\
	&proc_scsi_tmscsiw,	/* *proc_dir */ 	\
	tmscsiw_proc_info,	/* (*proc_info)() */	\
	"Tekram DC390WUF(T) V1.12 Feb-17-1997",  /* *name */ \
	DC390W_detect,			\
	DC390W_release, /* (*release)() */	\
	NULL,	/* *(*info)() */	\
	NULL,	/* (*command)() */	\
	DC390W_queue_command,	\
	DC390W_abort,		\
	DC390W_reset,		\
	NULL, /* slave attach */\
	DC390W_bios_param,	\
	10,/* can queue(-1) */	\
	7, /* id(-1) */ 	\
	32,/* old (SG_ALL) */	\
	2, /* cmd per lun(2) */ \
	0, /* present */	\
	0, /* unchecked isa dma */ \
	ENABLE_CLUSTERING	\
	}
#endif

#ifdef	VERSION_1_3_85

#define DC390WUF   {			\
	NULL,	/* *next */		\
	NULL,	/* *usage_count */	\
	&proc_scsi_tmscsiw,	/* *proc_dir */ 	\
	tmscsiw_proc_info,	/* (*proc_info)() */	\
	"Tekram DC390WUF(T) V1.12 Feb-17-1997",  /* *name */ \
	DC390W_detect,			\
	DC390W_release, /* (*release)() */	\
	NULL,	/* *(*info)() */	\
	NULL,	/* (*command)() */	\
	DC390W_queue_command,	\
	DC390W_abort,		\
	DC390W_reset,		\
	NULL, /* slave attach */\
	DC390W_bios_param,	\
	10,/* can queue(-1) */	\
	7, /* id(-1) */ 	\
	16,/* old (SG_ALL) */	\
	2, /* cmd per lun(2) */ \
	0, /* present */	\
	0, /* unchecked isa dma */ \
	ENABLE_CLUSTERING	\
	}
#endif

#ifdef	VERSION_ELF_1_2_13

#define DC390WUF   {		\
	NULL,			\
	NULL,			\
	"Tekram DC390WUF(T) V1.12 Feb-17-1997",\
	DC390W_detect,		\
	DC390W_release, 		\
	NULL, /* info */	\
	NULL, /* command, deprecated */ \
	DC390W_queue_command,	\
	DC390W_abort,		\
	DC390W_reset,		\
	NULL, /* slave attach */\
	DC390W_bios_param,	\
	10,/* can queue(-1) */	\
	7, /* id(-1) */ 	\
	16,/* old (SG_ALL) */	\
	2, /* cmd per lun(2) */ \
	0, /* present */	\
	0, /* unchecked isa dma */ \
	ENABLE_CLUSTERING	\
	}
#endif

#endif /* defined(HOSTS_C) || defined(MODULE) */

#endif /* DC390W_H */
