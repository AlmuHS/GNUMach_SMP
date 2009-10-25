/*
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990,1989 Carnegie Mellon University
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

#include <kern/printf.h>
#ifdef	MACH_KERNEL
#include <mach/std_types.h>
#else	/* MACH_KERNEL */
#include <cpus.h>
#include <platforms.h>
#include <generic.h>
#include <sys/param.h>
#include <mach/machine.h>
#include <machine/cpu.h>
#endif	/* MACH_KERNEL */
#include <i386/pic.h>
#include <i386/ipl.h>
#include <chips/busses.h>

/* initialization typecasts */
#define	SPL_FIVE	(vm_offset_t)SPL5
#define	SPL_SIX		(vm_offset_t)SPL6
#define	SPL_TTY		(vm_offset_t)SPLTTY


#if NCOM > 0
extern	struct	bus_driver	comdriver;
extern void			comintr();
#endif /* NCOM */

#if NLPR > 0
extern	struct	bus_driver	lprdriver;
extern void			lprintr();
#endif /* NLPR */

struct	bus_ctlr	bus_master_init[] = {

/* driver    name unit intr    address        len phys_address
     adaptor alive flags spl    pic				 */

  {0}
};


struct	bus_device	bus_device_init[] = {

/* driver     name unit intr    address       am   phys_address
     adaptor alive ctlr slave flags *mi       *next  sysdep sysdep */

#if NCOM > 0
  {&comdriver, "com", 0, comintr, 0x3f8, 8, 0x3f8,
     '?',    0,   -1,    -1,    0,   0,        0,   SPL_TTY, 4},
  {&comdriver, "com", 1, comintr, 0x2f8, 8, 0x2f8,
     '?',    0,   -1,    -1,    0,   0,        0,   SPL_TTY, 3},
  {&comdriver, "com", 2, comintr, 0x3e8, 8, 0x3e8,
     '?',    0,   -1,    -1,    0,   0,        0,   SPL_TTY, 5},
#endif /* NCOM > 0 */

#ifdef MACH_LPR
#if NLPR > 0
  {&lprdriver, "lpr", 0, lprintr, 0x378, 3, 0x378,
     '?',    0,   -1,    -1,    0,   0,        0,   SPL_TTY, 7},
  {&lprdriver, "lpr", 0, lprintr, 0x278, 3, 0x278,
     '?',    0,   -1,    -1,    0,   0,        0,   SPL_TTY, 7},
  {&lprdriver, "lpr", 0, lprintr, 0x3bc, 3, 0x3bc,
     '?',    0,   -1,    -1,    0,   0,        0,   SPL_TTY, 7},
#endif /* NLPR > 0 */
#endif /* MACH_LPR */

  {0}
};

/*
 * probeio:
 *
 *	Probe and subsequently attach devices out on the AT bus.
 *
 *
 */
void probeio(void)
{
	register struct	bus_device	*device;
	register struct	bus_ctlr	*master;
	int				i = 0;

	for (master = bus_master_init; master->driver; master++)
	{
		if (configure_bus_master(master->name, master->address,
				master->phys_address, i, "atbus"))
			i++;
	}

	for (device = bus_device_init; device->driver; device++)
	{
		/* ignore what we (should) have found already */
		if (device->alive || device->ctlr >= 0)
			continue;
		if (configure_bus_device(device->name, device->address,
				device->phys_address, i, "atbus"))
			i++;
	}

#if	MACH_TTD
	/*
	 * Initialize Remote kernel debugger.
	 */
	ttd_init();
#endif	/* MACH_TTD */
}

void take_dev_irq(
	struct bus_device *dev)
{
	int pic = (int)dev->sysdep1;

	if (intpri[pic] == 0) {
		iunit[pic] = dev->unit;
		ivect[pic] = dev->intr;
		intpri[pic] = (int)dev->sysdep;
		form_pic_mask();
	} else {
		printf("The device below will clobber IRQ %d.\n", pic);
		printf("You have two devices at the same IRQ.\n");
		printf("This won't work.  Reconfigure your hardware and try again.\n");
		printf("%s%d: port = %x, spl = %d, pic = %d.\n",
		        dev->name, dev->unit, dev->address,
			dev->sysdep, dev->sysdep1);
		while (1);
	}

}

void take_ctlr_irq(
	struct bus_ctlr *ctlr)
{
	int pic = ctlr->sysdep1;
	if (intpri[pic] == 0) {
		iunit[pic] = ctlr->unit;
		ivect[pic] = ctlr->intr;
		intpri[pic] = (int)ctlr->sysdep;
		form_pic_mask();
	} else {
		printf("The device below will clobber IRQ %d.\n", pic);
		printf("You have two devices at the same IRQ.  This won't work.\n");
		printf("Reconfigure your hardware and try again.\n");
		while (1);
	}
}
