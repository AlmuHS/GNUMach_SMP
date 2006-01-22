/* via-rhine.c: A Linux Ethernet device driver for VIA Rhine family chips. */
/*
	Written 1998-2003 by Donald Becker.

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	This driver is designed for the VIA VT86c100A Rhine-II PCI Fast Ethernet
	controller.  It also works with the older 3043 Rhine-I chip.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	914 Bay Ridge Road, Suite 220
	Annapolis MD 21403

	Support information and updates available at
		http://www.scyld.com/network/via-rhine.html
	The information and support mailing lists are based at
		http://www.scyld.com/mailman/listinfo/
*/

/* These identify the driver base version and may not be removed. */
static const char version1[] =
"via-rhine.c:v1.16 7/22/2003  Written by Donald Becker <becker@scyld.com>\n";
static const char version2[] =
"  http://www.scyld.com/network/via-rhine.html\n";

/* Automatically extracted configuration info:
probe-func: via_rhine_probe
config-in: tristate 'VIA "Rhine" vt86c100, vt3043, and vt3065 series PCI Ethernet support' CONFIG_VIA_RHINE

c-help-name: VIA Rhine series PCI Ethernet support
c-help-symbol: CONFIG_VIA_RHINE
c-help: This driver is for the VIA Rhine (v3043) and Rhine-II
c-help: (vt3065 AKA vt86c100) network adapter chip series.
c-help: More specific information and updates are available from 
c-help: http://www.scyld.com/network/via-rhine.html
*/

/* The user-configurable values.
   These may be modified when a driver module is loaded.*/

/* Message enable level: 0..31 = no..all messages.  See NETIF_MSG docs. */
static int debug = 2;

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;

/* Set the copy breakpoint for the copy-only-tiny-frames scheme.
   Setting to > 1518 effectively disables this feature. */
static int rx_copybreak = 0;

