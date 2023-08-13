/*
 * Copyright (c) 1995-1994 The University of Utah and
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
 *      Author: Bryan Ford, University of Utah CSL
 */

#if	ENABLE_IMMEDIATE_CONSOLE

#include <device/cons.h>
#include <mach/boolean.h>
#include <i386/vm_param.h>
#include <string.h>

/* This is a special "feature" (read: kludge)
   intended for use only for kernel debugging.
   It enables an extremely simple console output mechanism
   that sends text straight to CGA/EGA/VGA video memory.
   It has the nice property of being functional right from the start,
   so it can be used to debug things that happen very early
   before any devices are initialized.  */

boolean_t immediate_console_enable = TRUE;

/*
 * XXX we assume that pcs *always* have a console
 */
int
immc_cnprobe(struct consdev *cp)
{
	int maj, unit, pri;

	maj = 0;
	unit = 0;
	pri = CN_INTERNAL;

	cp->cn_dev = makedev(maj, unit);
	cp->cn_pri = pri;
	return 0;
}

int
immc_cninit(struct consdev *cp)
{
	return 0;
}

int immc_cnmaygetc(void)
{
	return -1;
}

int
immc_cngetc(dev_t dev, int wait)
{
	if (wait) {
		int c;
		while ((c = immc_cnmaygetc()) < 0)
			continue;
		return c;
	}
	else
		return immc_cnmaygetc();
}

int
immc_cnputc(dev_t dev, int c)
{
	static int ofs = -1;

	if (!immediate_console_enable)
		return -1;
	if (ofs < 0 || ofs >= 80)
	{
		ofs = 0;
		immc_cnputc(dev, '\n');
	}

	if (c == '\n')
	{
		memmove((void *) phystokv(0xb8000),
			(void *) phystokv(0xb8000+80*2), 80*2*24);
		memset((void *) phystokv((0xb8000+80*2*24)), 0, 80*2);
		ofs = 0;
	}
	else if (c == '\r')
	{
		ofs = 0;
	}
	else if (c == '\t')
	{
		ofs = (ofs & ~7) + 8;
	}
	else
	{
		volatile unsigned char *p;

		if (ofs >= 80)
		{
			immc_cnputc(dev, '\r');
			immc_cnputc(dev, '\n');
		}

		p = (void *) phystokv(0xb8000 + 80*2*24 + ofs*2);
		p[0] = c;
		p[1] = 0x0f;
		ofs++;
	}
	return 0;
}

void
immc_romputc(char c)
{
	immc_cnputc (0, c);
}

#endif /* ENABLE_IMMEDIATE_CONSOLE */
