/* sundance.c: A Linux device driver for the Sundance ST201 "Alta". */
/*
	Written 1999-2003 by Donald Becker.

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Support information and updates available at
	http://www.scyld.com/network/sundance.html
*/

/* These identify the driver base version and may not be removed. */
static const char version1[] =
"sundance.c:v1.11 2/4/2003  Written by Donald Becker <becker@scyld.com>\n";
static const char version2[] =
"  http://www.scyld.com/network/sundance.html\n";
/* Updated to recommendations in pci-skeleton v2.12. */

/* Automatically extracted configuration info:
probe-func: sundance_probe
config-in: tristate 'Sundance ST201 "Alta" PCI Ethernet support' CONFIG_SUNDANCE
c-help-name: Sundance ST201 "Alta" PCI Ethernet support
c-help-symbol: CONFIG_SUNDANCE
c-help: This driver is for the Sundance ST201 "Alta" and Kendin KS8723, as
c-help: used on the D-Link DFE-550 and DFE-580.
c-help: Design information, usage details and updates are available from
c-help: http://www.scyld.com/network/sundance.html
*/

/* The user-configurable values.
   These may be modified when a driver module is loaded.*/

/* Message enable level: 0..31 = no..all messages.  See NETIF_MSG docs. */
static int debug = 2;

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;

/* Maximum number of multicast addresses to filter (vs. rx-all-multicast).
   The sundance uses a 64 element hash table based on the Ethernet CRC.  */
static int multicast_filter_limit = 32;

/* Set the copy breakpoint for the copy-only-tiny-frames scheme.
   Setting to > 1518 effectively disables this feature.
   This chip can receive into any byte alignment buffers, so word-oriented
   archs do not need a copy-align of the IP header. */
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

/* Operational parameters that are set at compile time. */

/* Ring sizes are a power of two only for compile efficiency.
   The compiler will convert <unsigned>'%'<2^N> into a bit mask.
   There must be at least five Tx entries for the tx_full hysteresis, and
   more than 31 requires modifying the Tx status handling error recovery.
   Leave a inactive gap in the Tx ring for better cache behavior.
   Making the Tx ring too large decreases the effectiveness of channel
   bonding and packet priority.
   Large receive rings waste memory and impact buffer accounting.
   The driver need to protect against interrupt latency and the kernel
   not reserving enough available memory.
*/
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

/* Set iff a MII transceiver on any interface requires mdio preamble.
   This only set with older tranceivers, so the extra
   code size of a per-interface flag is not worthwhile. */
static char mii_preamble_required = 0;

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
#include <asm/bitops.h>
#include <asm/io.h>

#if LINUX_VERSION_CODE >= 0x20300
#include <linux/spinlock.h>
#elif LINUX_VERSION_CODE >= 0x20200
#include <asm/spinlock.h>
#endif

#ifdef INLINE_PCISCAN
#include "k_compat.h"
#else
#include "pci-scan.h"
#include "kern_compat.h"
#endif

/* Condensed operations for readability. */
#define virt_to_le32desc(addr)  cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr)  bus_to_virt(le32_to_cpu(addr))

#if (LINUX_VERSION_CODE >= 0x20100)  &&  defined(MODULE)
char kernel_version[] = UTS_RELEASE;
#endif

MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("Sundance Alta Ethernet driver");
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
MODULE_PARM_DESC(full_duplex,
				 "Non-zero to set forced full duplex (deprecated).");
MODULE_PARM_DESC(rx_copybreak,
				 "Breakpoint in bytes for copy-only-tiny-frames");
MODULE_PARM_DESC(multicast_filter_limit,
				 "Multicast addresses before switching to Rx-all-multicast");

/*
				Theory of Operation

I. Board Compatibility

This driver is designed for the Sundance Technologies "Alta" ST201 chip.
The Kendin KS8723 is the same design with an integrated transceiver and
new quirks.

II. Board-specific settings

This is an all-in-one chip, so there are no board-specific settings.

III. Driver operation

IIIa. Ring buffers

This driver uses two statically allocated fixed-size descriptor lists
formed into rings by a branch from the final descriptor to the beginning of
the list.  The ring sizes are set at compile time by RX/TX_RING_SIZE.
Some chips explicitly use only 2^N sized rings, while others use a
'next descriptor' pointer that the driver forms into rings.

IIIb/c. Transmit/Receive Structure

This driver uses a zero-copy receive and transmit scheme.
The driver allocates full frame size skbuffs for the Rx ring buffers at
open() time and passes the skb->data field to the chip as receive data
buffers.  When an incoming frame is less than RX_COPYBREAK bytes long,
a fresh skbuff is allocated and the frame is copied to the new skbuff.
When the incoming frame is larger, the skbuff is passed directly up the
protocol stack.  Buffers consumed this way are replaced by newly allocated
skbuffs in a later phase of receives.

The RX_COPYBREAK value is chosen to trade-off the memory wasted by
using a full-sized skbuff for small frames vs. the copying costs of larger
frames.  New boards are typically used in generously configured machines
and the underfilled buffers have negligible impact compared to the benefit of
a single allocation size, so the default value of zero results in never
copying packets.  When copying is done, the cost is usually mitigated by using
a combined copy/checksum routine.  Copying also preloads the cache, which is
most useful with small frames.

A subtle aspect of the operation is that the IP header at offset 14 in an
ethernet frame isn't longword aligned for further processing.
Unaligned buffers are permitted by the Sundance hardware, so
frames are received into the skbuff at an offset of "+2", 16-byte aligning
the IP header.

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

The Sundance ST201 datasheet, preliminary version.
The Kendin KS8723 datasheet, preliminary version.
http://www.scyld.com/expert/100mbps.html
http://www.scyld.com/expert/NWay.html

IVc. Errata

*/



/* Work-around for Kendin chip bugs.  This will be reversed after tracking
 down all of the chip access quirks in memory mode. */
#ifndef USE_MEM_OPS
#define USE_IO_OPS 1
#endif

static void *sundance_probe1(struct pci_dev *pdev, void *init_dev,
							 long ioaddr, int irq, int chip_idx, int find_cnt);
static int sundance_pwr_event(void *dev_instance, int event);

enum chip_capability_flags {CanHaveMII=1, KendinPktDropBug=2, };
#ifdef USE_IO_OPS
#define PCI_IOTYPE (PCI_USES_MASTER | PCI_USES_IO  | PCI_ADDR0)
#else
#define PCI_IOTYPE (PCI_USES_MASTER | PCI_USES_MEM | PCI_ADDR1)
#endif