/* Used to pass the media type, etc.
   Both 'options[]' and 'full_duplex[]' should exist for driver
   interoperability.
   The media type is usually passed in 'options[]'.
    The default is autonegotation for speed and duplex.
	This should rarely be overridden.
    Use option values 0x10/0x20 for 10Mbps, 0x100,0x200 for 100Mbps.
    Use option values 0x10 and 0x100 for forcing half duplex fixed speed.
    Use option values 0x20 and 0x200 for forcing full duplex operation.
*/
#define MAX_UNITS 8		/* More are supported, limit only on options */
static int options[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int full_duplex[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};

/* Maximum number of multicast addresses to filter (vs. rx-all-multicast).
   The Rhine has a 64 element 8390-like hash table.  */
static const int multicast_filter_limit = 32;

/* Operational parameters that are set at compile time. */

/* Making the Tx ring too large decreases the effectiveness of channel
   bonding and packet priority.
   There are no ill effects from too-large receive rings. */
#define TX_RING_SIZE	16
#define TX_QUEUE_LEN	10		/* Limit ring entries actually used.  */
#define RX_RING_SIZE	32

/* Operational parameters that usually are not changed. */
/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT  (6*HZ)

/* Allocation size of Rx buffers with normal sized Ethernet frames.
   Do not change this value without good reason.  This is not a limit,
   but a way to keep a consistent allocation size among drivers.
 */
#define PKT_BUF_SZ		1536

#ifndef __KERNEL__
#define __KERNEL__
#endif
#if !defined(__OPTIMIZE__)
#warning  You must compile this file with the correct options!
#warning  See the last lines of the source file.
#error You must compile this driver with "-O".
#endif

/* Include files, designed to support most kernel versions 2.0.0 and later. */
#include <linux/config.h>
#if defined(CONFIG_SMP) && ! defined(__SMP__)
#define __SMP__
#endif
#if defined(MODULE) && defined(CONFIG_MODVERSIONS) && ! defined(MODVERSIONS)
#define MODVERSIONS
#endif

#include <linux/version.h>
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#if LINUX_VERSION_CODE >= 0x20400
#include <linux/slab.h>
#else
#include <linux/malloc.h>
#endif
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <asm/processor.h>		/* Processor type for cache alignment. */
#include <asm/bitops.h>
#include <asm/io.h>

#ifdef INLINE_PCISCAN
#include "k_compat.h"
#else
#include "pci-scan.h"
#include "kern_compat.h"
#endif

/* Condensed bus+endian portability operations. */
#define virt_to_le32desc(addr)	cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr)	bus_to_virt(le32_to_cpu(addr))

/* This driver was written to use PCI memory space, however most versions
   of the Rhine only work correctly with I/O space accesses. */
#if defined(VIA_USE_MEMORY)
#warning Many adapters using the VIA Rhine chip are not configured to work
#warning with PCI memory space accesses.
#else
#define USE_IO_OPS
#undef readb
#undef readw
#undef readl
#undef writeb
#undef writew
#undef writel
#define readb inb
#define readw inw
#define readl inl
#define writeb outb
#define writew outw
#define writel outl
#endif

#if (LINUX_VERSION_CODE >= 0x20100)  &&  defined(MODULE)
char kernel_version[] = UTS_RELEASE;
#endif

MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("VIA Rhine PCI Fast Ethernet driver");
MODULE_LICENSE("GPL");
MODULE_PARM(max_interrupt_work, "i");
MODULE_PARM(debug, "i");
MODULE_PARM(rx_copybreak, "i");
MODULE_PARM(options, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(full_duplex, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(multicast_filter_limit, "i");
MODULE_PARM_DESC(debug, "Driver message level (0-31)");
MODULE_PARM_DESC(options, "Force transceiver type or fixed speed+duplex");
MODULE_PARM_DESC(max_interrupt_work,
				 "Driver maximum events handled per interrupt");
MODULE_PARM_DESC(full_duplex, "Non-zero to set forced full duplex "
				 "(deprecated, use options[] instead).");
MODULE_PARM_DESC(rx_copybreak,
				 "Breakpoint in bytes for copy-only-tiny-frames");
MODULE_PARM_DESC(multicast_filter_limit,
				 "Multicast addresses before switching to Rx-all-multicast");

/*
				Theory of Operation

I. Board Compatibility

This driver is designed for the VIA 86c100A Rhine-II PCI Fast Ethernet
controller.

II. Board-specific settings

Boards with this chip are functional only in a bus-master PCI slot.

Many operational settings are loaded from the EEPROM to the Config word at
offset 0x78.  This driver assumes that they are correct.
If this driver is compiled to use PCI memory space operations the EEPROM
must be configured to enable memory ops.

III. Driver operation

IIIa. Ring buffers

This driver uses two statically allocated fixed-size descriptor lists
formed into rings by a branch from the final descriptor to the beginning of
the list.  The ring sizes are set at compile time by RX/TX_RING_SIZE.

IIIb/c. Transmit/Receive Structure

This driver attempts to use a zero-copy receive and transmit scheme.

Alas, all data buffers are required to start on a 32 bit boundary, so
the driver must often copy transmit packets into bounce buffers.

The driver allocates full frame size skbuffs for the Rx ring buffers at
open() time and passes the skb->data field to the chip as receive data
buffers.  When an incoming frame is less than RX_COPYBREAK bytes long,
a fresh skbuff is allocated and the frame is copied to the new skbuff.
When the incoming frame is larger, the skbuff is passed directly up the
protocol stack.  Buffers consumed this way are replaced by newly allocated
skbuffs in the last phase of netdev_rx().

The RX_COPYBREAK value is chosen to trade-off the memory wasted by
using a full-sized skbuff for small frames vs. the copying costs of larger
frames.  New boards are typically used in generously configured machines
and the underfilled buffers have negligible impact compared to the benefit of
a single allocation size, so the default value of zero results in never
copying packets.  When copying is done, the cost is usually mitigated by using
a combined copy/checksum routine.  Copying also preloads the cache, which is
most useful with small frames.

Since the VIA chips are only able to transfer data to buffers on 32 bit
boundaries, the the IP header at offset 14 in an ethernet frame isn't
longword aligned for further processing.  Copying these unaligned buffers
has the beneficial effect of 16-byte aligning the IP header.

IIId. Synchronization

The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->tbusy flag.  The other thread is the interrupt handler, which is single
threaded by the hardware and interrupt handling software.

The send packet thread has partial control over the Tx ring and 'dev->tbusy'
flag.  It sets the tbusy flag whenever it's queuing a Tx packet. If the next
queue slot is empty, it clears the tbusy flag when finished otherwise it sets
the 'lp->tx_full' flag.

The interrupt handler has exclusive control over the Rx ring and records stats
from the Tx ring.  After reaping the stats, it marks the Tx queue entry as
empty by incrementing the dirty_tx mark. Iff the 'lp->tx_full' flag is set, it
clears both the tx_full and tbusy flags.

IV. Notes

IVb. References

This driver was originally written using a preliminary VT86C100A manual
from
  http://www.via.com.tw/ 
The usual background material was used:
  http://www.scyld.com/expert/100mbps.html
  http://scyld.com/expert/NWay.html

Additional information is now available, especially for the newer chips.
   http://www.via.com.tw/en/Networking/DS6105LOM100.pdf

IVc. Errata

The VT86C100A manual is not reliable information.
The 3043 chip does not handle unaligned transmit or receive buffers,
resulting in significant performance degradation for bounce buffer
copies on transmit and unaligned IP headers on receive.
The chip does not pad to minimum transmit length.

There is a bug with the transmit descriptor pointer handling when the
chip encounters a transmit error.

*/



static void *via_probe1(struct pci_dev *pdev, void *init_dev,
						long ioaddr, int irq, int chip_idx, int find_cnt);
static int via_pwr_event(void *dev_instance, int event);
enum chip_capability_flags {
	CanHaveMII=1, HasESIPhy=2, HasDavicomPhy=4, HasV1TxStat=8,
	ReqTxAlign=0x10, HasWOL=0x20, HasIPChecksum=0x40, HasVLAN=0x80,
	
};

#if defined(VIA_USE_MEMORY)
#define RHINE_IOTYPE (PCI_USES_MEM | PCI_USES_MASTER | PCI_ADDR1)
#define RHINE_I_IOSIZE 128
#define RHINEII_IOSIZE 4096
#else
#define RHINE_IOTYPE (PCI_USES_IO  | PCI_USES_MASTER | PCI_ADDR0)
#define RHINE_I_IOSIZE 128
#define RHINEII_IOSIZE 256
#endif

static struct pci_id_info pci_tbl[] = {
	{ "VIA VT3043 Rhine", { 0x30431106, 0xffffffff,},
	  RHINE_IOTYPE, RHINE_I_IOSIZE, CanHaveMII | ReqTxAlign | HasV1TxStat },
	{ "VIA VT86C100A Rhine", { 0x61001106, 0xffffffff,},
	  RHINE_IOTYPE, RHINE_I_IOSIZE, CanHaveMII | ReqTxAlign | HasV1TxStat },
	{ "VIA VT6102 Rhine-II", { 0x30651106, 0xffffffff,},
	  RHINE_IOTYPE, RHINEII_IOSIZE, CanHaveMII | HasWOL },
	{ "VIA VT6105LOM Rhine-III (3106)", { 0x31061106, 0xffffffff,},
	  RHINE_IOTYPE, RHINEII_IOSIZE, CanHaveMII | HasWOL },
	/* Duplicate entry, with 'M' features enabled. */
	{ "VIA VT6105M Rhine-III (3106)", { 0x31061106, 0xffffffff,},
	  RHINE_IOTYPE, RHINEII_IOSIZE, CanHaveMII|HasWOL|HasIPChecksum|HasVLAN},
	{ "VIA VT6105M Rhine-III (3053 prototype)", { 0x30531106, 0xffffffff,},
	  RHINE_IOTYPE, RHINEII_IOSIZE, CanHaveMII | HasWOL },
	{0,},						/* 0 terminated list. */
};

struct drv_id_info via_rhine_drv_id = {
	"via-rhine", PCI_HOTSWAP, PCI_CLASS_NETWORK_ETHERNET<<8, pci_tbl,
	via_probe1, via_pwr_event
};

/* Offsets to the device registers.
*/
enum register_offsets {
	StationAddr=0x00, RxConfig=0x06, TxConfig=0x07, ChipCmd=0x08,
	IntrStatus=0x0C, IntrEnable=0x0E,
	MulticastFilter0=0x10, MulticastFilter1=0x14,
	RxRingPtr=0x18, TxRingPtr=0x1C,
	MIIPhyAddr=0x6C, MIIStatus=0x6D, PCIBusConfig=0x6E,
	MIICmd=0x70, MIIRegAddr=0x71, MIIData=0x72, MACRegEEcsr=0x74,
	Config=0x78, ConfigA=0x7A, RxMissed=0x7C, RxCRCErrs=0x7E,
	StickyHW=0x83, WOLcrClr=0xA4, WOLcgClr=0xA7, PwrcsrClr=0xAC,
};

/* Bits in the interrupt status/mask registers. */
enum intr_status_bits {
	IntrRxDone=0x0001, IntrRxErr=0x0004, IntrRxEmpty=0x0020,
	IntrTxDone=0x0002, IntrTxAbort=0x0008, IntrTxUnderrun=0x0010,
	IntrPCIErr=0x0040,
	IntrStatsMax=0x0080, IntrRxEarly=0x0100, IntrMIIChange=0x0200,
	IntrRxOverflow=0x0400, IntrRxDropped=0x0800, IntrRxNoBuf=0x1000,
	IntrTxAborted=0x2000, IntrLinkChange=0x4000,
	IntrRxWakeUp=0x8000,
	IntrNormalSummary=0x0003, IntrAbnormalSummary=0xC260,
};

/* The Rx and Tx buffer descriptors. */
struct rx_desc {
	s32 rx_status;
	u32 desc_length;
	u32 addr;
	u32 next_desc;
};
struct tx_desc {
	s32 tx_status;
	u32 desc_length;
	u32 addr;
	u32 next_desc;
};

/* Bits in *_desc.status */
enum rx_status_bits {
	RxOK=0x8000, RxWholePkt=0x0300, RxErr=0x008F};
enum desc_status_bits {
	DescOwn=0x80000000, DescEndPacket=0x4000, DescIntr=0x1000,
};

/* Bits in rx.desc_length for extended status. */
enum rx_info_bits {
	RxTypeTag=0x00010000,
	RxTypeUDP=0x00020000, RxTypeTCP=0x00040000, RxTypeIP=0x00080000,
	RxTypeUTChksumOK=0x00100000, RxTypeIPChksumOK=0x00200000,
	/* Summarized. */
	RxTypeCsumMask=0x003E0000,
	RxTypeUDPSumOK=0x003A0000, RxTypeTCPSumOK=0x003C0000, 
};

/* Bits in ChipCmd. */
enum chip_cmd_bits {
	CmdInit=0x0001, CmdStart=0x0002, CmdStop=0x0004, CmdRxOn=0x0008,
	CmdTxOn=0x0010, CmdTxDemand=0x0020, CmdRxDemand=0x0040,
	CmdEarlyRx=0x0100, CmdEarlyTx=0x0200, CmdFDuplex=0x0400,
	CmdNoTxPoll=0x0800, CmdReset=0x8000,
};

#define PRIV_ALIGN	15	/* Required alignment mask */
/* Use  __attribute__((aligned (L1_CACHE_BYTES)))  to maintain alignment
   within the structure. */
struct netdev_private {
	/* Descriptor rings first for alignment. */
	struct rx_desc rx_ring[RX_RING_SIZE];
	struct tx_desc tx_ring[TX_RING_SIZE];
	/* The addresses of receive-in-place skbuffs. */
	struct sk_buff* rx_skbuff[RX_RING_SIZE];
	/* The saved address of a sent-in-place packet/buffer, for later free(). */
	struct sk_buff* tx_skbuff[TX_RING_SIZE];
	unsigned char *tx_buf[TX_RING_SIZE];	/* Tx bounce buffers */
	unsigned char *tx_bufs;				/* Tx bounce buffer region. */
	struct net_device *next_module;		/* Link for devices of this type. */
	void *priv_addr;					/* Unaligned address for kfree */
	struct net_device_stats stats;
	struct timer_list timer;	/* Media monitoring timer. */
	int msg_level;
	int max_interrupt_work;
	int intr_enable;
	int chip_id, drv_flags;
	struct pci_dev *pci_dev;

	/* Frequently used values: keep some adjacent for cache effect. */

	struct rx_desc *rx_head_desc;
	unsigned int cur_rx, dirty_rx;		/* Producer/consumer ring indices */
	unsigned int rx_buf_sz;				/* Based on MTU+slack. */
	int rx_copybreak;

	unsigned int cur_tx, dirty_tx;
	u16 chip_cmd;						/* Current setting for ChipCmd */
	int multicast_filter_limit;
	u32 mc_filter[2];
	int rx_mode;
	unsigned int tx_full:1;				/* The Tx queue is full. */
	/* These values are keep track of the transceiver/media in use. */
	unsigned int full_duplex:1;			/* Full-duplex operation requested. */
	unsigned int duplex_lock:1;
	unsigned int medialock:1;			/* Do not sense media. */
	unsigned int default_port;			/* Last dev->if_port value. */
	u8 tx_thresh, rx_thresh;
	/* MII transceiver section. */
	int mii_cnt;						/* MII device addresses. */
	u16 advertising;					/* NWay media advertisement */
	unsigned char phys[2];				/* MII device addresses. */
};

static int  mdio_read(struct net_device *dev, int phy_id, int location);
static void mdio_write(struct net_device *dev, int phy_id, int location, int value);
static int  netdev_open(struct net_device *dev);
static void check_duplex(struct net_device *dev);
static void netdev_timer(unsigned long data);
static void tx_timeout(struct net_device *dev);
static void init_ring(struct net_device *dev);
static int  start_tx(struct sk_buff *skb, struct net_device *dev);
static void intr_handler(int irq, void *dev_instance, struct pt_regs *regs);
static int  netdev_rx(struct net_device *dev);
static void netdev_error(struct net_device *dev, int intr_status);
static void set_rx_mode(struct net_device *dev);
static struct net_device_stats *get_stats(struct net_device *dev);
static int mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static int  netdev_close(struct net_device *dev);



/* A list of our installed devices, for removing the driver module. */
static struct net_device *root_net_dev = NULL;

#ifndef MODULE
int via_rhine_probe(struct net_device *dev)
{
	printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	return pci_drv_register(&via_rhine_drv_id, dev);
}
#endif

static void *via_probe1(struct pci_dev *pdev, void *init_dev,
						long ioaddr, int irq, int chip_idx, int card_idx)
{
	struct net_device *dev;
	struct netdev_private *np;
	void *priv_mem;
	int i, option = card_idx < MAX_UNITS ? options[card_idx] : 0;

	dev = init_etherdev(init_dev, 0);
	if (!dev)
		return NULL;

	printk(KERN_INFO "%s: %s at 0x%lx, ",
		   dev->name, pci_tbl[chip_idx].name, ioaddr);

	/* We would prefer to directly read the EEPROM but access may be locked. */
	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = readb(ioaddr + StationAddr + i);
	if (memcmp(dev->dev_addr, "\0\0\0\0\0", 6) == 0) {
		/* Reload the station address from the EEPROM. */
		writeb(0x20, ioaddr + MACRegEEcsr);
 		/* Typically 2 cycles to reload. */
		for (i = 0; i < 150; i++)
			if (! (readb(ioaddr + MACRegEEcsr) & 0x20))
				break;
		for (i = 0; i < 6; i++)
			dev->dev_addr[i] = readb(ioaddr + StationAddr + i);
		if (memcmp(dev->dev_addr, "\0\0\0\0\0", 6) == 0) {
			printk(" (MISSING EEPROM ADDRESS)");
			/* Fill a temp addr with the "locally administered" bit set. */
			memcpy(dev->dev_addr, ">Linux", 6);
		}
	}

	for (i = 0; i < 5; i++)
		printk("%2.2x:", dev->dev_addr[i]);
	printk("%2.2x, IRQ %d.\n", dev->dev_addr[i], irq);

	/* Make certain the descriptor lists are cache-aligned. */
	priv_mem = kmalloc(sizeof(*np) + PRIV_ALIGN, GFP_KERNEL);
	/* Check for the very unlikely case of no memory. */
	if (priv_mem == NULL)
		return NULL;

#ifdef USE_IO_OPS
	request_region(ioaddr, pci_tbl[chip_idx].io_size, dev->name);
#endif

	/* Reset the chip to erase previous misconfiguration. */
	writew(CmdReset, ioaddr + ChipCmd);

	dev->base_addr = ioaddr;
	dev->irq = irq;

	dev->priv = np = (void *)(((long)priv_mem + PRIV_ALIGN) & ~PRIV_ALIGN);
	memset(np, 0, sizeof(*np));
	np->priv_addr = priv_mem;

	np->next_module = root_net_dev;
	root_net_dev = dev;

	np->pci_dev = pdev;
	np->chip_id = chip_idx;
	np->drv_flags = pci_tbl[chip_idx].drv_flags;
	np->msg_level = (1 << debug) - 1;
	np->rx_copybreak = rx_copybreak;
	np->max_interrupt_work = max_interrupt_work;
	np->multicast_filter_limit = multicast_filter_limit;

	if (dev->mem_start)
		option = dev->mem_start;

	/* The lower four bits are the media type. */
	if (option > 0) {
		if (option & 0x220)
			np->full_duplex = 1;
		np->default_port = option & 15;
		if (np->default_port)
			np->medialock = 1;
	}
	if (card_idx < MAX_UNITS  &&  full_duplex[card_idx] > 0)
		np->full_duplex = 1;

	if (np->full_duplex) {
		printk(KERN_INFO "%s: Set to forced full duplex, autonegotiation"
			   " disabled.\n", dev->name);
		np->duplex_lock = 1;
	}

	/* The chip-specific entries in the device structure. */
	dev->open = &netdev_open;
	dev->hard_start_xmit = &start_tx;
	dev->stop = &netdev_close;
	dev->get_stats = &get_stats;
	dev->set_multicast_list = &set_rx_mode;
	dev->do_ioctl = &mii_ioctl;

	if (np->drv_flags & CanHaveMII) {
		int phy, phy_idx = 0;
		np->phys[0] = 1;		/* Standard for this chip. */
		for (phy = 1; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(dev, phy, 1);
			if (mii_status != 0xffff  &&  mii_status != 0x0000) {
				np->phys[phy_idx++] = phy;
				np->advertising = mdio_read(dev, phy, 4);
				printk(KERN_INFO "%s: MII PHY found at address %d, status "
					   "0x%4.4x advertising %4.4x Link %4.4x.\n",
					   dev->name, phy, mii_status, np->advertising,
					   mdio_read(dev, phy, 5));
			}
		}
		np->mii_cnt = phy_idx;
	}

	/* Allow forcing the media type. */
	if (option > 0) {
		if (option & 0x220)
			np->full_duplex = 1;
		np->default_port = option & 0x3ff;
		if (np->default_port & 0x330) {
			np->medialock = 1;
			printk(KERN_INFO "  Forcing %dMbs %s-duplex operation.\n",
				   (option & 0x300 ? 100 : 10),
				   (np->full_duplex ? "full" : "half"));
			if (np->mii_cnt)
				mdio_write(dev, np->phys[0], 0,
						   ((option & 0x300) ? 0x2000 : 0) | 	/* 100mbps? */
						   (np->full_duplex ? 0x0100 : 0)); /* Full duplex? */
		}
	}

	return dev;
}


/* Read and write over the MII Management Data I/O (MDIO) interface. */

static int mdio_read(struct net_device *dev, int phy_id, int regnum)
{
	long ioaddr = dev->base_addr;
	int boguscnt = 1024;

	/* Wait for a previous command to complete. */
	while ((readb(ioaddr + MIICmd) & 0x60) && --boguscnt > 0)
		;
	writeb(0x00, ioaddr + MIICmd);
	writeb(phy_id, ioaddr + MIIPhyAddr);
	writeb(regnum, ioaddr + MIIRegAddr);
	writeb(0x40, ioaddr + MIICmd);			/* Trigger read */
	boguscnt = 1024;
	while ((readb(ioaddr + MIICmd) & 0x40) && --boguscnt > 0)
		;
	return readw(ioaddr + MIIData);
}

static void mdio_write(struct net_device *dev, int phy_id, int regnum, int value)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int boguscnt = 1024;

	if (phy_id == np->phys[0]) {
		switch (regnum) {
		case 0:					/* Is user forcing speed/duplex? */
			if (value & 0x9000)	/* Autonegotiation. */
				np->duplex_lock = 0;
			else
				np->full_duplex = (value & 0x0100) ? 1 : 0;
			break;
		case 4: np->advertising = value; break;
		}
	}
	/* Wait for a previous command to complete. */
	while ((readb(ioaddr + MIICmd) & 0x60) && --boguscnt > 0)
		;
	writeb(0x00, ioaddr + MIICmd);
	writeb(phy_id, ioaddr + MIIPhyAddr);
	writeb(regnum, ioaddr + MIIRegAddr);
	writew(value, ioaddr + MIIData);
	writeb(0x20, ioaddr + MIICmd);			/* Trigger write. */
	return;
}


static int netdev_open(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int i;

	/* Reset the chip. */
	writew(CmdReset, ioaddr + ChipCmd);

	MOD_INC_USE_COUNT;

	if (request_irq(dev->irq, &intr_handler, SA_SHIRQ, dev->name, dev)) {
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}

	if (np->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: netdev_open() irq %d.\n",
			   dev->name, dev->irq);

	init_ring(dev);

	writel(virt_to_bus(np->rx_ring), ioaddr + RxRingPtr);
	writel(virt_to_bus(np->tx_ring), ioaddr + TxRingPtr);

	for (i = 0; i < 6; i++)
		writeb(dev->dev_addr[i], ioaddr + StationAddr + i);

	/* Initialize other registers. */
	writew(0x0006, ioaddr + PCIBusConfig);	/* Tune configuration??? */
	/* Configure the FIFO thresholds. */
	writeb(0x20, ioaddr + TxConfig);	/* Initial threshold 32 bytes */
	np->tx_thresh = 0x20;
	np->rx_thresh = 0x60;				/* Written in set_rx_mode(). */

	if (dev->if_port == 0)
		dev->if_port = np->default_port;

	set_rx_mode(dev);
	netif_start_tx_queue(dev);

	np->intr_enable = IntrRxDone | IntrRxErr | IntrRxEmpty |
		IntrRxOverflow| IntrRxDropped| IntrTxDone | IntrTxAbort |
		IntrTxUnderrun | IntrPCIErr | IntrStatsMax | IntrLinkChange |
		IntrMIIChange;
	/* Enable interrupts by setting the interrupt mask. */
	writew(np->intr_enable, ioaddr + IntrEnable);

	np->chip_cmd = CmdStart|CmdTxOn|CmdRxOn|CmdNoTxPoll;
	if (np->duplex_lock)
		np->chip_cmd |= CmdFDuplex;
	writew(np->chip_cmd, ioaddr + ChipCmd);

	check_duplex(dev);
	/* The LED outputs of various MII xcvrs should be configured.  */
	/* For NS or Mison phys, turn on bit 1 in register 0x17 */
	/* For ESI phys, turn on bit 7 in register 0x17. */
	mdio_write(dev, np->phys[0], 0x17, mdio_read(dev, np->phys[0], 0x17) |
			   (np->drv_flags & HasESIPhy) ? 0x0080 : 0x0001);

	if (np->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: Done netdev_open(), status %4.4x "
			   "MII status: %4.4x.\n",
			   dev->name, readw(ioaddr + ChipCmd),
			   mdio_read(dev, np->phys[0], 1));

	/* Set the timer to check for link beat. */
	init_timer(&np->timer);
	np->timer.expires = jiffies + 2;
	np->timer.data = (unsigned long)dev;
	np->timer.function = &netdev_timer;				/* timer handler */
	add_timer(&np->timer);

	return 0;
}

static void check_duplex(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int mii_reg5 = mdio_read(dev, np->phys[0], 5);
	int negotiated = mii_reg5 & np->advertising;
	int duplex;

	if (np->duplex_lock  ||  mii_reg5 == 0xffff)
		return;
	duplex = (negotiated & 0x0100) || (negotiated & 0x01C0) == 0x0040;
	if (np->full_duplex != duplex) {
		np->full_duplex = duplex;
		if (np->msg_level & NETIF_MSG_LINK)
			printk(KERN_INFO "%s: Setting %s-duplex based on MII #%d link"
				   " partner capability of %4.4x.\n", dev->name,
				   duplex ? "full" : "half", np->phys[0], mii_reg5);
		if (duplex)
			np->chip_cmd |= CmdFDuplex;
		else
			np->chip_cmd &= ~CmdFDuplex;
		writew(np->chip_cmd, ioaddr + ChipCmd);
	}
}

static void netdev_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int next_tick = 10*HZ;

	if (np->msg_level & NETIF_MSG_TIMER) {
		printk(KERN_DEBUG "%s: VIA Rhine monitor tick, status %4.4x.\n",
			   dev->name, readw(ioaddr + IntrStatus));
	}
	if (netif_queue_paused(dev)
		&& np->cur_tx - np->dirty_tx > 1
		&& jiffies - dev->trans_start > TX_TIMEOUT)
		tx_timeout(dev);

	check_duplex(dev);

	np->timer.expires = jiffies + next_tick;
	add_timer(&np->timer);
}

static void tx_timeout(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;

	printk(KERN_WARNING "%s: Transmit timed out, status %4.4x, PHY status "
		   "%4.4x, resetting...\n",
		   dev->name, readw(ioaddr + IntrStatus),
		   mdio_read(dev, np->phys[0], 1));

	/* Perhaps we should reinitialize the hardware here. */
	dev->if_port = 0;
	/* Restart the chip's Tx processes . */
	writel(virt_to_bus(np->tx_ring + (np->dirty_tx % TX_RING_SIZE)),
		   ioaddr + TxRingPtr);
	writew(CmdTxDemand | np->chip_cmd, dev->base_addr + ChipCmd);

	/* Trigger an immediate transmit demand. */

	dev->trans_start = jiffies;
	np->stats.tx_errors++;
	return;
}


/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void init_ring(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	int i;

	np->tx_full = 0;
	np->cur_rx = np->cur_tx = 0;
	np->dirty_rx = np->dirty_tx = 0;

	/* Use 1518/+18 if the CRC is transferred. */
	np->rx_buf_sz = dev->mtu + 14;
	if (np->rx_buf_sz < PKT_BUF_SZ)
		np->rx_buf_sz = PKT_BUF_SZ;
	np->rx_head_desc = &np->rx_ring[0];

	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].rx_status = 0;
		np->rx_ring[i].desc_length = cpu_to_le32(np->rx_buf_sz);
		np->rx_ring[i].next_desc = virt_to_le32desc(&np->rx_ring[i+1]);
		np->rx_skbuff[i] = 0;
	}
	/* Mark the last entry as wrapping the ring. */
	np->rx_ring[i-1].next_desc = virt_to_le32desc(&np->rx_ring[0]);

	/* Fill in the Rx buffers.  Handle allocation failure gracefully. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = dev_alloc_skb(np->rx_buf_sz);
		np->rx_skbuff[i] = skb;
		if (skb == NULL)
			break;
		skb->dev = dev;			/* Mark as being used by this device. */
		np->rx_ring[i].addr = virt_to_le32desc(skb->tail);
		np->rx_ring[i].rx_status = cpu_to_le32(DescOwn);
	}
	np->dirty_rx = (unsigned int)(i - RX_RING_SIZE);

	for (i = 0; i < TX_RING_SIZE; i++) {
		np->tx_skbuff[i] = 0;
		np->tx_ring[i].tx_status = 0;
		np->tx_ring[i].desc_length = cpu_to_le32(0x00e08000);
		np->tx_ring[i].next_desc = virt_to_le32desc(&np->tx_ring[i+1]);
		np->tx_buf[i] = 0;		/* Allocated as/if needed. */
	}
	np->tx_ring[i-1].next_desc = virt_to_le32desc(&np->tx_ring[0]);

	return;
}

