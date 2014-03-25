/* 
 * Copyright (c) 1988-1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	Utah $Hdr: cons.h 1.10 94/12/14$
 */

#ifndef _DEVICE_CONS_H
#define _DEVICE_CONS_H
#include <sys/types.h>

struct consdev {
	char	*cn_name;	/* name of device in dev_name_list */
	int	(*cn_probe)(struct consdev *cp);	/* probe hardware and fill in consdev info */
	int	(*cn_init)(struct consdev *cp);		/* turn on as console */
	int	(*cn_getc)(dev_t dev, int wait);	/* kernel getchar interface */
	int	(*cn_putc)(dev_t dev, int c);		/* kernel putchar interface */
	dev_t	cn_dev;		/* major/minor of device */
	short	cn_pri;		/* pecking order; the higher the better */
};

/* values for cn_pri - reflect our policy for console selection */
#define	CN_DEAD		0	/* device doesn't exist */
#define CN_NORMAL	1	/* device exists but is nothing special */
#define CN_INTERNAL	2	/* "internal" bit-mapped display */
#define CN_REMOTE	3	/* serial interface with remote bit set */

#define CONSBUFSIZE	1024

#ifdef KERNEL
extern	struct consdev constab[];
#endif

extern void cninit(void);

extern int cngetc(void);

extern int cnmaygetc(void);

extern void cnputc(char);

/*
 * ROM getc/putc primitives.
 * On some architectures, the boot ROM provides basic character input/output
 * routines that can be used before devices are configured or virtual memory
 * is enabled.  This can be useful to debug (or catch panics from) code early
 * in the bootstrap procedure.
 */
extern int	(*romgetc)(char c);
extern void	(*romputc)(char c);

#endif /* _DEVICE_CONS_H */