static struct pci_id_info pci_id_tbl[] = {
	{"D-Link DFE-580TX (Kendin/Sundance ST201 Alta)",
	 {0x10021186, 0xffffffff, 0x10121186, 0xffffffff, 0x14, 0xff},
	 PCI_IOTYPE, 128, CanHaveMII|KendinPktDropBug},
	{"D-Link DFE-580TX (Sundance ST201)",
	 {0x10021186, 0xffffffff, 0x10121186, 0xffffffff, },
	 PCI_IOTYPE, 128, CanHaveMII|KendinPktDropBug},
	{"D-Link DFE-550FX 100baseFx (Sundance ST201)",
	 {0x10031186, 0xffffffff, },
	 PCI_IOTYPE, 128, CanHaveMII|KendinPktDropBug},
	{"OEM Sundance Technology ST201", {0x10021186, 0xffffffff, },
	 PCI_IOTYPE, 128, CanHaveMII},
	{"Sundance Technology Alta", {0x020113F0, 0xffffffff, },
	 PCI_IOTYPE, 128, CanHaveMII},
	{0,},						/* 0 terminated list. */
};

struct drv_id_info sundance_drv_id = {
	"sundance", PCI_HOTSWAP, PCI_CLASS_NETWORK_ETHERNET<<8, pci_id_tbl,
	sundance_probe1, sundance_pwr_event };

/* This driver was written to use PCI memory space, however x86-oriented
   hardware often uses I/O space accesses. */
#ifdef USE_IO_OPS
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

/* Offsets to the device registers.
   Unlike software-only systems, device drivers interact with complex hardware.
   It's not useful to define symbolic names for every register bit in the
   device.  The name can only partially document the semantics and make
   the driver longer and more difficult to read.
   In general, only the important configuration values or bits changed
   multiple times should be defined symbolically.
*/
enum alta_offsets {
	DMACtrl=0x00,     TxListPtr=0x04, TxDMACtrl=0x08, TxDescPoll=0x0a,
	RxDMAStatus=0x0c, RxListPtr=0x10, RxDMACtrl=0x14, RxDescPoll=0x16,
	LEDCtrl=0x1a, ASICCtrl=0x30,
	EEData=0x34, EECtrl=0x36, TxThreshold=0x3c,
	FlashAddr=0x40, FlashData=0x44, WakeEvent=0x45, TxStatus=0x46,
	DownCounter=0x48, IntrClear=0x4a, IntrEnable=0x4c, IntrStatus=0x4e,
	MACCtrl0=0x50, MACCtrl1=0x52, StationAddr=0x54,
	MaxFrameSize=0x5A, RxMode=0x5c, MIICtrl=0x5e,
	MulticastFilter0=0x60, MulticastFilter1=0x64,
	RxOctetsLow=0x68, RxOctetsHigh=0x6a, TxOctetsLow=0x6c, TxOctetsHigh=0x6e,
	TxFramesOK=0x70, RxFramesOK=0x72, StatsCarrierError=0x74,
	StatsLateColl=0x75, StatsMultiColl=0x76, StatsOneColl=0x77,
	StatsTxDefer=0x78, RxMissed=0x79, StatsTxXSDefer=0x7a, StatsTxAbort=0x7b,
	StatsBcastTx=0x7c, StatsBcastRx=0x7d, StatsMcastTx=0x7e, StatsMcastRx=0x7f,
	/* Aliased and bogus values! */
	RxStatus=0x0c,
};

/* Bits in the interrupt status/mask registers. */
enum intr_status_bits {
	IntrSummary=0x0001, IntrPCIErr=0x0002, IntrMACCtrl=0x0008,
	IntrTxDone=0x0004, IntrRxDone=0x0010, IntrRxStart=0x0020,
	IntrDrvRqst=0x0040,
	StatsMax=0x0080, LinkChange=0x0100,
	IntrTxDMADone=0x0200, IntrRxDMADone=0x0400,
};

/* Bits in the RxMode register. */
enum rx_mode_bits {
	AcceptAllIPMulti=0x20, AcceptMultiHash=0x10, AcceptAll=0x08,
	AcceptBroadcast=0x04, AcceptMulticast=0x02, AcceptMyPhys=0x01,
};
/* Bits in MACCtrl. */
enum mac_ctrl0_bits {
	EnbFullDuplex=0x20, EnbRcvLargeFrame=0x40,
	EnbFlowCtrl=0x100, EnbPassRxCRC=0x200,
};
enum mac_ctrl1_bits {
	StatsEnable=0x0020,	StatsDisable=0x0040, StatsEnabled=0x0080,
	TxEnable=0x0100, TxDisable=0x0200, TxEnabled=0x0400,
	RxEnable=0x0800, RxDisable=0x1000, RxEnabled=0x2000,
};

/* The Rx and Tx buffer descriptors.
   Using only 32 bit fields simplifies software endian correction.
   This structure must be aligned, and should avoid spanning cache lines.
*/
struct netdev_desc {
	u32 next_desc;
	u32 status;
	struct desc_frag { u32 addr, length; } frag[1];
};

/* Bits in netdev_desc.status */
enum desc_status_bits {
	DescOwn=0x8000, DescEndPacket=0x4000, DescEndRing=0x2000,
	DescTxDMADone=0x10000,
	LastFrag=0x80000000, DescIntrOnTx=0x8000, DescIntrOnDMADone=0x80000000,
};

#define PRIV_ALIGN	15 	/* Required alignment mask */
/* Use  __attribute__((aligned (L1_CACHE_BYTES)))  to maintain alignment
   within the structure. */
struct netdev_private {
	/* Descriptor rings first for alignment. */
	struct netdev_desc rx_ring[RX_RING_SIZE];
	struct netdev_desc tx_ring[TX_RING_SIZE];
	struct net_device *next_module;		/* Link for devices of this type. */
	void *priv_addr;					/* Unaligned address for kfree */
	const char *product_name;
	/* The addresses of receive-in-place skbuffs. */
	struct sk_buff* rx_skbuff[RX_RING_SIZE];
	/* The saved address of a sent-in-place packet/buffer, for later free(). */
	struct sk_buff* tx_skbuff[TX_RING_SIZE];
	struct net_device_stats stats;
	struct timer_list timer;	/* Media monitoring timer. */
	/* Frequently used values: keep some adjacent for cache effect. */
	int msg_level;
	int chip_id, drv_flags;
	struct pci_dev *pci_dev;
	int max_interrupt_work;