static int start_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	unsigned entry;

	/* Block a timer-based transmit from overlapping.  This happens when
	   packets are presumed lost, and we use this check the Tx status. */
	if (netif_pause_tx_queue(dev) != 0) {
		/* This watchdog code is redundant with the media monitor timer. */
		if (jiffies - dev->trans_start > TX_TIMEOUT)
			tx_timeout(dev);
		return 1;
	}

	/* Caution: the write order is important here, set the descriptor word
	   with the "ownership" bit last.  No SMP locking is needed if the
	   cur_tx is incremented after the descriptor is consistent.  */

	/* Calculate the next Tx descriptor entry. */
	entry = np->cur_tx % TX_RING_SIZE;

	np->tx_skbuff[entry] = skb;

	if ((np->drv_flags & ReqTxAlign)  && ((long)skb->data & 3)) {
		/* Must use alignment buffer. */
		if (np->tx_buf[entry] == NULL &&
			(np->tx_buf[entry] = kmalloc(PKT_BUF_SZ, GFP_KERNEL)) == NULL)
			return 1;
		memcpy(np->tx_buf[entry], skb->data, skb->len);
		np->tx_ring[entry].addr = virt_to_le32desc(np->tx_buf[entry]);
	} else
		np->tx_ring[entry].addr = virt_to_le32desc(skb->data);
	/* Explicitly flush packet data cache lines here. */

	np->tx_ring[entry].desc_length =
		cpu_to_le32(0x00E08000 | (skb->len >= ETH_ZLEN ? skb->len : ETH_ZLEN));
	np->tx_ring[entry].tx_status = cpu_to_le32(DescOwn);

	np->cur_tx++;

	/* Explicitly flush descriptor cache lines here. */

	/* Wake the potentially-idle transmit channel. */
	writew(CmdTxDemand | np->chip_cmd, dev->base_addr + ChipCmd);

	if (np->cur_tx - np->dirty_tx >= TX_QUEUE_LEN - 1) {
		np->tx_full = 1;
		/* Check for a just-cleared queue. */
		if (np->cur_tx - (volatile unsigned int)np->dirty_tx
			< TX_QUEUE_LEN - 2) {
			np->tx_full = 0;
			netif_unpause_tx_queue(dev);
		} else
			netif_stop_tx_queue(dev);
	} else
		netif_unpause_tx_queue(dev);		/* Typical path */

	dev->trans_start = jiffies;

	if (np->msg_level & NETIF_MSG_TX_QUEUED) {
		printk(KERN_DEBUG "%s: Transmit frame #%d queued in slot %d.\n",
			   dev->name, np->cur_tx, entry);
	}
	return 0;
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void intr_handler(int irq, void *dev_instance, struct pt_regs *rgs)
{
	struct net_device *dev = (struct net_device *)dev_instance;
	struct netdev_private *np = (void *)dev->priv;
	long ioaddr = dev->base_addr;
	int boguscnt = np->max_interrupt_work;

	do {
		u32 intr_status = readw(ioaddr + IntrStatus);

		/* Acknowledge all of the current interrupt sources ASAP. */
		writew(intr_status & 0xffff, ioaddr + IntrStatus);

		if (np->msg_level & NETIF_MSG_INTR)
			printk(KERN_DEBUG "%s: Interrupt, status %4.4x.\n",
				   dev->name, intr_status);

		if (intr_status == 0)
			break;

		if (intr_status & (IntrRxDone | IntrRxErr | IntrRxDropped |
						   IntrRxWakeUp | IntrRxEmpty | IntrRxNoBuf))
			netdev_rx(dev);

		for (; np->cur_tx - np->dirty_tx > 0; np->dirty_tx++) {
			int entry = np->dirty_tx % TX_RING_SIZE;
			int txstatus = le32_to_cpu(np->tx_ring[entry].tx_status);
			if (txstatus & DescOwn)
				break;
			if (np->msg_level & NETIF_MSG_TX_DONE)
				printk(KERN_DEBUG "  Tx scavenge %d status %4.4x.\n",
					   entry, txstatus);
			if (txstatus & 0x8000) {
				if (np->msg_level & NETIF_MSG_TX_ERR)
					printk(KERN_DEBUG "%s: Transmit error, Tx status %4.4x.\n",
						   dev->name, txstatus);
				np->stats.tx_errors++;
				if (txstatus & 0x0400) np->stats.tx_carrier_errors++;
				if (txstatus & 0x0200) np->stats.tx_window_errors++;
				if (txstatus & 0x0100) np->stats.tx_aborted_errors++;
				if (txstatus & 0x0080) np->stats.tx_heartbeat_errors++;
				if (txstatus & 0x0002) np->stats.tx_fifo_errors++;
#ifdef ETHER_STATS
				if (txstatus & 0x0100) np->stats.collisions16++;
#endif
				/* Transmitter restarted in 'abnormal' handler. */
			} else {
#ifdef ETHER_STATS
				if (txstatus & 0x0001) np->stats.tx_deferred++;
#endif
				if (np->drv_flags & HasV1TxStat)
					np->stats.collisions += (txstatus >> 3) & 15;
				else
					np->stats.collisions += txstatus & 15;
#if defined(NETSTATS_VER2)
				np->stats.tx_bytes += np->tx_skbuff[entry]->len;
#endif
				np->stats.tx_packets++;
			}
			/* Free the original skb. */
			dev_free_skb_irq(np->tx_skbuff[entry]);
			np->tx_skbuff[entry] = 0;
		}
		/* Note the 4 slot hysteresis in mark the queue non-full. */
		if (np->tx_full  &&  np->cur_tx - np->dirty_tx < TX_QUEUE_LEN - 4) {
			/* The ring is no longer full, allow new TX entries. */
			np->tx_full = 0;
			netif_resume_tx_queue(dev);
		}

		/* Abnormal error summary/uncommon events handlers. */
		if (intr_status & (IntrPCIErr | IntrLinkChange | IntrMIIChange |
						   IntrStatsMax | IntrTxAbort | IntrTxUnderrun))
			netdev_error(dev, intr_status);

		if (--boguscnt < 0) {
			printk(KERN_WARNING "%s: Too much work at interrupt, "
				   "status=0x%4.4x.\n",
				   dev->name, intr_status);
			break;
		}
	} while (1);

	if (np->msg_level & NETIF_MSG_INTR)
		printk(KERN_DEBUG "%s: exiting interrupt, status=%#4.4x.\n",
			   dev->name, (int)readw(ioaddr + IntrStatus));

	return;
}

