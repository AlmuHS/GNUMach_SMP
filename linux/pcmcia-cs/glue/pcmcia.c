/*
 * pcmcia bridge initialization
 *
 * Copyright (C) 2006 Free Software Foundation, Inc.
 * Written by Stefan Siegl <stesie@brokenpipe.de>.
 *
 * This file is part of GNU Mach.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/proc_fs.h>
#include <linux/pci.h>

#include <asm/spinlock.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/cs.h>

extern int init_pcmcia_cs(void);
extern int init_i82365(void);
extern int init_pcmcia_ds(void);

extern int pcmcia_modinit_pcnet_cs(void);
extern int pcmcia_modinit_3c589_cs(void);
extern int pcmcia_modinit_3c574_cs(void);
extern int pcmcia_modinit_3c575_cb(void);
extern int pcmcia_modinit_axnet_cs(void);
extern int pcmcia_modinit_eepro100_cb(void);
extern int pcmcia_modinit_epic_cb(void);
extern int pcmcia_modinit_fmvj18x_cs(void);
extern int pcmcia_modinit_nmclan_cs(void);
extern int pcmcia_modinit_smc91c92_cs(void);
extern int pcmcia_modinit_tulip_cb(void);
extern int pcmcia_modinit_xirc2ps_cs(void);

extern int pcmcia_modinit_orinoco_cs(void);

/*
 * pcmcia bridge initialisation.
 */
void
pcmcia_init(void)
{
  init_pcmcia_cs();

#ifdef CONFIG_I82365
  init_i82365();
#endif

  init_pcmcia_ds();

  /*
   * Call te initialization routines of each driver.
   */
#ifdef CONFIG_PCNET_CS
  pcmcia_modinit_pcnet_cs();
#endif

#ifdef CONFIG_3C589_CS
  pcmcia_modinit_3c589_cs();
#endif

#ifdef CONFIG_3C574_CS
  pcmcia_modinit_3c574_cs();
#endif

#ifdef CONFIG_3C575_CB
  pcmcia_modinit_3c575_cb();
#endif

#ifdef CONFIG_AXNET_CS
  pcmcia_modinit_axnet_cs();
#endif

#ifdef CONFIG_EEPRO100_CB
  pcmcia_modinit_eepro100_cb();
#endif

#ifdef CONFIG_EPIC_CB
  pcmcia_modinit_epic_cb();
#endif

#ifdef CONFIG_FMVJ18X_CS
  pcmcia_modinit_fmvj18x_cs();
#endif

#ifdef CONFIG_NMCLAN_CS
  pcmcia_modinit_nmclan_cs();
#endif

#ifdef CONFIG_SMC91C92_CS
  pcmcia_modinit_smc91c92_cs();
#endif

#ifdef CONFIG_TULIP_CB
  pcmcia_modinit_tulip_cb();
#endif

#ifdef CONFIG_XIRC2PS_CS
  pcmcia_modinit_xirc2ps_cs();
#endif

#ifdef CONFIG_ORINOCO_CS
  pcmcia_modinit_orinoco_cs();
#endif
}