	/* Note: Group variables for cache line effect. */
	struct netdev_desc *rx_head_desc;
	unsigned int cur_rx, dirty_rx;		/* Producer/consumer ring indices */
	unsigned int rx_buf_sz;				/* Based on MTU+slack. */
	int rx_copybreak;

	spinlock_t txlock;					/* Group with Tx control cache line. */
	struct netdev_desc *last_tx;		/* Last Tx descriptor used. */
	unsigned int cur_tx, dirty_tx;
	unsigned int tx_full:1;				/* The Tx queue is full. */

	/* These values keep track of the transceiver/media in use. */
	unsigned int full_duplex:1;			/* Full-duplex operation requested. */
	unsigned int duplex_lock:1;
	unsigned int medialock:1;			/* Do not sense media. */
	unsigned int default_port;			/* Last dev->if_port value. */
	/* Multicast and receive mode. */
	spinlock_t mcastlock;				/* SMP lock multicast updates. */
	u16 mcast_filter[4];
	int multicast_filter_limit;

	/* MII transceiver section. */
	int mii_cnt;						/* MII device addresses. */
	int link_status;
	u16 advertising;					/* NWay media advertisement */
	unsigned char phys[2];				/* MII device addresses. */
};

/* The station address location in the EEPROM. */
#define EEPROM_SA_OFFSET	0x10

static int  eeprom_read(long ioaddr, int location);
static int  mdio_read(struct net_device *dev, int phy_id,
					  unsigned int location);
static void mdio_write(struct net_device *dev, int phy_id,
					   unsigned int location, int value);
static int  netdev_open(struct net_device *dev);
static void sundance_start(struct net_device *dev);
static int  change_mtu(struct net_device *dev, int new_mtu);
static void check_duplex(struct net_device *dev);
static void netdev_timer(unsigned long data);
static void tx_timeout(struct net_device *dev);
static void init_ring(struct net_device *dev);
static int  start_tx(struct sk_buff *skb, struct net_device *dev);
static void intr_handler(int irq, void *dev_instance, struct pt_regs *regs);
static void netdev_error(struct net_device *dev, int intr_status);
static int  netdev_rx(struct net_device *dev);
static void netdev_error(struct net_device *dev, int intr_status);
static void set_rx_mode(struct net_device *dev);
static struct net_device_stats *get_stats(struct net_device *dev);
static int mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static int  netdev_close(struct net_device *dev);



/* A list of our installed devices, for removing the driver module. */
static struct net_device *root_net_dev = NULL;

#ifndef MODULE
int sundance_probe(struct net_device *dev)
{
	if (pci_drv_register(&sundance_drv_id, dev) < 0)
		return -ENODEV;
	if (debug >= NETIF_MSG_DRV)	/* Emit version even if no cards detected. */
		printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	return 0;
}
#endif

static void *sundance_probe1(struct pci_dev *pdev, void *init_dev,
							 long ioaddr, int irq, int chip_idx, int card_idx)
{
	struct net_device *dev;
	struct netdev_private *np;
	void *priv_mem;
	int i, option = card_idx < MAX_UNITS ? options[card_idx] : 0;

	dev = init_etherdev(init_dev, 0);
	if (!dev)
		return NULL;

	/* Perhaps NETIF_MSG_PROBE */
	printk(KERN_INFO "%s: %s at 0x%lx, ",
		   dev->name, pci_id_tbl[chip_idx].name, ioaddr);

	for (i = 0; i < 3; i++)
		((u16 *)dev->dev_addr)[i] =
			le16_to_cpu(eeprom_read(ioaddr, i + EEPROM_SA_OFFSET));
	for (i = 0; i < 5; i++)
		printk("%2.2x:", dev->dev_addr[i]);
	printk("%2.2x, IRQ %d.\n", dev->dev_addr[i], irq);

	/* Make certain elements e.g. descriptor lists are aligned. */
	priv_mem = kmalloc(sizeof(*np) + PRIV_ALIGN, GFP_KERNEL);
	/* Check for the very unlikely case of no memory. */
	if (priv_mem == NULL)
		return NULL;

	/* All failure checks before this point.
	   We do a request_region() only to register /proc/ioports info. */
#ifdef USE_IO_OPS
	request_region(ioaddr, pci_id_tbl[chip_idx].io_size, dev->name);
#endif

	dev->base_addr = ioaddr;
	dev->irq = irq;

	dev->priv = np = (void *)(((long)priv_mem + PRIV_ALIGN) & ~PRIV_ALIGN);
	memset(np, 0, sizeof(*np));
	np->priv_addr = priv_mem;

	np->next_module = root_net_dev;
	root_net_dev = dev;

	np->pci_dev = pdev;
	np->chip_id = chip_idx;
	np->drv_flags = pci_id_tbl[chip_idx].drv_flags;
	np->msg_level = (1 << debug) - 1;
	np->rx_copybreak = rx_copybreak;
	np->max_interrupt_work = max_interrupt_work;
	np->multicast_filter_limit = multicast_filter_limit;

	if (dev->mem_start)
		option = dev->mem_start;

	if (card_idx < MAX_UNITS  &&  full_duplex[card_idx] > 0)
		np->full_duplex = 1;

	if (np->full_duplex)
		np->medialock = 1;

	/* The chip-specific entries in the device structure. */
	dev->open = &netdev_open;
	dev->hard_start_xmit = &start_tx;
	dev->stop = &netdev_close;
	dev->get_stats = &get_stats;
	dev->set_multicast_list = &set_rx_mode;
	dev->do_ioctl = &mii_ioctl;
	dev->change_mtu = &change_mtu;

	if (1) {
		int phy, phy_idx = 0;
		np->phys[0] = 1;		/* Default setting */
		mii_preamble_required++;
		for (phy = 1; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(dev, phy, 1);
			if (mii_status != 0xffff  &&  mii_status != 0x0000) {
				np->phys[phy_idx++] = phy;
				np->advertising = mdio_read(dev, phy, 4);
				if ((mii_status & 0x0040) == 0)
					mii_preamble_required++;
				if (np->msg_level & NETIF_MSG_PROBE)
					printk(KERN_INFO "%s: MII PHY found at address %d, status "
						   "0x%4.4x advertising %4.4x.\n",
						   dev->name, phy, mii_status, np->advertising);
			}
		}
		mii_preamble_required--;
		np->mii_cnt = phy_idx;
		if (phy_idx == 0)
			printk(KERN_INFO "%s: No MII transceiver found!, ASIC status %x\n",
				   dev->name, (int)readl(ioaddr + ASICCtrl));
	}

	/* Allow forcing the media type. */
	if (option > 0) {
		if (option & 0x220)
			np->full_duplex = 1;
		np->default_port = option & 0x3ff;
		if (np->default_port & 0x330) {
			np->medialock = 1;
			if (np->msg_level & NETIF_MSG_PROBE)
				printk(KERN_INFO "  Forcing %dMbs %s-duplex operation.\n",
					   (option & 0x300 ? 100 : 10),
					   (np->full_duplex ? "full" : "half"));
			if (np->mii_cnt)
				mdio_write(dev, np->phys[0], 0,
						   ((option & 0x300) ? 0x2000 : 0) | 	/* 100mbps? */
						   (np->full_duplex ? 0x0100 : 0)); /* Full duplex? */
		}
	}

	/* Reset the chip to erase previous misconfiguration. */
	if (np->msg_level & NETIF_MSG_MISC)
		printk("ASIC Control is %x.\n", (int)readl(ioaddr + ASICCtrl));
	writel(0x007f0000 | readl(ioaddr + ASICCtrl), ioaddr + ASICCtrl);
	if (np->msg_level & NETIF_MSG_MISC)
		printk("ASIC Control is now %x.\n", (int)readl(ioaddr + ASICCtrl));

	return dev;
}



