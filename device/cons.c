/*
 * Copyright (c) 1988-1994, The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
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
 *      Utah $Hdr: cons.c 1.14 94/12/14$
 */

#include <string.h>
#include <kern/debug.h>
#include <sys/types.h>
#include <device/conf.h>
#include <mach/boolean.h>
#include <device/cons.h>

#ifdef MACH_KMSG
#include <device/io_req.h>
#include <device/kmsg.h>
#endif /* MACH_KMSG */

static	boolean_t cn_inited = FALSE;
static	struct consdev *cn_tab = 0;	/* physical console device info */

/*
 * ROM getc/putc primitives.
 * On some architectures, the boot ROM provides basic character input/output
 * routines that can be used before devices are configured or virtual memory
 * is enabled.  This can be useful to debug (or catch panics from) code early
 * in the bootstrap procedure.
 */
int	(*romgetc)(char c) = 0;
void	(*romputc)(char c) = 0;

#if CONSBUFSIZE > 0
/*
 * Temporary buffer to store console output before a console is selected.
 * This is statically allocated so it can be called before malloc/kmem_alloc
 * have been initialized.  It is initialized so it won't be clobbered as
 * part of the zeroing of BSS (on PA/Mach).
 */
static	char consbuf[CONSBUFSIZE] = { 0 };
static	char *consbp = consbuf;
static	boolean_t consbufused = FALSE;
#endif /* CONSBUFSIZE > 0 */

void
cninit(void)
{
	struct consdev *cp;
	dev_ops_t cn_ops;
	int x;

	if (cn_inited)
		return;

	/*
	 * Collect information about all possible consoles
	 * and find the one with highest priority
	 */
	for (cp = constab; cp->cn_probe; cp++) {
		(*cp->cn_probe)(cp);
		if (cp->cn_pri > CN_DEAD &&
		    (cn_tab == NULL || cp->cn_pri > cn_tab->cn_pri))
			cn_tab = cp;
	}
	
	/*
	 * Found a console, initialize it.
	 */
	if ((cp = cn_tab)) { 
		/*
		 * Initialize as console
		 */
		(*cp->cn_init)(cp);
		/*
		 * Look up its dev_ops pointer in the device table and
		 * place it in the device indirection table.
		 */
		if (dev_name_lookup(cp->cn_name, &cn_ops, &x) == FALSE)
			panic("cninit: dev_name_lookup failed");
		dev_set_indirection("console", cn_ops, minor(cp->cn_dev));
#if CONSBUFSIZE > 0
		/*
		 * Now that the console is initialized, dump any chars in
		 * the temporary console buffer.
		 */
		if (consbufused) {
			char *cbp = consbp;
			do {
				if (*cbp)
					cnputc(*cbp);
				if (++cbp == &consbuf[CONSBUFSIZE])
					cbp = consbuf;
			} while (cbp != consbp);
			consbufused = FALSE;
		}
#endif /* CONSBUFSIZE > 0 */
		cn_inited = TRUE;
		return;
	}
	/*
	 * No console device found, not a problem for BSD, fatal for Mach
	 */
	panic("can't find a console device");
}


int
cngetc(void)
{
	if (cn_tab)
		return ((*cn_tab->cn_getc)(cn_tab->cn_dev, 1));
	if (romgetc)
		return ((*romgetc)(1));
	return (0);
}

int
cnmaygetc(void)
{
	if (cn_tab)
		return((*cn_tab->cn_getc)(cn_tab->cn_dev, 0));
	if (romgetc)
		return ((*romgetc)(0));
	return (0);
}

void
cnputc(c)
	char c;
{
	if (c == 0)
		return;

#ifdef MACH_KMSG
	/* XXX: Assume that All output routines always use cnputc. */
	kmsg_putchar (c);
#endif
	
#if defined(MACH_HYP) && 0
	{
		/* Also output on hypervisor's emergency console, for
		 * debugging */
		unsigned char d = c;
		hyp_console_write(&d, 1);
	}
#endif	/* MACH_HYP */
	
	if (cn_tab) {
		(*cn_tab->cn_putc)(cn_tab->cn_dev, c);
		if (c == '\n')
			(*cn_tab->cn_putc)(cn_tab->cn_dev, '\r');
	} else if (romputc) {
		(*romputc)(c);
		if (c == '\n')
			(*romputc)('\r');
	}
#if CONSBUFSIZE > 0
	else {
		if (consbufused == FALSE) {
			consbp = consbuf;
			consbufused = TRUE;
			memset(consbuf, 0, CONSBUFSIZE);
		}
		*consbp++ = c;
		if (consbp >= &consbuf[CONSBUFSIZE])
			consbp = consbuf;
	}
#endif /* CONSBUFSIZE > 0 */
}
