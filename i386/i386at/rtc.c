/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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

#include <sys/types.h>
#include <sys/time.h>
#include <kern/mach_clock.h>
#include <i386/machspl.h>
#include <i386/pio.h>
#include <i386at/rtc.h>

/* time of day stored in RTC are currently between 1970 and 2070. Update that
 * before 2070 please. */
#define CENTURY_START	1970

static boolean_t first_rtcopen_ever = TRUE;

void
rtcinit(void)
{
	outb(RTC_ADDR, RTC_A);
	outb(RTC_DATA, RTC_DIV2 | RTC_RATE6);
	outb(RTC_ADDR, RTC_B);
	outb(RTC_DATA, RTC_HM);
}


int
rtcget(struct rtc_st *st)
{
	unsigned char *regs = (unsigned char *)st;
	if (first_rtcopen_ever) {
		rtcinit();
		first_rtcopen_ever = FALSE;
	}
	outb(RTC_ADDR, RTC_D);
	if ((inb(RTC_DATA) & RTC_VRT) == 0) return(-1);
	outb(RTC_ADDR, RTC_A);
	while (inb(RTC_DATA) & RTC_UIP)		/* busy wait */
		outb(RTC_ADDR, RTC_A);
	load_rtc(regs);
	return(0);
}

void
rtcput(struct rtc_st *st)
{
	unsigned char *regs = (unsigned char *)st;
	unsigned char	x;

	if (first_rtcopen_ever) {
		rtcinit();
		first_rtcopen_ever = FALSE;
	}
	outb(RTC_ADDR, RTC_B);
	x = inb(RTC_DATA);
	outb(RTC_ADDR, RTC_B);
	outb(RTC_DATA, x | RTC_SET);
	save_rtc(regs);
	outb(RTC_ADDR, RTC_B);
	outb(RTC_DATA, x & ~RTC_SET);
}


extern struct timeval time;

static int month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int
yeartoday(int year)
{
	if (year%4)
		/* Not divisible by 4, not bissextile */
		return 365;

	/* Divisible by 4 */
	if (year % 100)
		/* Not divisible by 100, bissextile */
		return 366;

	/* Divisible by 100 */
	if (year % 400)
		/* Not divisible by 400, not bissextile */
		return 365;

	/* Divisible by 400 */
	/* Rules for 2000 and further have not been officially decided yet.
	 * 2000 was made bissextile.  */
	return 366;
}

int
hexdectodec(char n)
{
	return(((n>>4)&0x0F)*10 + (n&0x0F));
}

char
dectohexdec(int n)
{
	return((char)(((n/10)<<4)&0xF0) | ((n%10)&0x0F));
}

int
readtodc(u_int *tp)
{
	struct rtc_st rtclk;
	time_t n;
	int sec, min, hr, dom, mon, yr;
	int i, days = 0;
	spl_t	ospl;

	ospl = splclock();
	if (rtcget(&rtclk)) {
		splx(ospl);
		return(-1);
	}
	splx (ospl);

	sec = hexdectodec(rtclk.rtc_sec);
	min = hexdectodec(rtclk.rtc_min);
	hr = hexdectodec(rtclk.rtc_hr);
	dom = hexdectodec(rtclk.rtc_dom);
	mon = hexdectodec(rtclk.rtc_mon);
	yr = hexdectodec(rtclk.rtc_yr);
	yr = (yr < CENTURY_START%100) ?
		yr+CENTURY_START-CENTURY_START%100+100 :
		yr+CENTURY_START-CENTURY_START%100;

	n = sec + 60 * min + 3600 * hr;
	n += (dom - 1) * 3600 * 24;

	if (yeartoday(yr) == 366)
		month[1] = 29;
	for (i = mon - 2; i >= 0; i--)
		days += month[i];
	month[1] = 28;
	/* Epoch shall be 1970 January 1st */
	for (i = 1970; i < yr; i++)
		days += yeartoday(i);
	n += days * 3600 * 24;


	*tp = n;

	return(0);
}

int
writetodc(void)
{
	struct rtc_st rtclk;
	time_t n;
	int diff, i, j;
	spl_t	ospl;

	ospl = splclock();
	if (rtcget(&rtclk)) {
		splx(ospl);
		return(-1);
	}
	splx(ospl);

	diff = 0;
	n = (time.tv_sec - diff) % (3600 * 24);   /* hrs+mins+secs */
	rtclk.rtc_sec = dectohexdec(n%60);
	n /= 60;
	rtclk.rtc_min = dectohexdec(n%60);
	rtclk.rtc_hr = dectohexdec(n/60);

	n = (time.tv_sec - diff) / (3600 * 24);	/* days */
	rtclk.rtc_dow = (n + 4) % 7;  /* 1/1/70 is Thursday */

	/* Epoch shall be 1970 January 1st */
	for (j = 1970, i = yeartoday(j); n >= i; j++, i = yeartoday(j))
		n -= i;

	rtclk.rtc_yr = dectohexdec(j - 1900);

	if (i == 366)
		month[1] = 29;
	for (i = 0; n >= month[i]; i++)
		n -= month[i];
	month[1] = 28;
	rtclk.rtc_mon = dectohexdec(++i);

	rtclk.rtc_dom = dectohexdec(++n);

	ospl = splclock();
	rtcput(&rtclk);
	splx(ospl);

	return(0);
}
