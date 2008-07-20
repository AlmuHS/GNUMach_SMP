/*
 * pcmcia card services glue code
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

#ifndef _PCMCIA_GLUE_H
#define _PCMCIA_GLUE_H

/*
 * pcmcia glue configuration
 */
#define PCMCIA_DEBUG 4
/* Maximum number of sockets supported by the glue code. */
#define MAX_SOCKS 8


/* 
 * Linux kernel version handling.
 */
#include <linux/version.h>
#define UTS_VERSION "" /* Hm.  */
#define KERNEL_VERSION(v,p,s)          (((v)<<16)+(p<<8)+s)


/*
 * Some cardbus drivers want `CARDBUS' to be defined.
 */
#ifdef CONFIG_CARDBUS
#define CARDBUS 1
#endif


/*
 * Some includes.
 */
#include <linux/malloc.h>
#include <pcmcia/driver_ops.h>


/*
 * ioremap and iounmap
 */
#include <linux/pci.h>
#include <linux/compatmac.h>
#define iounmap(x)             (((long)x<0x100000)?0:vfree ((void*)x))


/*
 * These are implemented in rsrc_mgr.c.
 */
extern int check_mem_region(u_long base, u_long num);
extern void request_mem_region(u_long base, u_long num, char *name);
extern void release_mem_region(u_long base, u_long num);


/*
 * Timer and delaying functions.
 */
#include <linux/delay.h>
#define mod_timer(a, b) \
  do { del_timer(a); (a)->expires = (b); add_timer(a); } while (0)
#define mdelay(x) \
  do { int i; for (i=0;i<x;i++) __udelay(1000); } while (0)


/*
 * GNU Mach's Linux glue code doesn't have
 * `interruptible_sleep_on_timeout'.  For the moment let's use the
 * non-timeout variant.  :-/
 */
#define interruptible_sleep_on_timeout(w,t) \
  interruptible_sleep_on(w)

/*
 * The macro implementation relies on current_set symbol, which doesn't
 * appear to be available on GNU Mach.  TODO: How to fix this properly?
 */
#undef signal_pending
#define signal_pending(c) \
  0


/*
 * Byte order stuff.  TODO: This does not work on big endian systems,
 * does it?  Move to asm-i386?
 */
#include <asm/byteorder.h>
#ifndef le16_to_cpu
#define le16_to_cpu(x)          (x)
#define le32_to_cpu(x)          (x)
#endif
#ifndef cpu_to_le16
#define cpu_to_le16(val)        (val)
#define cpu_to_le32(val)        (val)
#endif


/* 
 * There is no `wake_up_interruptible' on GNU Mach. Use plain `wake_up'
 * for the moment.  TODO.
 */
#define wake_up_interruptible wake_up


/* Eliminate the 4-arg versions from <linux/compatmac.h>.  */
#undef pci_read_config_word
#undef pci_read_config_dword

#define bus_number(pci_dev)   ((pci_dev)->bus->number)
#define devfn_number(pci_dev) ((pci_dev)->devfn)

#define pci_read_config_byte(pdev, where, valp) \
  pcibios_read_config_byte(bus_number(pdev), devfn_number(pdev), where, valp)
#define pci_read_config_word(pdev, where, valp) \
  pcibios_read_config_word(bus_number(pdev), devfn_number(pdev), where, valp)
#define pci_read_config_dword(pdev, where, valp) \
  pcibios_read_config_dword(bus_number(pdev), devfn_number(pdev), where, valp)
#define pci_write_config_byte(pdev, where, val) \
  pcibios_write_config_byte(bus_number(pdev), devfn_number(pdev), where, val)
#define pci_write_config_word(pdev, where, val) \
  pcibios_write_config_word(bus_number(pdev), devfn_number(pdev), where, val)
#define pci_write_config_dword(pdev, where, val) \
  pcibios_write_config_dword(bus_number(pdev), devfn_number(pdev), where, val)


/*
 * From pcmcia-cs/include/linux/pci.h.
 */