/* This routine is logically part of the interrupt handler, but isolated
   for clarity and better register allocation. */
static int netdev_rx(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	int entry = np->cur_rx % RX_RING_SIZE;
	int boguscnt = np->dirty_rx + RX_RING_SIZE - np->cur_rx;

	if (np->msg_level & NETIF_MSG_RX_STATUS) {
		printk(KERN_DEBUG " In netdev_rx(), entry %d status %8.8x.\n",
			   entry, np->rx_head_desc->rx_status);
	}

	/* If EOP is set on the next entry, it's a new packet. Send it up. */
	while ( ! (np->rx_head_desc->rx_status & cpu_to_le32(DescOwn))) {
		struct rx_desc *desc = np->rx_head_desc;
		u32 desc_status = le32_to_cpu(desc->rx_status);
		int data_size = desc_status >> 16;

		if (np->msg_level & NETIF_MSG_RX_STATUS)
			printk(KERN_DEBUG "  netdev_rx() status is %4.4x.\n",
				   desc_status);
		if (--boguscnt < 0)
			break;
		if ( (desc_status & (RxWholePkt | RxErr)) !=  RxWholePkt) {
			if ((desc_status & RxWholePkt) !=  RxWholePkt) {
				printk(KERN_WARNING "%s: Oversized Ethernet frame spanned "
					   "multiple buffers, entry %#x length %d status %4.4x!\n",
					   dev->name, np->cur_rx, data_size, desc_status);
				printk(KERN_WARNING "%s: Oversized Ethernet frame %p vs %p.\n",
					   dev->name, np->rx_head_desc,
					   &np->rx_ring[np->cur_rx % RX_RING_SIZE]);
				np->stats.rx_length_errors++;
			} else if (desc_status & RxErr) {
				/* There was a error. */
				if (np->msg_level & NETIF_MSG_RX_ERR)
					printk(KERN_DEBUG "  netdev_rx() Rx error was %8.8x.\n",
						   desc_status);
				np->stats.rx_errors++;
				if (desc_status & 0x0030) np->stats.rx_length_errors++;
				if (desc_status & 0x0048) np->stats.rx_fifo_errors++;
				if (desc_status & 0x0004) np->stats.rx_frame_errors++;
				if (desc_status & 0x0002) np->stats.rx_crc_errors++;
			}
		} else {
			struct sk_buff *skb;
			/* Length should omit the CRC */
			int pkt_len = data_size - 4;

			/* Check if the packet is long enough to accept without copying
			   to a minimally-sized skbuff. */
			if (pkt_len < np->rx_copybreak
				&& (skb = dev_alloc_skb(pkt_len + 2)) != NULL) {
				skb->dev = dev;
				skb_reserve(skb, 2);	/* 16 byte align the IP header */
#if HAS_IP_COPYSUM			/* Call copy + cksum if available. */
				eth_copy_and_sum(skb, np->rx_skbuff[entry]->tail, pkt_len, 0);
				skb_put(skb, pkt_len);
#else
				memcpy(skb_put(skb, pkt_len), np->rx_skbuff[entry]->tail,
					   pkt_len);
#endif
			} else {
				skb_put(skb = np->rx_skbuff[entry], pkt_len);
				np->rx_skbuff[entry] = NULL;
			}
			skb->protocol = eth_type_trans(skb, dev);
			{					/* Use hardware checksum info. */
				int rxtype = le32_to_cpu(desc->desc_length);
				int csum_bits = rxtype & RxTypeCsumMask;
				if (csum_bits == RxTypeUDPSumOK ||
					csum_bits == RxTypeTCPSumOK)
					skb->ip_summed = CHECKSUM_UNNECESSARY;
			}
			netif_rx(skb);
			dev->last_rx = jiffies;
#if defined(NETSTATS_VER2)
			np->stats.rx_bytes += pkt_len;
#endif
			np->stats.rx_packets++;
		}
		entry = (++np->cur_rx) % RX_RING_SIZE;
		np->rx_head_desc = &np->rx_ring[entry];
	}

	/* Refill the Rx ring buffers. */
	for (; np->cur_rx - np->dirty_rx > 0; np->dirty_rx++) {
		struct sk_buff *skb;
		entry = np->dirty_rx % RX_RING_SIZE;
		if (np->rx_skbuff[entry] == NULL) {
			skb = dev_alloc_skb(np->rx_buf_sz);
			np->rx_skbuff[entry] = skb;
			if (skb == NULL)
				break;			/* Better luck next round. */
			skb->dev = dev;			/* Mark as being used by this device. */
			np->rx_ring[entry].addr = virt_to_le32desc(skb->tail);
		}
		np->rx_ring[entry].rx_status = cpu_to_le32(DescOwn);
	}

	/* Pre-emptively restart Rx engine. */
	writew(CmdRxDemand | np->chip_cmd, dev->base_addr + ChipCmd);
	return 0;
}