static int change_mtu(struct net_device *dev, int new_mtu)
{
	if ((new_mtu < 68) || (new_mtu > 8191)) /* Limited by RxDMAFrameLen */
		return -EINVAL;
	if (netif_running(dev))
		return -EBUSY;
	dev->mtu = new_mtu;
	return 0;
}

/* Read the EEPROM and MII Management Data I/O (MDIO) interfaces. */
static int eeprom_read(long ioaddr, int location)
{
	int boguscnt = 2000;		/* Typical 190 ticks. */
	writew(0x0200 | (location & 0xff), ioaddr + EECtrl);
	do {
		if (! (readw(ioaddr + EECtrl) & 0x8000)) {
			return readw(ioaddr + EEData);
		}
	} while (--boguscnt > 0);
	return 0;
}

/*  MII transceiver control section.
	Read and write the MII registers using software-generated serial
	MDIO protocol.  See the MII specifications or DP83840A data sheet
	for details.

	The maximum data clock rate is 2.5 Mhz.
	The timing is decoupled from the processor clock by flushing the write
	from the CPU write buffer with a following read, and using PCI
	transaction time. */
#define mdio_in(mdio_addr) readb(mdio_addr)
#define mdio_out(value, mdio_addr) writeb(value, mdio_addr)
#define mdio_delay(mdio_addr) readb(mdio_addr)

enum mii_reg_bits {
	MDIO_ShiftClk=0x0001, MDIO_Data=0x0002, MDIO_EnbOutput=0x0004,
};
#define MDIO_EnbIn  (0)
#define MDIO_WRITE0 (MDIO_EnbOutput)
#define MDIO_WRITE1 (MDIO_Data | MDIO_EnbOutput)

/* Generate the preamble required for initial synchronization and
   a few older transceivers. */
