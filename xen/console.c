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
#include <device/tty.h>
#include <device/cons.h>
#include <machine/pmap.h>
#include <machine/machspl.h>
#include <xen/public/io/console.h>
#include "console.h"
#include "ring.h"
#include "evt.h"

/* Hypervisor part */

decl_simple_lock_data(static, outlock);
decl_simple_lock_data(static, inlock);
static struct xencons_interface *console;
static int kd_pollc;
int kb_mode;	/* XXX: actually don't care.  */

#undef hyp_console_write
void hyp_console_write(const char *str, int len)
{
	hyp_console_io (CONSOLEIO_write, len, kvtolin(str));
}

int hypputc(int c)
{
	if (!console) {
		char d = c;
		hyp_console_io(CONSOLEIO_write, 1, kvtolin(&d));
	} else {
		spl_t spl = splhigh();
		simple_lock(&outlock);
		while (hyp_ring_smash(console->out, console->out_prod, console->out_cons)) {
			hyp_console_put("ring smash\n");
			/* TODO: are we allowed to sleep in putc? */
			hyp_yield();
		}
		hyp_ring_cell(console->out, console->out_prod) = c;
		wmb();
		console->out_prod++;
		hyp_event_channel_send(boot_info.console_evtchn);
		simple_unlock(&outlock);
		splx(spl);
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
	simple_lock(&inlock);
	while (console->in_prod != console->in_cons) {
		int c = hyp_ring_cell(console->in, console->in_cons);
		mb();
		console->in_cons++;
#if	MACH_KDB
		if (c == (char)'£') {
			printf("£ pressed\n");
			kdb_kintr();
			continue;
		}
#endif	/* MACH_KDB */
		if ((tp->t_state & (TS_ISOPEN|TS_WOPEN)))
			(*linesw[tp->t_line].l_rint)(c, tp);
	}
	hyp_event_channel_send(boot_info.console_evtchn);
	simple_unlock(&inlock);
}

int hypcnread(int dev, io_req_t ior)
{
	struct tty *tp = &hypcn_tty;
	tp->t_state |= TS_CARR_ON;
	return char_read(tp, ior);
}

int hypcnwrite(int dev, io_req_t ior)
{
	return char_write(&hypcn_tty, ior);
}

void hypcnstart(struct tty *tp)
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

void hypcnstop()
{
}

io_return_t hypcngetstat(dev_t dev, int flavor, int *data, unsigned int *count)
{
	return tty_get_status(&hypcn_tty, flavor, data, count);
}

io_return_t hypcnsetstat(dev_t dev, int flavor, int *data, unsigned int count)
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

	o_pri = spltty();
	simple_lock(&tp->t_lock);
	if (!(tp->t_state & (TS_ISOPEN|TS_WOPEN))) {
		/* XXX ttychars allocates memory */
		simple_unlock(&tp->t_lock);
		ttychars(tp);
		simple_lock(&tp->t_lock);
		tp->t_oproc = hypcnstart;
		tp->t_stop = hypcnstop;
		tp->t_ospeed = tp->t_ispeed = B9600;
		tp->t_flags = ODDP|EVENP|ECHO|CRMOD|XTABS;
	}
	tp->t_state |= TS_CARR_ON;
	simple_unlock(&tp->t_lock);
	splx(o_pri);
	return (char_open(dev, tp, flag, ior));
}

int hypcnclose(int dev, int flag)
{
	struct tty	*tp = &hypcn_tty;
	spl_t s = spltty();
	simple_lock(&tp->t_lock);
	ttyclose(tp);
	simple_unlock(&tp->t_lock);
	splx(s);
	return 0;
}

int hypcnprobe(struct consdev *cp)
{
	struct xencons_interface *my_console;
	my_console = (void*) mfn_to_kv(boot_info.console_mfn);

	cp->cn_dev = makedev(0, 0);
	cp->cn_pri = CN_INTERNAL;
	return 0;
}

int hypcninit(struct consdev *cp)
{
	if (console)
		return 0;
	simple_lock_init(&outlock);
	simple_lock_init(&inlock);
	console = (void*) mfn_to_kv(boot_info.console_mfn);
	pmap_set_page_readwrite(console);
	hyp_evt_handler(boot_info.console_evtchn, hypcnintr, 0, SPL6);
	return 0;
}