static void netdev_error(struct net_device *dev, int intr_status)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;

	if (intr_status & (IntrMIIChange | IntrLinkChange)) {
		if (readb(ioaddr + MIIStatus) & 0x02) {
			/* Link failed, restart autonegotiation. */
			if (np->drv_flags & HasDavicomPhy)
				mdio_write(dev, np->phys[0], 0, 0x3300);
			netif_link_down(dev);
		} else {
			netif_link_up(dev);
			check_duplex(dev);
		}
		if (np->msg_level & NETIF_MSG_LINK)
			printk(KERN_ERR "%s: MII status changed: Autonegotiation "
				   "advertising %4.4x  partner %4.4x.\n", dev->name,
			   mdio_read(dev, np->phys[0], 4),
			   mdio_read(dev, np->phys[0], 5));
	}
	if (intr_status & IntrStatsMax) {
		np->stats.rx_crc_errors	+= readw(ioaddr + RxCRCErrs);
		np->stats.rx_missed_errors	+= readw(ioaddr + RxMissed);
		writel(0, ioaddr + RxMissed);
	}
	if (intr_status & IntrTxAbort) {
		/* Stats counted in Tx-done handler, just restart Tx. */
		writel(virt_to_bus(&np->tx_ring[np->dirty_tx % TX_RING_SIZE]),
			   ioaddr + TxRingPtr);
		writew(CmdTxDemand | np->chip_cmd, dev->base_addr + ChipCmd);
	}
	if (intr_status & IntrTxUnderrun) {
		if (np->tx_thresh < 0xE0)
			writeb(np->tx_thresh += 0x20, ioaddr + TxConfig);
		if (np->msg_level & NETIF_MSG_TX_ERR)
			printk(KERN_INFO "%s: Transmitter underrun, increasing Tx "
				   "threshold setting to %2.2x.\n", dev->name, np->tx_thresh);
	}
	if ((intr_status & ~(IntrLinkChange | IntrMIIChange | IntrStatsMax |
						 IntrTxAbort|IntrTxAborted | IntrNormalSummary))
		 && (np->msg_level & NETIF_MSG_DRV)) {
		printk(KERN_ERR "%s: Something Wicked happened! %4.4x.\n",
			   dev->name, intr_status);
		/* Recovery for other fault sources not known. */
		writew(CmdTxDemand | np->chip_cmd, dev->base_addr + ChipCmd);
	}
}

