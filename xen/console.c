/*
 *  Copyright (C) 2006-2011 Free Software Foundation
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

#include <i386at/kd.h>
#include <sys/types.h>
#include <device/tty.h>
#include <device/cons.h>
#include <machine/pmap.h>
#include <machine/spl.h>
#include <xen/public/io/console.h>
#include "console.h"
#include "ring.h"
#include "evt.h"

/* Hypervisor part */

def_simple_lock_irq_data(static, outlock);
def_simple_lock_irq_data(static, inlock);
static struct xencons_interface *console;
static int kd_pollc;
int kb_mode;	/* XXX: actually don't care.  */

static int hypputc(int c)
{
	if (!console) {
		char d = c;
		hyp_console_io(CONSOLEIO_write, 1, kvtolin(&d));
	} else {
		spl_t spl;
		static int complain;
		spl = simple_lock_irq(&outlock);
		while (hyp_ring_smash(console->out, console->out_prod, console->out_cons)) {
			if (!complain) {
				complain = 1;
				hyp_console_put("ring smash\n");
			}
			/* TODO: are we allowed to sleep in putc? */
			hyp_yield();
		}
		hyp_ring_cell(console->out, console->out_prod) = c;
		wmb();
		console->out_prod++;
		hyp_event_channel_send(boot_info.console_evtchn);
		simple_unlock_irq(spl, &outlock);
	}
	return 0;
}

int hypcnputc(dev_t dev, int c)
{
	return hypputc(c);
}

/* get char by polling, used by debugger */
int hypcngetc(dev_t dev, int wait)
{
	int ret;
	if (wait)
		while (console->in_prod == console->in_cons)
			hyp_yield();
	else
		if (console->in_prod == console->in_cons)
			return -1;
	ret = hyp_ring_cell(console->in, console->in_cons);
	mb();
	console->in_cons++;
	hyp_event_channel_send(boot_info.console_evtchn);
	return ret;
}

void cnpollc(boolean_t on) {
	if (on) {
		kd_pollc++;
	} else {
		--kd_pollc;
	}
}

void kd_setleds1(u_char val)
{
	/* Can't do this.  */
}

/* Mach part */

struct tty hypcn_tty;

static void hypcnintr(int unit, spl_t spl, void *ret_addr, void *regs) {
	struct tty *tp = &hypcn_tty;
	if (kd_pollc)
		return;
	simple_lock_nocheck(&inlock.slock);
	while (console->in_prod != console->in_cons) {
		int c = hyp_ring_cell(console->in, console->in_cons);
		mb();
		console->in_cons++;
#if	MACH_KDB
		if (c == (char)0xA3) {
			printf("pound pressed\n");
			kdb_kintr();
			continue;
		}
#endif	/* MACH_KDB */
		if ((tp->t_state & (TS_ISOPEN|TS_WOPEN)))
			(*linesw[tp->t_line].l_rint)(c, tp);
	}
	hyp_event_channel_send(boot_info.console_evtchn);
	simple_unlock_nocheck(&inlock.slock);
}

int hypcnread(dev_t dev, io_req_t ior)
{
	struct tty *tp = &hypcn_tty;
	tp->t_state |= TS_CARR_ON;
	return char_read(tp, ior);
}

int hypcnwrite(dev_t dev, io_req_t ior)
{
	return char_write(&hypcn_tty, ior);
}

static void hypcnstart(struct tty *tp)
{
	spl_t	o_pri;
	int ch;
	unsigned char c;

	if (tp->t_state & TS_TTSTOP)
		return;
	while (1) {
		tp->t_state &= ~TS_BUSY;
		if (tp->t_state & TS_TTSTOP)
			break;
		if ((tp->t_outq.c_cc <= 0) || (ch = getc(&tp->t_outq)) == -1)
			break;
		c = ch;
		o_pri = splsoftclock();
		hypputc(c);
		splx(o_pri);
	}
	if (tp->t_outq.c_cc <= TTLOWAT(tp)) {
		tt_write_wakeup(tp);
	}
}

static void hypcnstop(struct tty *t, int n)
{
}

io_return_t hypcngetstat(dev_t dev, dev_flavor_t flavor, dev_status_t data, mach_msg_type_number_t *count)
{
	return tty_get_status(&hypcn_tty, flavor, data, count);
}

io_return_t hypcnsetstat(dev_t dev, dev_flavor_t flavor, dev_status_t data, mach_msg_type_number_t count)
{
	return tty_set_status(&hypcn_tty, flavor, data, count);
}

int hypcnportdeath(dev_t dev, mach_port_t port)
{
	return tty_portdeath(&hypcn_tty, (ipc_port_t) port);
}

int hypcnopen(dev_t dev, int flag, io_req_t ior)
{
	struct tty 	*tp = &hypcn_tty;
	spl_t	o_pri;

	o_pri = simple_lock_irq(&tp->t_lock);
	if (!(tp->t_state & (TS_ISOPEN|TS_WOPEN))) {
		/* XXX ttychars allocates memory */
		simple_unlock_nocheck(&tp->t_lock.slock);
		ttychars(tp);
		simple_lock_nocheck(&tp->t_lock.slock);
		tp->t_oproc = hypcnstart;
		tp->t_stop = hypcnstop;
		tp->t_ospeed = tp->t_ispeed = B115200;
		tp->t_flags = ODDP|EVENP|ECHO|CRMOD|XTABS|LITOUT;
	}
	tp->t_state |= TS_CARR_ON;
	simple_unlock_irq(o_pri, &tp->t_lock);
	return (char_open(dev, tp, flag, ior));
}

void hypcnclose(dev_t dev, int flag)
{
	struct tty	*tp = &hypcn_tty;
	spl_t s;
	s = simple_lock_irq(&tp->t_lock);
	ttyclose(tp);
	simple_unlock_irq(s, &tp->t_lock);
}

int hypcnprobe(struct consdev *cp)
{
	cp->cn_dev = makedev(0, 0);
	cp->cn_pri = CN_INTERNAL;
	return 0;
}

int hypcninit(struct consdev *cp)
{
	if (console)
		return 0;
	simple_lock_init_irq(&outlock);
	simple_lock_init_irq(&inlock);
	console = (void*) mfn_to_kv(boot_info.console_mfn);
#ifdef	MACH_PV_PAGETABLES
	pmap_set_page_readwrite(console);
#endif	/* MACH_PV_PAGETABLES */
	hyp_evt_handler(boot_info.console_evtchn, (interrupt_handler_fn)hypcnintr, 0, SPL6);
	return 0;
}
