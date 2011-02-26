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
 *	Utah $Hdr: cons_conf.c 1.7 94/12/14$
 */

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console would be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
#include <sys/types.h>
#include <device/cons.h>

#ifdef	MACH_HYP
extern	int hypcnprobe(), hypcninit(), hypcngetc(), hypcnputc();
#else	/* MACH_HYP */
extern	int kdcnprobe(), kdcninit(), kdcngetc(), kdcnputc();
#if NCOM > 0
extern	int comcnprobe(), comcninit(), comcngetc(), comcnputc();
#endif
#endif	/* MACH_HYP */

/*
 * The rest of the consdev fields are filled in by the respective
 * cnprobe routine.
 */
struct	consdev constab[] = {
#ifdef	MACH_HYP
	{"hyp",	hypcnprobe,	hypcninit,	hypcngetc,	hypcnputc},
#else	/* MACH_HYP */
	{"kd",	kdcnprobe,	kdcninit,	kdcngetc,	kdcnputc},
#if NCOM > 0
	{"com",	comcnprobe,	comcninit,	comcngetc,	comcnputc},
#endif
#endif	/* MACH_HYP */
	{0}
};
