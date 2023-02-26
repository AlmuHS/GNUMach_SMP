/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity 
 * pertaining to distribution of the software without specific, written
 * prior permission.
 * 
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
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
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <kern/mach_clock.h>
#include <i386/ipl.h>
#include <machine/irq.h>
#include <i386/pit.h>
#include <i386/pio.h>
#include <kern/cpu_number.h>

int pitctl_port  = PITCTL_PORT;		/* For 386/20 Board */
int pitctr0_port = PITCTR0_PORT;	/* For 386/20 Board */
/* We want PIT 0 in square wave mode */

int pit0_mode = PIT_C0|PIT_SQUAREMODE|PIT_READMODE ;


unsigned int clknumb = CLKNUM;		/* interrupt interval for timer 0 */

void
pit_prepare_sleep(int persec)
{
    /* Prepare to sleep for 1/persec seconds */
    uint32_t val = 0;
    uint8_t lsb, msb;

    val = inb(PITAUX_PORT);
    val &= ~PITAUX_OUT2;
    val |= PITAUX_GATE2;
    outb (PITAUX_PORT, val);
    outb (PITCTL_PORT, PIT_C2 | PIT_LOADMODE | PIT_ONESHOTMODE);
    val = CLKNUM / persec;
    lsb = val & 0xff;
    msb = val >> 8;
    outb (PITCTR2_PORT, lsb);
    val = inb(POST_PORT); /* ~1us i/o delay */
    outb (PITCTR2_PORT, msb);
}

void
pit_sleep(void)
{
    uint8_t val;

    /* Start counting down */
    val = inb(PITAUX_PORT);
    val &= ~PITAUX_GATE2;
    outb (PITAUX_PORT, val); /* Gate low */
    val |= PITAUX_GATE2;
    outb (PITAUX_PORT, val); /* Gate high */

    /* Wait until counter reaches zero */
    while ((inb(PITAUX_PORT) & PITAUX_VAL) == 0);
}

void
pit_udelay(int usec)
{
    pit_prepare_sleep(1000000 / usec);
    pit_sleep();
}

void
pit_mdelay(int msec)
{
    pit_prepare_sleep(1000 / msec);
    pit_sleep();
}

void
clkstart(void)
{
	if (cpu_number() != 0)
		/* Only one PIT initialization is needed */
		return;
	unsigned char	byte;
	unsigned long s;

	s = sploff();         /* disable interrupts */

	/* Since we use only timer 0, we program that.
	 * 8254 Manual specifically says you do not need to program
	 * timers you do not use
	 */
	outb(pitctl_port, pit0_mode);
	clknumb = (CLKNUM + hz / 2) / hz;
	byte = clknumb;
	outb(pitctr0_port, byte);
	byte = clknumb>>8;
	outb(pitctr0_port, byte); 
	splon(s);         /* restore interrupt state */
}
