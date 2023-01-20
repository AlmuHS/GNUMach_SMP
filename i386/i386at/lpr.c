/* 
 * Mach Operating System
 * Copyright (c) 1993-1990 Carnegie Mellon University
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
 *	Parallel port printer driver v1.0
 *	All rights reserved.
 */ 
  
#if NLPR > 0
  
#include <mach/std_types.h>
#include <sys/types.h>
#include <kern/printf.h>
#include <kern/mach_clock.h>
#include <device/conf.h>
#include <device/device_types.h>
#include <device/tty.h>
#include <device/io_req.h>
  
#include <i386/ipl.h>
#include <i386/pio.h>
#include <chips/busses.h>
#include <i386at/autoconf.h>
#include <i386at/lpr.h>
  
/* 
 * Driver information for auto-configuration stuff.
 */

struct bus_device *lprinfo[NLPR];	/* ??? */

static vm_offset_t lpr_std[NLPR] = { 0 };
static struct bus_device *lpr_info[NLPR];
struct bus_driver lprdriver = {
	lprprobe, 0, lprattach, 0, lpr_std, "lpr", lpr_info, 0, 0, 0};

struct tty	lpr_tty[NLPR];

int lpr_alive[NLPR];

int
lprprobe(vm_offset_t port, struct bus_ctlr *dev)
{
	u_short	addr = (u_short) dev->address;
	int	unit = dev->unit;
	int ret;

	if ((unit < 0) || (unit >= NLPR)) {
		printf("com %d out of range\n", unit);
		return(0);
	}

	outb(INTR_ENAB(addr),0x07);
	outb(DATA(addr),0xaa);
	ret = inb(DATA(addr)) == 0xaa;
	if (ret) {
		if (lpr_alive[unit]) {
			printf("lpr: Multiple alive entries for unit %d.\n", unit);
			printf("lpr: Ignoring entry with address = %x .\n", addr);
			ret = 0;
		} else
			lpr_alive[unit]++;
	}
	return(ret);
}

void lprattach(struct bus_device *dev)
{
	u_char		unit = dev->unit;
	u_short		addr = (u_short) dev->address;

	if (unit >= NLPR) {
		printf(", disabled by NLPR configuration\n");
		return;
	}

	take_dev_irq(dev);
	printf(", port = %zx, spl = %zd, pic = %d.",
	       dev->address, dev->sysdep, dev->sysdep1);
	lprinfo[unit] = dev;
  
	outb(INTR_ENAB(addr), inb(INTR_ENAB(addr)) & 0x0f);

	return;
}

int
lpropen(dev_t dev, int flag, io_req_t ior)
{
	int unit = minor(dev);
	struct bus_device *isai;
	struct tty *tp;
	u_short addr;

	if (unit >= NLPR)
		return D_NO_SUCH_DEVICE;

	isai = lprinfo[unit];
	if (isai == NULL || !isai->alive)
		return D_NO_SUCH_DEVICE;

	tp = &lpr_tty[unit];
	addr = (u_short) isai->address;
	tp->t_dev = dev;
	tp->t_addr = (void*) (natural_t) addr;
	tp->t_oproc = lprstart;
	tp->t_state |= TS_WOPEN;
	tp->t_stop = lprstop;
	tp->t_getstat = lprgetstat;
	tp->t_setstat = lprsetstat;
	if ((tp->t_state & TS_ISOPEN) == 0)
		ttychars(tp);
	outb(INTR_ENAB(addr), inb(INTR_ENAB(addr)) | 0x10);
	tp->t_state |= TS_CARR_ON;
	return (char_open(dev, tp, flag, ior));
}

void
lprclose(dev_t dev, int flag)
{
int 		unit = minor(dev);
struct	tty	*tp = &lpr_tty[unit];
u_short		addr = 	(u_short) lprinfo[unit]->address;
  
	ttyclose(tp);
	if (tp->t_state&TS_HUPCLS || (tp->t_state&TS_ISOPEN)==0) {
		outb(INTR_ENAB(addr), inb(INTR_ENAB(addr)) & 0x0f);
		tp->t_state &= ~TS_BUSY;
	} 
}