#define pci_for_each_dev(p) for (p = pci_devices; p; p = p->next)  



/*
 * These are defined in pci_fixup.c.
 */
extern struct pci_dev *pci_find_slot(u_int bus, u_int devfn);
extern struct pci_dev *pci_find_class(u_int class, struct pci_dev *from);
extern int pci_set_power_state(struct pci_dev *dev, int state);
extern int pci_enable_device(struct pci_dev *dev);

extern u32 pci_irq_mask;


#ifdef PCMCIA_CLIENT
/*
 * Worse enough, we need to have `mach_device' as well (at least in ds.c)
 * and this one is typedef'd to `device', therefore we cannot just
 * include `netdevice.h' when we're compiling the core.
 *
 * For compilation of the clients `PCMCIA_CLIENT' is defined through the
 * Makefile.
 */
#include <linux/netdevice.h>
#include <linux/kcomp.h>


/*
 * init_dev_name and copy_dev_name glue (for `PCMCIA_CLIENT's only).
 */
static inline void
init_dev_name(struct net_device *dev, dev_node_t node)
{
  /* just allocate some space for the device name,
   * register_netdev will happily provide one to us 
   */
  dev->name = kmalloc(8, GFP_KERNEL);
  dev->name[0] = 0;
  
  /*
   * dev->init needs to be initialized in order for register_netdev to work
   */
  int stub(struct device *dev)
  {
    (void) dev;
    return 0;
  }
  dev->init = stub;
}

#define copy_dev_name(node, dev) do { } while (0)
#endif /* PCMCIA_CLIENT */


/*
 * Some network interface glue, additional to the one from
 * <linux/kcomp.h>.
 */
#define netif_mark_up(dev)      do { (dev)->start = 1; } while (0)
#define netif_mark_down(dev)    do { (dev)->start = 0; } while (0)
#define netif_carrier_on(dev)   do { dev->flags |= IFF_RUNNING; } while (0)
#define netif_carrier_off(dev)  do { dev->flags &= ~IFF_RUNNING; } while (0)
#define tx_timeout_check(dev, tx_timeout)			 \
  do { if (test_and_set_bit(0, (void *)&(dev)->tbusy) != 0) {	 \
      if (jiffies - (dev)->trans_start < TX_TIMEOUT) return 1;	 \
      tx_timeout(dev);						 \
    } } while (0)
 
 
/*
 * Some `struct netdevice' interface glue (from the pcmcia-cs package).
 */
#define skb_tx_check(dev, skb)				\
  do { if (skb == NULL) { dev_tint(dev); return 0; }	\
    if (skb->len <= 0) return 0; } while (0)
#define tx_timeout_check(dev, tx_timeout)			 \
  do { if (test_and_set_bit(0, (void *)&(dev)->tbusy) != 0) {	  \
      if (jiffies - (dev)->trans_start < TX_TIMEOUT) return 1;	  \
      tx_timeout(dev);						  \
    } } while (0)
#define DEV_KFREE_SKB(skb)      dev_kfree_skb(skb, FREE_WRITE)
#define net_device_stats        enet_statistics
#define add_rx_bytes(stats, n)  do { int x; x = (n); } while (0)
#define add_tx_bytes(stats, n)  do { int x; x = (n); } while (0)



/*
 * TODO: This is i386 dependent.
 */
#define readw_ns(p)             readw(p)
#define writew_ns(v,p)          writew(v,p)




/*
 * We compile everything directly into the GNU Mach kernel, there are no
 * modules.
 */
#define MODULE_PARM(a,b)
#define MODULE_AUTHOR(a)
#define MODULE_DESCRIPTION(a)
#define MODULE_LICENSE(a)

#define module_init(a) \
  void pcmcia_mod ## a (void) { a(); return; }
#define module_exit(a)

/*
 * TODO: We don't have `disable_irq_nosync', do we need it?  This is used
 * by the axnet_cs client driver only.
 */
#define disable_irq_nosync(irq) disable_irq(irq)


#endif /* _PCMCIA_GLUE_H */