static void mdio_sync(long mdio_addr)
{
	int bits = 32;

	/* Establish sync by sending at least 32 logic ones. */
	while (--bits >= 0) {
		mdio_out(MDIO_WRITE1, mdio_addr);
		mdio_delay(mdio_addr);
		mdio_out(MDIO_WRITE1 | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
}

static int mdio_read(struct net_device *dev, int phy_id, unsigned int location)
{
	long mdio_addr = dev->base_addr + MIICtrl;
	int mii_cmd = (0xf6 << 10) | (phy_id << 5) | location;
	int i, retval = 0;

	if (mii_preamble_required)
		mdio_sync(mdio_addr);

	/* Shift the read command bits out. */
	for (i = 15; i >= 0; i--) {
		int dataval = (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;

		mdio_out(dataval, mdio_addr);
		mdio_delay(mdio_addr);
		mdio_out(dataval | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
	/* Read the two transition, 16 data, and wire-idle bits. */
	for (i = 19; i > 0; i--) {
		mdio_out(MDIO_EnbIn, mdio_addr);
		mdio_delay(mdio_addr);
		retval = (retval << 1) | ((mdio_in(mdio_addr) & MDIO_Data) ? 1 : 0);
		mdio_out(MDIO_EnbIn | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
	return (retval>>1) & 0xffff;
}

static void mdio_write(struct net_device *dev, int phy_id,
					   unsigned int location, int value)
{
	long mdio_addr = dev->base_addr + MIICtrl;
	int mii_cmd = (0x5002 << 16) | (phy_id << 23) | (location<<18) | value;
	int i;

	if (mii_preamble_required)
		mdio_sync(mdio_addr);

	/* Shift the command bits out. */
	for (i = 31; i >= 0; i--) {
		int dataval = (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;

		mdio_out(dataval, mdio_addr);
		mdio_delay(mdio_addr);
		mdio_out(dataval | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
	/* Clear out extra bits. */
	for (i = 2; i > 0; i--) {
		mdio_out(MDIO_EnbIn, mdio_addr);
		mdio_delay(mdio_addr);
		mdio_out(MDIO_EnbIn | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
	return;
}


static int netdev_open(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;

	MOD_INC_USE_COUNT;

	if (request_irq(dev->irq, &intr_handler, SA_SHIRQ, dev->name, dev)) {
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}

	if (np->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: netdev_open() irq %d.\n",
			   dev->name, dev->irq);

	init_ring(dev);

	if (dev->if_port == 0)
		dev->if_port = np->default_port;

	np->full_duplex = np->duplex_lock;
	np->mcastlock = (spinlock_t) SPIN_LOCK_UNLOCKED;

	sundance_start(dev);
	netif_start_tx_queue(dev);

	if (np->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: Done netdev_open(), status: Rx %x Tx %x "
			   "MAC Control %x, %4.4x %4.4x.\n",
			   dev->name, (int)readl(ioaddr + RxStatus),
			   (int)readw(ioaddr + TxStatus), (int)readl(ioaddr + MACCtrl0),
			   (int)readw(ioaddr + MACCtrl1), (int)readw(ioaddr + MACCtrl0));

	/* Set the timer to check for link beat. */
	init_timer(&np->timer);
	np->timer.expires = jiffies + 3*HZ;
	np->timer.data = (unsigned long)dev;
	np->timer.function = &netdev_timer;				/* timer handler */
	add_timer(&np->timer);

	return 0;
}

static void sundance_start(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int i;

	/* No reports have indicated that we need to reset the chip. */

	writel(virt_to_bus(&np->rx_ring[np->cur_rx % RX_RING_SIZE]),
		   ioaddr + RxListPtr);
	/* The Tx list pointer is written as packets are queued. */

	/* Station address must be written as 16 bit words with the Kendin chip. */
	for (i = 0; i < 6; i += 2)
		writew((dev->dev_addr[i + 1] << 8) + dev->dev_addr[i],
			   ioaddr + StationAddr + i);

	np->link_status = readb(ioaddr + MIICtrl) & 0xE0;
	writew((np->full_duplex || (np->link_status & 0x20)) ? 0x120 : 0,
		   ioaddr + MACCtrl0);
	writew(dev->mtu + 14, ioaddr + MaxFrameSize);
	if (dev->mtu > 2047)
		writel(readl(ioaddr + ASICCtrl) | 0x0C, ioaddr + ASICCtrl);

	set_rx_mode(dev);
	writew(0, ioaddr + DownCounter);
	/* Set the chip to poll every N*320nsec. */
	writeb(100, ioaddr + RxDescPoll);
	writeb(127, ioaddr + TxDescPoll);
#if 0
	if (np->drv_flags & KendinPktDropBug)
		writeb(0x01, ioaddr + DebugCtrl1);
#endif

	/* Enable interrupts by setting the interrupt mask. */
	writew(IntrRxDMADone | IntrPCIErr | IntrDrvRqst | IntrTxDone
		   | StatsMax | LinkChange, ioaddr + IntrEnable);
	writew(StatsEnable | RxEnable | TxEnable, ioaddr + MACCtrl1);
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
			printk(KERN_INFO "%s: Setting %s-duplex based on MII #%d "
				   "negotiated capability %4.4x.\n", dev->name,
				   duplex ? "full" : "half", np->phys[0], negotiated);
		writew(duplex ? 0x20 : 0, ioaddr + MACCtrl0);
	}
}

static void netdev_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int next_tick = 10*HZ;

	if (np->msg_level & NETIF_MSG_TIMER) {
		printk(KERN_DEBUG "%s: Media selection timer tick, intr status %4.4x, "
			   "Tx %x Rx %x.\n",
			   dev->name, (int)readw(ioaddr + IntrEnable),
			   (int)readw(ioaddr + TxStatus), (int)readl(ioaddr + RxStatus));
	}
	/* Note: This does not catch a 0 or 1 element stuck queue. */
	if (netif_queue_paused(dev)  &&
		np->cur_tx - np->dirty_tx > 1  &&
		(jiffies - dev->trans_start) > TX_TIMEOUT) {
		tx_timeout(dev);
	}
	check_duplex(dev);
	np->timer.expires = jiffies + next_tick;
	add_timer(&np->timer);
}

static void tx_timeout(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;

	printk(KERN_WARNING "%s: Transmit timed out, status %4.4x,"
		   " resetting...\n", dev->name, (int)readw(ioaddr + TxStatus));

#ifdef __i386__
	if (np->msg_level & NETIF_MSG_TX_ERR) {
		int i;
		printk(KERN_DEBUG "  Rx ring %8.8x: ", (int)np->rx_ring);
		for (i = 0; i < RX_RING_SIZE; i++)
			printk(" %8.8x", (unsigned int)np->rx_ring[i].status);
		printk("\n"KERN_DEBUG"  Tx ring %8.8x: ", (int)np->tx_ring);
		for (i = 0; i < TX_RING_SIZE; i++)
			printk(" %8.8x", np->tx_ring[i].status);
		printk("\n");
	}
#endif

	/* Perhaps we should reinitialize the hardware here. */
	dev->if_port = 0;
	/* Stop and restart the chip's Tx processes . */

	/* Trigger an immediate transmit demand. */
	writew(IntrRxDMADone | IntrPCIErr | IntrDrvRqst | IntrTxDone
		   | StatsMax | LinkChange, ioaddr + IntrEnable);

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

	np->rx_buf_sz = dev->mtu + 20;
	if (np->rx_buf_sz < PKT_BUF_SZ)
		np->rx_buf_sz = PKT_BUF_SZ;
	np->rx_head_desc = &np->rx_ring[0];

	/* Initialize all Rx descriptors. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].next_desc = virt_to_le32desc(&np->rx_ring[i+1]);
		np->rx_ring[i].status = 0;
		np->rx_ring[i].frag[0].length = 0;
		np->rx_skbuff[i] = 0;
	}
	/* Wrap the ring. */
	np->rx_ring[i-1].next_desc = virt_to_le32desc(&np->rx_ring[0]);

	/* Fill in the Rx buffers.  Handle allocation failure gracefully. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = dev_alloc_skb(np->rx_buf_sz);
		np->rx_skbuff[i] = skb;
		if (skb == NULL)
			break;
		skb->dev = dev;			/* Mark as being used by this device. */
		skb_reserve(skb, 2);	/* 16 byte align the IP header. */
		np->rx_ring[i].frag[0].addr = virt_to_le32desc(skb->tail);
		np->rx_ring[i].frag[0].length = cpu_to_le32(np->rx_buf_sz | LastFrag);
	}
	np->dirty_rx = (unsigned int)(i - RX_RING_SIZE);

	for (i = 0; i < TX_RING_SIZE; i++) {
		np->tx_skbuff[i] = 0;
		np->tx_ring[i].status = 0;
	}
	return;
}

static int start_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	struct netdev_desc *txdesc;
	unsigned entry;

	/* Block a timer-based transmit from overlapping. */
	if (netif_pause_tx_queue(dev) != 0) {
		/* This watchdog code is redundant with the media monitor timer. */
		if (jiffies - dev->trans_start > TX_TIMEOUT)
			tx_timeout(dev);
		return 1;
	}

	/* Note: Ordering is important here, set the field with the
	   "ownership" bit last, and only then increment cur_tx. */

	/* Calculate the next Tx descriptor entry. */
	entry = np->cur_tx % TX_RING_SIZE;
	np->tx_skbuff[entry] = skb;
	txdesc = &np->tx_ring[entry];

	txdesc->next_desc = 0;
	/* Note: disable the interrupt generation here before releasing. */
	txdesc->status =
		cpu_to_le32((entry<<2) | DescIntrOnDMADone | DescIntrOnTx | 1);
	txdesc->frag[0].addr = virt_to_le32desc(skb->data);
	txdesc->frag[0].length = cpu_to_le32(skb->len | LastFrag);
	if (np->last_tx)
		np->last_tx->next_desc = virt_to_le32desc(txdesc);
	np->last_tx = txdesc;
	np->cur_tx++;

	/* On some architectures: explicitly flush cache lines here. */

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

	/* Side effect: The read wakes the potentially-idle transmit channel. */
	if (readl(dev->base_addr + TxListPtr) == 0)
		writel(virt_to_bus(&np->tx_ring[entry]), dev->base_addr + TxListPtr);

	dev->trans_start = jiffies;

	if (np->msg_level & NETIF_MSG_TX_QUEUED) {
		printk(KERN_DEBUG "%s: Transmit frame #%d len %ld queued in slot %ld.\n",
			   dev->name, np->cur_tx, skb->len, entry);
	}
	return 0;
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void intr_handler(int irq, void *dev_instance, struct pt_regs *rgs)
{
	struct net_device *dev = (struct net_device *)dev_instance;
	struct netdev_private *np;
	long ioaddr;
	int boguscnt;

	ioaddr = dev->base_addr;
	np = (struct netdev_private *)dev->priv;
	boguscnt = np->max_interrupt_work;

	do {
		int intr_status = readw(ioaddr + IntrStatus);
		if ((intr_status & ~IntrRxDone) == 0 || intr_status == 0xffff)
			break;

		writew(intr_status & (IntrRxDMADone | IntrPCIErr |
							  IntrDrvRqst |IntrTxDone|IntrTxDMADone |
							  StatsMax | LinkChange),
							  ioaddr + IntrStatus);

		if (np->msg_level & NETIF_MSG_INTR)
			printk(KERN_DEBUG "%s: Interrupt, status %4.4x.\n",
				   dev->name, intr_status);

		if (intr_status & IntrRxDMADone)
			netdev_rx(dev);

		if (intr_status & IntrTxDone) {
			int txboguscnt = 32;
			int tx_status = readw(ioaddr + TxStatus);
			while (tx_status & 0x80) {
				if (np->msg_level & NETIF_MSG_TX_DONE)
					printk("%s: Transmit status is %4.4x.\n",
						   dev->name, tx_status);
				if (tx_status & 0x1e) {
					if (np->msg_level & NETIF_MSG_TX_ERR)
						printk("%s: Transmit error status %4.4x.\n",
							   dev->name, tx_status);
					np->stats.tx_errors++;
					if (tx_status & 0x10)  np->stats.tx_fifo_errors++;
#ifdef ETHER_STATS
					if (tx_status & 0x08)  np->stats.collisions16++;
#else
					if (tx_status & 0x08)  np->stats.collisions++;
#endif
					if (tx_status & 0x04)  np->stats.tx_fifo_errors++;
					if (tx_status & 0x02)  np->stats.tx_window_errors++;
					/* This reset has not been verified!. */
					if (tx_status & 0x10) {			/* Reset the Tx. */
						writel(0x001c0000 | readl(ioaddr + ASICCtrl),
							   ioaddr + ASICCtrl);
#if 0					/* Do we need to reset the Tx pointer here? */
						writel(virt_to_bus(&np->tx_ring[np->dirty_tx]),
							   dev->base_addr + TxListPtr);
#endif
					}
					if (tx_status & 0x1e) 		/* Restart the Tx. */
						writew(TxEnable, ioaddr + MACCtrl1);
				}
				/* Yup, this is a documentation bug.  It cost me *hours*. */
				writew(0, ioaddr + TxStatus);
				if (--txboguscnt < 0)
					break;
				tx_status = readw(ioaddr + TxStatus);
			}
		}
		for (; np->cur_tx - np->dirty_tx > 0; np->dirty_tx++) {
			int entry = np->dirty_tx % TX_RING_SIZE;
			if ( ! (np->tx_ring[entry].status & cpu_to_le32(DescTxDMADone)))
				break;
			/* Free the original skb. */
			dev_free_skb_irq(np->tx_skbuff[entry]);
			np->tx_skbuff[entry] = 0;
		}
		if (np->tx_full  &&  np->cur_tx - np->dirty_tx < TX_QUEUE_LEN - 4) {
			/* The ring is no longer full, allow new TX entries. */
			np->tx_full = 0;
			netif_resume_tx_queue(dev);
		}

		/* Abnormal error summary/uncommon events handlers. */
		if (intr_status & (IntrDrvRqst | IntrPCIErr | LinkChange | StatsMax))
			netdev_error(dev, intr_status);

		if (--boguscnt < 0) {
			int intr_clear = readw(ioaddr + IntrClear);
			get_stats(dev);
			printk(KERN_WARNING "%s: Too much work at interrupt, "
				   "status=0x%4.4x / 0x%4.4x .. 0x%4.4x.\n",
				   dev->name, intr_status, intr_clear,
				   (int)readw(ioaddr + IntrClear));
			/* Re-enable us in 3.2msec. */
			writew(1000, ioaddr + DownCounter);
			writew(IntrDrvRqst, ioaddr + IntrEnable);
			break;
		}
	} while (1);

	if (np->msg_level & NETIF_MSG_INTR)
		printk(KERN_DEBUG "%s: exiting interrupt, status=%#4.4x.\n",
			   dev->name, (int)readw(ioaddr + IntrStatus));

	return;
}

/* This routine is logically part of the interrupt handler, but separated
   for clarity and better register allocation. */
static int netdev_rx(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	int entry = np->cur_rx % RX_RING_SIZE;
	int boguscnt = np->dirty_rx + RX_RING_SIZE - np->cur_rx;

	if (np->msg_level & NETIF_MSG_RX_STATUS) {
		printk(KERN_DEBUG " In netdev_rx(), entry %d status %4.4x.\n",
			   entry, np->rx_ring[entry].status);
	}

	/* If EOP is set on the next entry, it's a new packet. Send it up. */
	while (np->rx_head_desc->status & cpu_to_le32(DescOwn)) {
		struct netdev_desc *desc = np->rx_head_desc;
		u32 frame_status = le32_to_cpu(desc->status);
		int pkt_len = frame_status & 0x1fff;		/* Chip omits the CRC. */

		if (np->msg_level & NETIF_MSG_RX_STATUS)
			printk(KERN_DEBUG "  netdev_rx() status was %8.8x.\n",
				   frame_status);
		if (--boguscnt < 0)
			break;
		if (frame_status & 0x001f4000) {
			/* There was a error. */
			if (np->msg_level & NETIF_MSG_RX_ERR)
				printk(KERN_DEBUG "  netdev_rx() Rx error was %8.8x.\n",
					   frame_status);
			np->stats.rx_errors++;
			if (frame_status & 0x00100000) np->stats.rx_length_errors++;
			if (frame_status & 0x00010000) np->stats.rx_fifo_errors++;
			if (frame_status & 0x00060000) np->stats.rx_frame_errors++;
			if (frame_status & 0x00080000) np->stats.rx_crc_errors++;
			if (frame_status & 0x00100000) {
				printk(KERN_WARNING "%s: Oversized Ethernet frame,"
					   " status %8.8x.\n",
					   dev->name, frame_status);
			}
		} else {
			struct sk_buff *skb;

#ifndef final_version
			if (np->msg_level & NETIF_MSG_RX_STATUS)
				printk(KERN_DEBUG "  netdev_rx() normal Rx pkt length %d"
					   ", bogus_cnt %d.\n",
					   pkt_len, boguscnt);
#endif
			/* Check if the packet is long enough to accept without copying
			   to a minimally-sized skbuff. */
			if (pkt_len < np->rx_copybreak
				&& (skb = dev_alloc_skb(pkt_len + 2)) != NULL) {
				skb->dev = dev;
				skb_reserve(skb, 2);	/* 16 byte align the IP header */
				eth_copy_and_sum(skb, np->rx_skbuff[entry]->tail, pkt_len, 0);
				skb_put(skb, pkt_len);
			} else {
				skb_put(skb = np->rx_skbuff[entry], pkt_len);
				np->rx_skbuff[entry] = NULL;
			}
			skb->protocol = eth_type_trans(skb, dev);
			/* Note: checksum -> skb->ip_summed = CHECKSUM_UNNECESSARY; */
			netif_rx(skb);
			dev->last_rx = jiffies;
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
				break;				/* Better luck next round. */
			skb->dev = dev;			/* Mark as being used by this device. */
			skb_reserve(skb, 2);	/* Align IP on 16 byte boundaries */
			np->rx_ring[entry].frag[0].addr = virt_to_le32desc(skb->tail);
		}
		/* Perhaps we need not reset this field. */
		np->rx_ring[entry].frag[0].length =
			cpu_to_le32(np->rx_buf_sz | LastFrag);
		np->rx_ring[entry].status = 0;
	}

	/* No need to restart Rx engine, it will poll. */
	return 0;
}

static void netdev_error(struct net_device *dev, int intr_status)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = (struct netdev_private *)dev->priv;

	if (intr_status & IntrDrvRqst) {
		/* Stop the down counter and turn interrupts back on. */
		printk(KERN_WARNING "%s: Turning interrupts back on.\n", dev->name);
		writew(0, ioaddr + DownCounter);
		writew(IntrRxDMADone | IntrPCIErr | IntrDrvRqst |
			   IntrTxDone | StatsMax | LinkChange, ioaddr + IntrEnable);
	}
	if (intr_status & LinkChange) {
		int new_status = readb(ioaddr + MIICtrl) & 0xE0;
		if (np->msg_level & NETIF_MSG_LINK)
			printk(KERN_NOTICE "%s: Link changed: Autonegotiation advertising"
				   " %4.4x  partner %4.4x.\n", dev->name,
				   mdio_read(dev, np->phys[0], 4),
				   mdio_read(dev, np->phys[0], 5));
		if ((np->link_status ^ new_status) & 0x80) {
			if (new_status & 0x80)
				netif_link_up(dev);
			else
				netif_link_down(dev);
		}
		np->link_status = new_status;
		check_duplex(dev);
	}
	if (intr_status & StatsMax) {
		get_stats(dev);
	}
	if (intr_status & IntrPCIErr) {
		printk(KERN_ERR "%s: Something Wicked happened! %4.4x.\n",
			   dev->name, intr_status);
		/* We must do a global reset of DMA to continue. */
	}
}

static struct net_device_stats *get_stats(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	int i;

	if (readw(ioaddr + StationAddr) == 0xffff)
		return &np->stats;

	/* We do not spinlock statistics.
	   A window only exists if we have non-atomic adds, the error counts
	   are typically zero, and statistics are non-critical. */ 
	np->stats.rx_missed_errors	+= readb(ioaddr + RxMissed);
	np->stats.tx_packets += readw(ioaddr + TxFramesOK);
	np->stats.rx_packets += readw(ioaddr + RxFramesOK);
	np->stats.collisions += readb(ioaddr + StatsLateColl);
	np->stats.collisions += readb(ioaddr + StatsMultiColl);
	np->stats.collisions += readb(ioaddr + StatsOneColl);
	readb(ioaddr + StatsCarrierError);
	readb(ioaddr + StatsTxDefer);
	for (i = StatsTxXSDefer; i <= StatsMcastRx; i++)
		readb(ioaddr + i);
#if LINUX_VERSION_CODE > 0x20127
	np->stats.tx_bytes += readw(ioaddr + TxOctetsLow);
	np->stats.tx_bytes += readw(ioaddr + TxOctetsHigh) << 16;
	np->stats.rx_bytes += readw(ioaddr + RxOctetsLow);
	np->stats.rx_bytes += readw(ioaddr + RxOctetsHigh) << 16;
#else
	readw(ioaddr + TxOctetsLow);
	readw(ioaddr + TxOctetsHigh);
	readw(ioaddr + RxOctetsLow);
	readw(ioaddr + RxOctetsHigh);
#endif

	return &np->stats;
}

/* The little-endian AUTODIN II ethernet CRC calculations.
   A big-endian version is also available.
   This is slow but compact code.  Do not use this routine for bulk data,
   use a table-based routine instead.
   This is common code and should be moved to net/core/crc.c.
   Chips may use the upper or lower CRC bits, and may reverse and/or invert
   them.  Select the endian-ness that results in minimal calculations.
*/
static unsigned const ethernet_polynomial_le = 0xedb88320U;
static inline unsigned ether_crc_le(int length, unsigned char *data)
{
	unsigned int crc = ~0;	/* Initial value. */
	while(--length >= 0) {
		unsigned char current_octet = *data++;
		int bit;
		for (bit = 8; --bit >= 0; current_octet >>= 1) {
			if ((crc ^ current_octet) & 1) {
				crc >>= 1;
				crc ^= ethernet_polynomial_le;
			} else
				crc >>= 1;
		}
	}
	return crc;
}

static void set_rx_mode(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	u16 mc_filter[4];			/* Multicast hash filter */
	u32 rx_mode;
	int i;

	if (dev->flags & IFF_PROMISC) {			/* Set promiscuous. */
		/* Unconditionally log net taps. */
		printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n", dev->name);
		memset(mc_filter, ~0, sizeof(mc_filter));
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptAll | AcceptMyPhys;
	} else if ((dev->mc_count > np->multicast_filter_limit)
			   ||  (dev->flags & IFF_ALLMULTI)) {
		/* Too many to match, or accept all multicasts. */
		memset(mc_filter, 0xff, sizeof(mc_filter));
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
	} else if (dev->mc_count) {
		struct dev_mc_list *mclist;
		memset(mc_filter, 0, sizeof(mc_filter));
		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
			 i++, mclist = mclist->next) {
			set_bit(ether_crc_le(ETH_ALEN, mclist->dmi_addr) & 0x3f,
					mc_filter);
		}
		rx_mode = AcceptBroadcast | AcceptMultiHash | AcceptMyPhys;
	} else {
		writeb(AcceptBroadcast | AcceptMyPhys, ioaddr + RxMode);
		return;
	}
	for (i = 0; i < 4; i++)
		writew(mc_filter[i], ioaddr + MulticastFilter0 + i*2);
	writeb(rx_mode, ioaddr + RxMode);
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
		if (data[0] == np->phys[0]) {
			u16 value = data[2];
			switch (data[1]) {
			case 0:
				/* Check for autonegotiation on or reset. */
				np->medialock = (value & 0x9000) ? 0 : 1;
				if (np->medialock)
					np->full_duplex = (value & 0x0100) ? 1 : 0;
				break;
			case 4: np->advertising = value; break;
			}
			/* Perhaps check_duplex(dev), depending on chip semantics. */
		}
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

static int sundance_pwr_event(void *dev_instance, int event)
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
		writew(TxDisable | RxDisable | StatsDisable, ioaddr + MACCtrl1);
		break;
	case DRV_RESUME:
		sundance_start(dev);
		break;
	case DRV_DETACH: {
		struct net_device **devp, **next;
		if (dev->flags & IFF_UP) {
			/* Some, but not all, kernel versions close automatically. */
			dev_close(dev);
			dev->flags &= ~(IFF_UP|IFF_RUNNING);
		}
		unregister_netdev(dev);
		release_region(dev->base_addr, pci_id_tbl[np->chip_id].io_size);
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
	case DRV_PWR_WakeOn:
		writeb(readb(ioaddr + WakeEvent) | 2, ioaddr + WakeEvent);
		/* Fall through. */
	case DRV_PWR_DOWN:
	case DRV_PWR_UP:
		acpi_set_pwr_state(np->pci_dev, event==DRV_PWR_UP ? ACPI_D0:ACPI_D3);
		break;
	default:
		return -1;
	}

	return 0;
}

static int netdev_close(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	int i;

	netif_stop_tx_queue(dev);

	if (np->msg_level & NETIF_MSG_IFDOWN) {
		printk(KERN_DEBUG "%s: Shutting down ethercard, status was Tx %2.2x "
			   "Rx %4.4x Int %2.2x.\n",
			   dev->name, (int)readw(ioaddr + TxStatus),
			   (int)readl(ioaddr + RxStatus), (int)readw(ioaddr + IntrStatus));
		printk(KERN_DEBUG "%s: Queue pointers were Tx %d / %d,  Rx %d / %d.\n",
			   dev->name, np->cur_tx, np->dirty_tx, np->cur_rx, np->dirty_rx);
	}

	/* Disable interrupts by clearing the interrupt mask. */
	writew(0x0000, ioaddr + IntrEnable);

	/* Stop the chip's Tx and Rx processes. */
	writew(TxDisable | RxDisable | StatsDisable, ioaddr + MACCtrl1);

	del_timer(&np->timer);

#ifdef __i386__
	if (np->msg_level & NETIF_MSG_IFDOWN) {
		printk("\n"KERN_DEBUG"  Tx ring at %8.8x:\n",
			   (int)virt_to_bus(np->tx_ring));
		for (i = 0; i < TX_RING_SIZE; i++)
			printk(" #%d desc. %4.4x %8.8x %8.8x.\n",
				   i, np->tx_ring[i].status, np->tx_ring[i].frag[0].addr,
				   np->tx_ring[i].frag[0].length);
		printk("\n"KERN_DEBUG "  Rx ring %8.8x:\n",
			   (int)virt_to_bus(np->rx_ring));
		for (i = 0; i < /*RX_RING_SIZE*/4 ; i++) {
			printk(KERN_DEBUG " #%d desc. %4.4x %4.4x %8.8x\n",
				   i, np->rx_ring[i].status, np->rx_ring[i].frag[0].addr,
				   np->rx_ring[i].frag[0].length);
		}
	}
#endif /* __i386__ debugging only */

	free_irq(dev->irq, dev);

	/* Free all the skbuffs in the Rx queue. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].status = 0;
		np->rx_ring[i].frag[0].addr = 0xBADF00D0; /* An invalid address. */
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
	}

	MOD_DEC_USE_COUNT;

	return 0;
}


#ifdef MODULE
int init_module(void)
{
	if (debug >= NETIF_MSG_DRV)	/* Emit version even if no cards detected. */
		printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	return pci_drv_register(&sundance_drv_id, NULL);
}

void cleanup_module(void)
{
	struct net_device *next_dev;

	pci_drv_unregister(&sundance_drv_id);

	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	while (root_net_dev) {
		struct netdev_private *np = (void *)(root_net_dev->priv);
		unregister_netdev(root_net_dev);
#ifdef USE_IO_OPS
		release_region(root_net_dev->base_addr,
					   pci_id_tbl[np->chip_id].io_size);
#else
		iounmap((char *)root_net_dev->base_addr);
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
 *  compile-command: "make KERNVER=`uname -r` sundance.o"
 *  compile-cmd1: "gcc -DMODULE -Wall -Wstrict-prototypes -O6 -c sundance.c"
 *  simple-compile-command: "gcc -DMODULE -O6 -c sundance.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