static struct net_device_stats *get_stats(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;

	/* Nominally we should lock this segment of code for SMP, although
	   the vulnerability window is very small and statistics are
	   non-critical. */
	np->stats.rx_crc_errors	+= readw(ioaddr + RxCRCErrs);
	np->stats.rx_missed_errors	+= readw(ioaddr + RxMissed);
	writel(0, ioaddr + RxMissed);

	return &np->stats;
}

/* The big-endian AUTODIN II ethernet CRC calculation.
   N.B. Do not use for bulk data, use a table-based routine instead.
   This is common code and should be moved to net/core/crc.c */
static unsigned const ethernet_polynomial = 0x04c11db7U;
static inline u32 ether_crc(int length, unsigned char *data)
{
	int crc = -1;

	while(--length >= 0) {
		unsigned char current_octet = *data++;
		int bit;
		for (bit = 0; bit < 8; bit++, current_octet >>= 1) {
			crc = (crc << 1) ^
				((crc < 0) ^ (current_octet & 1) ? ethernet_polynomial : 0);
		}
	}
	return crc;
}

static void set_rx_mode(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	u32 mc_filter[2];			/* Multicast hash filter */
	u8 rx_mode;					/* Note: 0x02=accept runt, 0x01=accept errs */

	if (dev->flags & IFF_PROMISC) {			/* Set promiscuous. */
		/* Unconditionally log net taps. */
		printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n", dev->name);
		rx_mode = 0x1C;
	} else if ((dev->mc_count > np->multicast_filter_limit)
			   ||  (dev->flags & IFF_ALLMULTI)) {
		/* Too many to match, or accept all multicasts. */
		writel(0xffffffff, ioaddr + MulticastFilter0);
		writel(0xffffffff, ioaddr + MulticastFilter1);
		rx_mode = 0x0C;
	} else {
		struct dev_mc_list *mclist;
		int i;
		memset(mc_filter, 0, sizeof(mc_filter));
		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
			 i++, mclist = mclist->next) {
			set_bit(ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26,
					mc_filter);
		}
		writel(mc_filter[0], ioaddr + MulticastFilter0);
		writel(mc_filter[1], ioaddr + MulticastFilter1);
		rx_mode = 0x0C;
	}
	writeb(np->rx_thresh | rx_mode, ioaddr + RxConfig);
}