int
lprread(dev_t dev, io_req_t ior)
{
	return char_read(&lpr_tty[minor(dev)], ior);
}

int
lprwrite(dev_t dev, io_req_t ior)
{
	return char_write(&lpr_tty[minor(dev)], ior);
}

int
lprportdeath(dev_t dev, mach_port_t port)
{
	return (tty_portdeath(&lpr_tty[minor(dev)], (ipc_port_t)port));
}

io_return_t
lprgetstat(
  dev_t dev,
  dev_flavor_t flavor,
  dev_status_t data, /* pointer to OUT array */
  mach_msg_type_number_t *count /* out */
  )
{
	io_return_t	result = D_SUCCESS;
	int		unit = minor(dev);

	switch (flavor) {
	default:
		result = tty_get_status(&lpr_tty[unit], flavor, data, count);
		break;
	}
	return (result);
}

io_return_t
lprsetstat(
	dev_t		dev,
	dev_flavor_t	flavor,
	dev_status_t	data,
	mach_msg_type_number_t	count)
{
	io_return_t	result = D_SUCCESS;
	int 		unit = minor(dev);

	switch (flavor) {
	default:
		result = tty_set_status(&lpr_tty[unit], flavor, data, count);
/*		if (result == D_SUCCESS && flavor == TTY_STATUS)
			lprparam(unit);
*/		return (result);
	}
	return (D_SUCCESS);
}

void lprintr(int unit)
{
	struct tty *tp = &lpr_tty[unit];

	if ((tp->t_state & TS_ISOPEN) == 0)
	  return;

	tp->t_state &= ~TS_BUSY;
	if (tp->t_state&TS_FLUSH)
		tp->t_state &=~TS_FLUSH;
	tt_write_wakeup(tp);
	lprstart(tp);
}   

void lprstart(struct tty *tp)
{
	spl_t s = spltty();
	u_short addr = (natural_t) tp->t_addr;
	int status = inb(STATUS(addr));
	int nch;

	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP|TS_BUSY)) {
		splx(s);
		return;
	}

	if (status & 0x20) {
		printf("Printer out of paper!\n");
		splx(s);
		return;
	}

	if (tp->t_outq.c_cc <= TTLOWAT(tp)) {
		tt_write_wakeup(tp);
	}
	if (tp->t_outq.c_cc == 0) {
		splx(s);
		return;
	}
	nch = getc(&tp->t_outq);
	if (nch == -1) {
		splx(s);
		return;
	}
	if ((tp->t_flags & LITOUT) == 0 && (nch & 0200)) {
		timeout((timer_func_t *)ttrstrt, (char *)tp, (nch & 0x7f) + 6);
		tp->t_state |= TS_TIMEOUT;
		return;
	}
	outb(DATA(addr), nch);
	outb(INTR_ENAB(addr),inb(INTR_ENAB(addr)) | 0x01);
	outb(INTR_ENAB(addr),inb(INTR_ENAB(addr)) & 0x1e);
	tp->t_state |= TS_BUSY;
	splx(s);
	return;
}

void
lprstop(
	struct tty 	*tp,
	int		flags)
{
	if ((tp->t_state & TS_BUSY) && (tp->t_state & TS_TTSTOP) == 0)
		tp->t_state |= TS_FLUSH;
}

void
lprpr_addr(unsigned short addr)
{
	printf("DATA(%x) %x, STATUS(%x) %x, INTR_ENAB(%x) %x\n",
		DATA(addr), inb(DATA(addr)),
		STATUS(addr), inb(STATUS(addr)),
		INTR_ENAB(addr), inb(INTR_ENAB(addr)));
}
#endif /* NLPR */
