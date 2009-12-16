/*
 *  Copyright (C) 2006 Samuel Thibault <samuel.thibault@ens-lyon.org>
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>
#include <mach/mach_types.h>
#include <kern/mach_clock.h>
#include <mach/xen.h>
#include <machine/xen.h>
#include <machine/spl.h>
#include <machine/ipl.h>
#include <mach/machine/eflags.h>
#include <xen/evt.h>
#include "time.h"
#include "store.h"

static unsigned64_t lastnsec;

/* 2^64 nanoseconds ~= 500 years */
static unsigned64_t hyp_get_stime(void) {
	unsigned32_t version;
	unsigned64_t cpu_clock, last_cpu_clock, delta, system_time;
	unsigned32_t mul;
	signed8_t shift;
	volatile struct vcpu_time_info *time = &hyp_shared_info.vcpu_info[0].time;

	do {
		version		= time->version;
		rmb();
		cpu_clock	= hyp_cpu_clock();
		last_cpu_clock	= time->tsc_timestamp;
		system_time	= time->system_time;
		mul		= time->tsc_to_system_mul;
		shift		= time->tsc_shift;
		rmb();
	} while (version != time->version);

	delta = cpu_clock - last_cpu_clock;
	if (shift < 0)
		delta >>= -shift;
	else
		delta <<= shift;
	return system_time + ((delta * (unsigned64_t) mul) >> 32);
}

unsigned64_t hyp_get_time(void) {
	unsigned32_t version;
	unsigned32_t sec, nsec;

	do {
		version = hyp_shared_info.wc_version;
		rmb();
		sec = hyp_shared_info.wc_sec;
		nsec = hyp_shared_info.wc_nsec;
		rmb();
	} while (version != hyp_shared_info.wc_version);

	return sec*1000000000ULL + nsec + hyp_get_stime();
}

static void hypclock_intr(int unit, int old_ipl, void *ret_addr, struct i386_interrupt_state *regs) {
	unsigned64_t nsec, delta;

	if (!lastnsec)
		return;

	nsec = hyp_get_stime();
	if (nsec < lastnsec) {
		printf("warning: nsec 0x%08lx%08lx < lastnsec 0x%08lx%08lx\n",(unsigned long)(nsec>>32), (unsigned long)nsec, (unsigned long)(lastnsec>>32), (unsigned long)lastnsec);
		nsec = lastnsec;
	}
	delta = nsec-lastnsec;

	lastnsec += (delta/1000)*1000;
	hypclock_machine_intr(old_ipl, ret_addr, regs, delta);
	/* 10ms tick rest */
	hyp_do_set_timer_op(hyp_get_stime()+10*1000*1000);

#if 0
	char *c = hyp_store_read(0, 1, "control/shutdown");
	if (c) {
		static int go_down = 0;
		if (!go_down) {
			printf("uh oh, shutdown: %s\n", c);
			go_down = 1;
			/* TODO: somehow send startup_reboot notification to init */
			if (!strcmp(c, "reboot")) {
				/* this is just a reboot */
			}
		}
	}
#endif
}

extern struct timeval time;
extern struct timezone tz;

int
readtodc(tp)
	u_int	*tp;
{
	unsigned64_t t = hyp_get_time();
	u_int n = t / 1000000000;

#ifndef	MACH_KERNEL
	n += tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		n -= 3600;
#endif	/* MACH_KERNEL */
	*tp = n;

	return(0);
}

int
writetodc()
{
	/* Not allowed in Xen */
	return(-1);
}

void
clkstart()
{
	evtchn_port_t port = hyp_event_channel_bind_virq(VIRQ_TIMER, 0);
	hyp_evt_handler(port, hypclock_intr, 0, SPLHI);

	/* first clock tick */
	clock_interrupt(0, 0, 0);
	lastnsec = hyp_get_stime();

	/* 10ms tick rest */
	hyp_do_set_timer_op(hyp_get_stime()+10*1000*1000);
}