static int mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	u16 *data = (u16 *)&rq->ifr_data;
	u32 *data32 = (void *)&rq->ifr_data;

	switch(cmd) {
	case 0x8947: case 0x89F0:
		/* SIOCGMIIPHY: Get the address of the PHY in use. */
		data[0] = np->phys[0] & 0x1f;
		/* Fall Through */
	case 0x8948: case 0x89F1:
		/* SIOCGMIIREG: Read the specified MII register. */
		data[3] = mdio_read(dev, data[0] & 0x1f, data[1] & 0x1f);
		return 0;
	case 0x8949: case 0x89F2:
		/* SIOCSMIIREG: Write the specified MII register */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		/* Note: forced media tracking is done in mdio_write(). */
		mdio_write(dev, data[0] & 0x1f, data[1] & 0x1f, data[2]);
		return 0;
	case SIOCGPARAMS:
		data32[0] = np->msg_level;
		data32[1] = np->multicast_filter_limit;
		data32[2] = np->max_interrupt_work;
		data32[3] = np->rx_copybreak;
		return 0;
	case SIOCSPARAMS:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		np->msg_level = data32[0];
		np->multicast_filter_limit = data32[1];
		np->max_interrupt_work = data32[2];
		np->rx_copybreak = data32[3];
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

static int netdev_close(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	int i;

	netif_stop_tx_queue(dev);

	if (np->msg_level & NETIF_MSG_IFDOWN)
		printk(KERN_DEBUG "%s: Shutting down ethercard, status was %4.4x.\n",
			   dev->name, readw(ioaddr + ChipCmd));

	/* Switch to loopback mode to avoid hardware races. */
	writeb(np->tx_thresh | 0x01, ioaddr + TxConfig);

	/* Disable interrupts by clearing the interrupt mask. */
	writew(0x0000, ioaddr + IntrEnable);

	/* Stop the chip's Tx and Rx processes. */
	np->chip_cmd = CmdStop;
	writew(CmdStop, ioaddr + ChipCmd);

	del_timer(&np->timer);

	free_irq(dev->irq, dev);

	/* Free all the skbuffs in the Rx queue. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].rx_status = 0;
		np->rx_ring[i].addr = 0xBADF00D0; /* An invalid address. */
		if (np->rx_skbuff[i]) {
#if LINUX_VERSION_CODE < 0x20100
			np->rx_skbuff[i]->free = 1;
#endif
			dev_free_skb(np->rx_skbuff[i]);
		}
		np->rx_skbuff[i] = 0;
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		if (np->tx_skbuff[i])
			dev_free_skb(np->tx_skbuff[i]);
		np->tx_skbuff[i] = 0;
		if (np->tx_buf[i]) {
			kfree(np->tx_buf[i]);
			np->tx_buf[i] = 0;
		}
	}

	MOD_DEC_USE_COUNT;

	return 0;
}

static int via_pwr_event(void *dev_instance, int event)
{
	struct net_device *dev = dev_instance;
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;

	if (np->msg_level & NETIF_MSG_LINK)
		printk(KERN_DEBUG "%s: Handling power event %d.\n", dev->name, event);
	switch(event) {
	case DRV_ATTACH:
		MOD_INC_USE_COUNT;
		break;
	case DRV_SUSPEND:
		/* Disable interrupts, stop Tx and Rx. */
		writew(0x0000, ioaddr + IntrEnable);
		/* Stop the chip's Tx and Rx processes. */
		writew(CmdStop, ioaddr + ChipCmd);
		break;
	case DRV_RESUME:
		/* This is incomplete: the actions are very chip specific. */
		set_rx_mode(dev);
		netif_start_tx_queue(dev);
		writew(np->chip_cmd, ioaddr + ChipCmd);
		writew(np->intr_enable, ioaddr + IntrEnable);
		break;
	case DRV_DETACH: {
		struct net_device **devp, **next;
		if (dev->flags & IFF_UP) {
			/* Some, but not all, kernel versions close automatically. */
			dev_close(dev);
			dev->flags &= ~(IFF_UP|IFF_RUNNING);
		}
		unregister_netdev(dev);
		release_region(dev->base_addr, pci_tbl[np->chip_id].io_size);
#ifndef USE_IO_OPS
		iounmap((char *)dev->base_addr);
#endif
		for (devp = &root_net_dev; *devp; devp = next) {
			next = &((struct netdev_private *)(*devp)->priv)->next_module;
			if (*devp == dev) {
				*devp = *next;
				break;
			}
		}
		if (np->priv_addr)
			kfree(np->priv_addr);
		kfree(dev);
		MOD_DEC_USE_COUNT;
		break;
	}
	}

	return 0;
}


#ifdef MODULE
int init_module(void)
{
	if (debug >= NETIF_MSG_DRV)	/* Emit version even if no cards detected. */
		printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	return pci_drv_register(&via_rhine_drv_id, NULL);
}

void cleanup_module(void)
{
	struct net_device *next_dev;

	pci_drv_unregister(&via_rhine_drv_id);

	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	while (root_net_dev) {
		struct netdev_private *np = (void *)(root_net_dev->priv);
		unregister_netdev(root_net_dev);
#ifdef USE_IO_OPS
		release_region(root_net_dev->base_addr, pci_tbl[np->chip_id].io_size);
#else
		iounmap((char *)(root_net_dev->base_addr));
#endif
		next_dev = np->next_module;
		if (np->priv_addr)
			kfree(np->priv_addr);
		kfree(root_net_dev);
		root_net_dev = next_dev;
	}
}

#endif  /* MODULE */

/*
 * Local variables:
 *  compile-command: "make KERNVER=`uname -r` via-rhine.o"
 *  compile-cmd: "gcc -DMODULE -Wall -Wstrict-prototypes -O6 -c via-rhine.c"
 *  simple-compile-command: "gcc -DMODULE -O6 -c via-rhine.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
