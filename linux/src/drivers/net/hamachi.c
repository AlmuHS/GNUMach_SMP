/* hamachi.c: A Packet Engines GNIC-II Gigabit Ethernet driver for Linux. */
/*
	Written 1998-2002 by Donald Becker.

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

	This driver is for the Packet Engines GNIC-II PCI Gigabit Ethernet
	adapter.

	Support and updates available at
	http://www.scyld.com/network/hamachi.html
*/

/* These identify the driver base version and may not be removed. */
static const char version1[] =
"hamachi.c:v1.04 11/17/2002  Written by Donald Becker <becker@scyld.com>\n";
static const char version2[] =
"  http://www.scyld.com/network/hamachi.html\n";

/* Automatically extracted configuration info:
probe-func: hamachi_probe
config-in: tristate 'Packet Engines "Hamachi" PCI Gigabit Ethernet support' CONFIG_HAMACHI
c-help-name: Packet Engines "Hamachi" PCI Gigabit Ethernet support
c-help-symbol: CONFIG_HAMACHI
c-help: This driver is for the Packet Engines "Hamachi" GNIC-2 Gigabit Ethernet
c-help: adapter.
c-help: Usage information and updates are available from
c-help: http://www.scyld.com/network/hamachi.html
*/

/* The user-configurable values.
   These may be modified when a driver module is loaded.*/

/* Message enable level: 0..31 = no..all messages.  See NETIF_MSG docs. */
static int debug = 2;

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 40;

/* Maximum number of multicast addresses to filter (vs. rx-all-multicast).
   The Hamachi has a 64 element perfect filter.  */
static int multicast_filter_limit = 32;

/* Set the copy breakpoint for the copy-only-tiny-frames scheme.
   Setting to > 1518 effectively disables this feature. */
static int rx_copybreak = 0;

/* A override for the hardware detection of bus width.
   Set to 1 to force 32 bit PCI bus detection.  Set to 4 to force 64 bit.
   Add 2 to disable parity detection.
*/
static int force32 = 0;

/* Used to pass the media type, etc.
   These exist for driver interoperability.
   Only 1 Gigabit is supported by the chip.
*/
#define MAX_UNITS 8		/* More are supported, limit only on options */
static int options[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int full_duplex[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};

/* Operational parameters that are set at compile time. */

/* Keep the ring sizes a power of two for compile efficiency.
   The compiler will convert <unsigned>'%'<2^N> into a bit mask.
   Making the Tx ring too large decreases the effectiveness of channel
   bonding and packet priority.
   There are no ill effects from too-large receive rings. */
#define TX_RING_SIZE	64
#define TX_QUEUE_LEN	60		/* Limit ring entries actually used.  */
#define RX_RING_SIZE	128

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
#include <asm/unaligned.h>

#ifdef INLINE_PCISCAN
#include "k_compat.h"
#else
#include "pci-scan.h"
#include "kern_compat.h"
#endif

/* Condensed operations for readability. */
#if ADDRLEN == 64
#define virt_to_desc(addr)  cpu_to_le64(virt_to_bus(addr))
#else
#define virt_to_desc(addr)  cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr)  bus_to_virt(le32_to_cpu(addr))
#endif

#if (LINUX_VERSION_CODE >= 0x20100)  &&  defined(MODULE)
char kernel_version[] = UTS_RELEASE;
#endif

MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("Packet Engines 'Hamachi' GNIC-II Gigabit Ethernet driver");
MODULE_LICENSE("GPL");
MODULE_PARM(debug, "i");
MODULE_PARM(options, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(rx_copybreak, "i");
MODULE_PARM(full_duplex, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(multicast_filter_limit, "i");
MODULE_PARM(max_interrupt_work, "i");
MODULE_PARM(force32, "i");
MODULE_PARM_DESC(debug, "Driver message level (0-31)");
MODULE_PARM_DESC(options, "Force transceiver type or fixed speed+duplex");
MODULE_PARM_DESC(max_interrupt_work,
				 "Driver maximum events handled per interrupt");
MODULE_PARM_DESC(full_duplex,
				 "Non-zero to force full duplex, non-negotiated link "
				 "(unused, deprecated).");
MODULE_PARM_DESC(rx_copybreak,
				 "Breakpoint in bytes for copy-only-tiny-frames");
MODULE_PARM_DESC(multicast_filter_limit,
				 "Multicast addresses before switching to Rx-all-multicast");
MODULE_PARM_DESC(force32, "Set to 1 to force 32 bit PCI bus use.");

/*
				Theory of Operation

I. Board Compatibility

This device driver is designed for the Packet Engines "Hamachi"
Gigabit Ethernet chip.  The only PCA currently supported is the GNIC-II 64-bit
66Mhz PCI card.

II. Board-specific settings

No jumpers exist on the board.  The chip supports software correction of
various motherboard wiring errors, however this driver does not support
that feature.

III. Driver operation

IIIa. Ring buffers

The Hamachi uses a typical descriptor based bus-master architecture.
The descriptor list is similar to that used by the Digital Tulip.
This driver uses two statically allocated fixed-size descriptor lists
formed into rings by a branch from the final descriptor to the beginning of
the list.  The ring sizes are set at compile time by RX/TX_RING_SIZE.

This driver uses a zero-copy receive and transmit scheme similar my other
network drivers.
The driver allocates full frame size skbuffs for the Rx ring buffers at
open() time and passes the skb->data field to the Hamachi as receive data
buffers.  When an incoming frame is less than RX_COPYBREAK bytes long,
a fresh skbuff is allocated and the frame is copied to the new skbuff.
When the incoming frame is larger, the skbuff is passed directly up the
protocol stack and replaced by a newly allocated skbuff.

The RX_COPYBREAK value is chosen to trade-off the memory wasted by
using a full-sized skbuff for small frames vs. the copying costs of larger
frames.  Gigabit cards are typically used on generously configured machines
and the underfilled buffers have negligible impact compared to the benefit of
a single allocation size, so the default value of zero results in never
copying packets.

IIIb/c. Transmit/Receive Structure

The Rx and Tx descriptor structure are straight-forward, with no historical
baggage that must be explained.  Unlike the awkward DBDMA structure, there
are no unused fields or option bits that had only one allowable setting.

Two details should be noted about the descriptors: The chip supports both 32
bit and 64 bit address structures, and the length field is overwritten on
the receive descriptors.  The descriptor length is set in the control word
for each channel. The development driver uses 32 bit addresses only, however
64 bit addresses may be enabled for 64 bit architectures e.g. the Alpha.

IIId. Synchronization

This driver is very similar to my other network drivers.
The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->tbusy flag.  The other thread is the interrupt handler, which is single
threaded by the hardware and other software.

The send packet thread has partial control over the Tx ring and 'dev->tbusy'
flag.  It sets the tbusy flag whenever it's queuing a Tx packet. If the next
queue slot is empty, it clears the tbusy flag when finished otherwise it sets
the 'hmp->tx_full' flag.

The interrupt handler has exclusive control over the Rx ring and records stats
from the Tx ring.  After reaping the stats, it marks the Tx queue entry as
empty by incrementing the dirty_tx mark. Iff the 'hmp->tx_full' flag is set, it
clears both the tx_full and tbusy flags.

IV. Notes

Thanks to Kim Stearns of Packet Engines for providing a pair of GNIC-II boards.

IVb. References

Hamachi Engineering Design Specification, 5/15/97
(Note: This version was marked "Confidential".)

IVc. Errata

None noted.
*/


/* The table for PCI detection and activation. */

static void *hamachi_probe1(struct pci_dev *pdev, void *init_dev,
							long ioaddr, int irq, int chip_idx, int find_cnt);
enum chip_capability_flags { CanHaveMII=1, };

static struct pci_id_info pci_id_tbl[] = {
	{"Packet Engines GNIC-II \"Hamachi\"", { 0x09111318, 0xffffffff,},
	 PCI_USES_MEM | PCI_USES_MASTER | PCI_ADDR0 | PCI_ADDR_64BITS, 0x400, 0, },
	{ 0,},
};

struct drv_id_info hamachi_drv_id = {
	"hamachi", 0, PCI_CLASS_NETWORK_ETHERNET<<8, pci_id_tbl,
	hamachi_probe1, 0,
};

/* Offsets to the Hamachi registers.  Various sizes. */
enum hamachi_offsets {
	TxDMACtrl=0x00, TxCmd=0x04, TxStatus=0x06, TxPtr=0x08, TxCurPtr=0x10,
	RxDMACtrl=0x20, RxCmd=0x24, RxStatus=0x26, RxPtr=0x28, RxCurPtr=0x30,
	PCIClkMeas=0x060, MiscStatus=0x066, ChipRev=0x68, ChipReset=0x06B,
	LEDCtrl=0x06C, VirtualJumpers=0x06D,
	TxChecksum=0x074, RxChecksum=0x076,
	TxIntrCtrl=0x078, RxIntrCtrl=0x07C,
	InterruptEnable=0x080, InterruptClear=0x084, IntrStatus=0x088,
	EventStatus=0x08C,
	MACCnfg=0x0A0, FrameGap0=0x0A2, FrameGap1=0x0A4,
	/* See enum MII_offsets below. */
	MACCnfg2=0x0B0, RxDepth=0x0B8, FlowCtrl=0x0BC, MaxFrameSize=0x0CE,
	AddrMode=0x0D0, StationAddr=0x0D2,
	/* Gigabit AutoNegotiation. */
	ANCtrl=0x0E0, ANStatus=0x0E2, ANXchngCtrl=0x0E4, ANAdvertise=0x0E8,
	ANLinkPartnerAbility=0x0EA,
	EECmdStatus=0x0F0, EEData=0x0F1, EEAddr=0x0F2,
	FIFOcfg=0x0F8,
};

/* Offsets to the MII-mode registers. */
enum MII_offsets {
	MII_Cmd=0xA6, MII_Addr=0xA8, MII_Wr_Data=0xAA, MII_Rd_Data=0xAC,
	MII_Status=0xAE,
};

/* Bits in the interrupt status/mask registers. */
enum intr_status_bits {
	IntrRxDone=0x01, IntrRxPCIFault=0x02, IntrRxPCIErr=0x04,
	IntrTxDone=0x100, IntrTxPCIFault=0x200, IntrTxPCIErr=0x400,
	LinkChange=0x10000, NegotiationChange=0x20000, StatsMax=0x40000, };

/* The Hamachi Rx and Tx buffer descriptors. */
struct hamachi_desc {
	u32 status_n_length;
#if ADDRLEN == 64
	u32 pad;
	u64 addr;
#else
	u32 addr;
#endif
};

/* Bits in hamachi_desc.status */
enum desc_status_bits {
	DescOwn=0x80000000, DescEndPacket=0x40000000, DescEndRing=0x20000000,
	DescIntr=0x10000000,
};

#define PRIV_ALIGN	15 	/* Required alignment mask */
struct hamachi_private {
	/* Descriptor rings first for alignment.  Tx requires a second descriptor
	   for status. */
	struct hamachi_desc rx_ring[RX_RING_SIZE];
	struct hamachi_desc tx_ring[TX_RING_SIZE];
	/* The addresses of receive-in-place skbuffs. */
	struct sk_buff* rx_skbuff[RX_RING_SIZE];
	/* The saved address of a sent-in-place packet/buffer, for skfree(). */
	struct sk_buff* tx_skbuff[TX_RING_SIZE];
	struct net_device *next_module;
	void *priv_addr;					/* Unaligned address for kfree */
	struct net_device_stats stats;
	struct timer_list timer;	/* Media monitoring timer. */
	int chip_id, drv_flags;
	struct pci_dev *pci_dev;

	/* Frequently used and paired value: keep adjacent for cache effect. */
	int msg_level;
	int max_interrupt_work;
	long in_interrupt;

	struct hamachi_desc *rx_head_desc;
	unsigned int cur_rx, dirty_rx;		/* Producer/consumer ring indices */
	unsigned int rx_buf_sz;				/* Based on MTU+slack. */
	int rx_copybreak;
	int multicast_filter_limit;
	int rx_mode;

	unsigned int cur_tx, dirty_tx;
	unsigned int tx_full:1;				/* The Tx queue is full. */
	unsigned int full_duplex:1;			/* Full-duplex operation requested. */
	unsigned int duplex_lock:1;
	unsigned int medialock:1;			/* Do not sense media. */
	unsigned int default_port;			/* Last dev->if_port value. */
	/* MII transceiver section. */
	int mii_cnt;						/* MII device addresses. */
	u16 advertising;					/* NWay media advertisement */
	unsigned char phys[2];				/* MII device addresses. */
};

static int read_eeprom(struct net_device *dev, int location);
static int mdio_read(long ioaddr, int phy_id, int location);
static void mdio_write(long ioaddr, int phy_id, int location, int value);
static int hamachi_open(struct net_device *dev);
static int mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
#ifdef HAVE_CHANGE_MTU
static int change_mtu(struct net_device *dev, int new_mtu);
#endif
static void hamachi_timer(unsigned long data);
static void hamachi_tx_timeout(struct net_device *dev);
static void hamachi_init_ring(struct net_device *dev);
static int hamachi_start_xmit(struct sk_buff *skb, struct net_device *dev);
static void hamachi_interrupt(int irq, void *dev_instance, struct pt_regs *regs);
static int hamachi_rx(struct net_device *dev);
static void hamachi_error(struct net_device *dev, int intr_status);
static int hamachi_close(struct net_device *dev);
static struct net_device_stats *hamachi_get_stats(struct net_device *dev);
static void set_rx_mode(struct net_device *dev);



/* A list of our installed devices, for removing the driver module. */
static struct net_device *root_hamachi_dev = NULL;

#ifndef MODULE
int hamachi_probe(struct net_device *dev)
{
	if (pci_drv_register(&hamachi_drv_id, dev) < 0)
		return -ENODEV;
	printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	return 0;
}
#endif

static void *hamachi_probe1(struct pci_dev *pdev, void *init_dev,
							long ioaddr, int irq, int chip_idx, int card_idx)
{
	struct net_device *dev;
	struct hamachi_private *np;
	void *priv_mem;
	int i, option = card_idx < MAX_UNITS ? options[card_idx] : 0;

	dev = init_etherdev(init_dev, 0);
	if (!dev)
		return NULL;

	printk(KERN_INFO "%s: %s type %x at 0x%lx, ",
		   dev->name, pci_id_tbl[chip_idx].name, (int)readl(ioaddr + ChipRev),
		   ioaddr);

	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = read_eeprom(dev, 4 + i);
	/* Alternate:  readb(ioaddr + StationAddr + i); */
	for (i = 0; i < 5; i++)
			printk("%2.2x:", dev->dev_addr[i]);
	printk("%2.2x, IRQ %d.\n", dev->dev_addr[i], irq);

	i = readb(ioaddr + PCIClkMeas);
	printk(KERN_INFO "%s:  %d-bit %d Mhz PCI bus (%d), Virtual Jumpers "
		   "%2.2x, LPA %4.4x.\n",
		   dev->name, readw(ioaddr + MiscStatus) & 1 ? 64 : 32,
		   i ? 2000/(i&0x7f) : 0, i&0x7f, (int)readb(ioaddr + VirtualJumpers),
		   (int)readw(ioaddr + ANLinkPartnerAbility));

	/* Hmmm, do we really need to reset the chip???. */
	writeb(1, ioaddr + ChipReset);

	/* If the bus size is misidentified, do the following. */
	if (force32)
		writeb(force32, ioaddr + VirtualJumpers);

	/* Make certain elements e.g. descriptor lists are aligned. */
	priv_mem = kmalloc(sizeof(*np) + PRIV_ALIGN, GFP_KERNEL);
	/* Check for the very unlikely case of no memory. */
	if (priv_mem == NULL)
		return NULL;

	dev->base_addr = ioaddr;
	dev->irq = irq;

	dev->priv = np = (void *)(((long)priv_mem + PRIV_ALIGN) & ~PRIV_ALIGN);
	memset(np, 0, sizeof(*np));
	np->priv_addr = priv_mem;

	np->next_module = root_hamachi_dev;
	root_hamachi_dev = dev;

	np->pci_dev = pdev;
	np->chip_id = chip_idx;
	np->drv_flags = pci_id_tbl[chip_idx].drv_flags;
	np->msg_level = (1 << debug) - 1;
	np->rx_copybreak = rx_copybreak;
	np->max_interrupt_work = max_interrupt_work;
	np->multicast_filter_limit =
		multicast_filter_limit < 64 ? multicast_filter_limit : 64;

	if (dev->mem_start)
		option = dev->mem_start;

	/* The lower four bits are the media type. */
	if (option > 0) {
		if (option & 0x2220)
			np->full_duplex = 1;
		np->default_port = option & 15;
		if (np->default_port & 0x3330)
			np->medialock = 1;
	}
	if (card_idx < MAX_UNITS  &&  full_duplex[card_idx] > 0)
		np->full_duplex = 1;

	if (np->full_duplex) {
		if (np->msg_level & NETIF_MSG_PROBE)
			printk(KERN_INFO "%s: Set to forced full duplex, autonegotiation"
				   " disabled.\n", dev->name);
		np->duplex_lock = 1;
	}

	/* The Hamachi-specific entries in the device structure. */
	dev->open = &hamachi_open;
	dev->hard_start_xmit = &hamachi_start_xmit;
	dev->stop = &hamachi_close;
	dev->get_stats = &hamachi_get_stats;
	dev->set_multicast_list = &set_rx_mode;
	dev->do_ioctl = &mii_ioctl;
#ifdef HAVE_CHANGE_MTU
	dev->change_mtu = change_mtu;
#endif

	if (np->drv_flags & CanHaveMII) {
		int phy, phy_idx = 0;
		for (phy = 0; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(ioaddr, phy, 1);
			if (mii_status != 0xffff  &&  mii_status != 0x0000) {
				np->phys[phy_idx++] = phy;
				np->advertising = mdio_read(ioaddr, phy, 4);
				printk(KERN_INFO "%s: MII PHY found at address %d, status "
					   "0x%4.4x advertising %4.4x.\n",
					   dev->name, phy, mii_status, np->advertising);
			}
		}
		np->mii_cnt = phy_idx;
	}
#ifdef notyet	
	/* Disable PCI Parity Error (0x02) or PCI 64 Bit (0x01) for miswired
	   motherboards. */
	if (readb(ioaddr + VirtualJumpers) != 0x30)
		writeb(0x33, ioaddr + VirtualJumpers)
#endif
	/* Configure gigabit autonegotiation. */
	writew(0x0400, ioaddr + ANXchngCtrl);	/* Enable legacy links. */
	writew(0x08e0, ioaddr + ANAdvertise);	/* Set our advertise word. */
	writew(0x1000, ioaddr + ANCtrl);		/* Enable negotiation */

	return dev;
}

static int read_eeprom(struct net_device *dev, int location)
{
	struct hamachi_private *np = (void *)dev->priv;
	long ioaddr = dev->base_addr;
	int bogus_cnt = 1000;

	writew(location, ioaddr + EEAddr);
	writeb(0x02, ioaddr + EECmdStatus);
	while ((readb(ioaddr + EECmdStatus) & 0x40)  && --bogus_cnt > 0)
		;
	if (np->msg_level & NETIF_MSG_MISC)
		printk(KERN_DEBUG "   EEPROM status is %2.2x after %d ticks.\n",
			   (int)readb(ioaddr + EECmdStatus), 1000- bogus_cnt);
	return readb(ioaddr + EEData);
}

/* MII Managemen Data I/O accesses.
   These routines assume the MDIO controller is idle, and do not exit until
   the command is finished. */

static int mdio_read(long ioaddr, int phy_id, int location)
{
	int i;

	writew((phy_id<<8) + location, ioaddr + MII_Addr);
	writew(1, ioaddr + MII_Cmd);
	for (i = 10000; i >= 0; i--)
		if ((readw(ioaddr + MII_Status) & 1) == 0)
			break;
	return readw(ioaddr + MII_Rd_Data);
}

static void mdio_write(long ioaddr, int phy_id, int location, int value)
{
	int i;

	writew((phy_id<<8) + location, ioaddr + MII_Addr);
	writew(value, ioaddr + MII_Wr_Data);

	/* Wait for the command to finish. */
	for (i = 10000; i >= 0; i--)
		if ((readw(ioaddr + MII_Status) & 1) == 0)
			break;
	return;
}


static int hamachi_open(struct net_device *dev)
{
	struct hamachi_private *hmp = (struct hamachi_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int i;

	/* Do we need to reset the chip??? */

	MOD_INC_USE_COUNT;

	if (request_irq(dev->irq, &hamachi_interrupt, SA_SHIRQ, dev->name, dev)) {
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}

	if (hmp->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: hamachi_open() irq %d.\n",
			   dev->name, dev->irq);

	hamachi_init_ring(dev);

#if ADDRLEN == 64
	writel(virt_to_bus(hmp->rx_ring), ioaddr + RxPtr);
	writel(virt_to_bus(hmp->rx_ring) >> 32, ioaddr + RxPtr + 4);
	writel(virt_to_bus(hmp->tx_ring), ioaddr + TxPtr);
	writel(virt_to_bus(hmp->tx_ring) >> 32, ioaddr + TxPtr + 4);
#else
	writel(virt_to_bus(hmp->rx_ring), ioaddr + RxPtr);
	writel(virt_to_bus(hmp->tx_ring), ioaddr + TxPtr);
#endif

	for (i = 0; i < 6; i++)
		writeb(dev->dev_addr[i], ioaddr + StationAddr + i);

	/* Initialize other registers: with so many this eventually this will
	   converted to an offset/value list. */
	/* Configure the FIFO for 512K external, 16K used for Tx. */
	writew(0x0028, ioaddr + FIFOcfg);

	if (dev->if_port == 0)
		dev->if_port = hmp->default_port;
	hmp->in_interrupt = 0;

	/* Setting the Rx mode will start the Rx process. */
	/* We are always in full-duplex mode with gigabit! */
	hmp->full_duplex = 1;
	writew(0x0001, ioaddr + RxChecksum); /* Enable Rx IP partial checksum. */
	writew(0x8000, ioaddr + MACCnfg); /* Soft reset the MAC */
	writew(0x215F, ioaddr + MACCnfg);
	writew(0x000C, ioaddr + FrameGap0); /* 0060/4060 for non-MII 10baseT */
	writew(0x1018, ioaddr + FrameGap1);
	writew(0x2780, ioaddr + MACCnfg2); /* Upper 16 bits control LEDs. */
	/* Enable automatic generation of flow control frames, period 0xffff. */
	writel(0x0030FFFF, ioaddr + FlowCtrl);
	writew(dev->mtu+19, ioaddr + MaxFrameSize); 	/* hmp->rx_buf_sz ??? */

	/* Enable legacy links. */
	writew(0x0400, ioaddr + ANXchngCtrl);	/* Enable legacy links. */
	/* Initial Link LED to blinking red. */
	writeb(0x03, ioaddr + LEDCtrl);

	/* Configure interrupt mitigation.  This has a great effect on
	   performance, so systems tuning should start here!. */
	writel(0x00080000, ioaddr + TxIntrCtrl);
	writel(0x00000020, ioaddr + RxIntrCtrl);

	hmp->rx_mode = 0;			/* Force Rx mode write. */
	set_rx_mode(dev);
	netif_start_tx_queue(dev);

	/* Enable interrupts by setting the interrupt mask. */
	writel(0x80878787, ioaddr + InterruptEnable);
	writew(0x0000, ioaddr + EventStatus);	/* Clear non-interrupting events */

	/* Configure and start the DMA channels. */
	/* Burst sizes are in the low three bits: size = 4<<(val&7) */
#if ADDRLEN == 64
	writew(0x0055, ioaddr + RxDMACtrl); 		/* 128 dword bursts */
	writew(0x0055, ioaddr + TxDMACtrl);
#else
	writew(0x0015, ioaddr + RxDMACtrl);
	writew(0x0015, ioaddr + TxDMACtrl);
#endif
	writew(1, dev->base_addr + RxCmd);

	if (hmp->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: Done hamachi_open(), status: Rx %x Tx %x.\n",
			   dev->name, (int)readw(ioaddr + RxStatus),
			   (int)readw(ioaddr + TxStatus));

	/* Set the timer to check for link beat. */
	init_timer(&hmp->timer);
	hmp->timer.expires = jiffies + 3*HZ;
	hmp->timer.data = (unsigned long)dev;
	hmp->timer.function = &hamachi_timer;				/* timer handler */
	add_timer(&hmp->timer);

	return 0;
}

static void hamachi_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct hamachi_private *hmp = (struct hamachi_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int next_tick = 10*HZ;

	if (hmp->msg_level & NETIF_MSG_TIMER) {
		printk(KERN_INFO "%s: Hamachi Autonegotiation status %4.4x, LPA "
			   "%4.4x.\n", dev->name, (int)readw(ioaddr + ANStatus),
			   (int)readw(ioaddr + ANLinkPartnerAbility));
		printk(KERN_INFO "%s: Autonegotiation regs %4.4x %4.4x %4.4x "
			   "%4.4x %4.4x %4.4x.\n", dev->name,
		       (int)readw(ioaddr + 0x0e0),
			   (int)readw(ioaddr + 0x0e2),
			   (int)readw(ioaddr + 0x0e4),
			   (int)readw(ioaddr + 0x0e6),
			   (int)readw(ioaddr + 0x0e8),
			   (int)readw(ioaddr + 0x0eA));
	}
	/* This has a small false-trigger window. */
	if (netif_queue_paused(dev) &&
		(jiffies - dev->trans_start) > TX_TIMEOUT
		&& hmp->cur_tx - hmp->dirty_tx > 1) {
		hamachi_tx_timeout(dev);
	}
	/* We could do something here... nah. */
	hmp->timer.expires = jiffies + next_tick;
	add_timer(&hmp->timer);
}

static void hamachi_tx_timeout(struct net_device *dev)
{
	struct hamachi_private *hmp = (struct hamachi_private *)dev->priv;
	long ioaddr = dev->base_addr;

	printk(KERN_WARNING "%s: Hamachi transmit timed out, status %8.8x,"
		   " resetting...\n", dev->name, (int)readw(ioaddr + TxStatus));

	if (hmp->msg_level & NETIF_MSG_TX_ERR) {
		int i;
		printk(KERN_DEBUG "  Rx ring %p: ", hmp->rx_ring);
		for (i = 0; i < RX_RING_SIZE; i++)
			printk(" %8.8x", (unsigned int)hmp->rx_ring[i].status_n_length);
		printk("\n"KERN_DEBUG"  Tx ring %p: ", hmp->tx_ring);
		for (i = 0; i < TX_RING_SIZE; i++)
			printk(" %4.4x", hmp->tx_ring[i].status_n_length);
		printk("\n");
	}

	/* Perhaps we should reinitialize the hardware here. */
	dev->if_port = 0;
	/* Stop and restart the chip's Tx processes . */

	/* Trigger an immediate transmit demand. */
	writew(2, dev->base_addr + TxCmd);
	writew(1, dev->base_addr + TxCmd);
	writew(1, dev->base_addr + RxCmd);

	dev->trans_start = jiffies;
	hmp->stats.tx_errors++;
	return;
}


/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void hamachi_init_ring(struct net_device *dev)
{
	struct hamachi_private *hmp = (struct hamachi_private *)dev->priv;
	int i;

	hmp->tx_full = 0;
	hmp->cur_rx = hmp->cur_tx = 0;
	hmp->dirty_rx = hmp->dirty_tx = 0;

	/* Size of each temporary Rx buffer.  Add 8 if you do Rx checksumming! */
	hmp->rx_buf_sz = dev->mtu + 18 + 8;
	/* Match other driver's allocation size when possible. */
	if (hmp->rx_buf_sz < PKT_BUF_SZ)
		hmp->rx_buf_sz = PKT_BUF_SZ;
	hmp->rx_head_desc = &hmp->rx_ring[0];

	/* Initialize all Rx descriptors. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		hmp->rx_ring[i].status_n_length = 0;
		hmp->rx_skbuff[i] = 0;
	}
	/* Fill in the Rx buffers.  Handle allocation failure gracefully. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = dev_alloc_skb(hmp->rx_buf_sz);
		hmp->rx_skbuff[i] = skb;
		if (skb == NULL)
			break;
		skb->dev = dev;			/* Mark as being used by this device. */
		skb_reserve(skb, 2);	/* 16 byte align the IP header. */
		hmp->rx_ring[i].addr = virt_to_desc(skb->tail);
		hmp->rx_ring[i].status_n_length =
			cpu_to_le32(DescOwn | DescEndPacket | DescIntr | hmp->rx_buf_sz);
	}
	hmp->dirty_rx = (unsigned int)(i - RX_RING_SIZE);
	/* Mark the last entry as wrapping the ring. */
	hmp->rx_ring[i-1].status_n_length |= cpu_to_le32(DescEndRing);

	for (i = 0; i < TX_RING_SIZE; i++) {
		hmp->tx_skbuff[i] = 0;
		hmp->tx_ring[i].status_n_length = 0;
	}
	return;
}

static int hamachi_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct hamachi_private *hmp = (struct hamachi_private *)dev->priv;
	unsigned entry;

	/* Block a timer-based transmit from overlapping.  This could better be
	   done with atomic_swap(1, dev->tbusy), but set_bit() works as well. */
	if (netif_pause_tx_queue(dev) != 0) {
		/* This watchdog code is redundant with the media monitor timer. */
		if (jiffies - dev->trans_start > TX_TIMEOUT)
			hamachi_tx_timeout(dev);
		return 1;
	}

	/* Note: Ordering is important here, set the field with the
	   "ownership" bit last, and only then increment cur_tx. */

	/* Calculate the next Tx descriptor entry. */
	entry = hmp->cur_tx % TX_RING_SIZE;

	hmp->tx_skbuff[entry] = skb;

	hmp->tx_ring[entry].addr = virt_to_desc(skb->data);
	if (entry >= TX_RING_SIZE-1)		 /* Wrap ring */
		hmp->tx_ring[entry].status_n_length =
			cpu_to_le32(DescOwn|DescEndPacket|DescEndRing|DescIntr | skb->len);
	else
		hmp->tx_ring[entry].status_n_length =
			cpu_to_le32(DescOwn|DescEndPacket | skb->len);
	hmp->cur_tx++;

	/* Architecture-specific: explicitly flush cache lines here. */

	/* Wake the potentially-idle transmit channel. */
	writew(1, dev->base_addr + TxCmd);

	if (hmp->cur_tx - hmp->dirty_tx >= TX_QUEUE_LEN - 1) {
		hmp->tx_full = 1;
		if (hmp->cur_tx - hmp->dirty_tx < TX_QUEUE_LEN - 1) {
			netif_unpause_tx_queue(dev);
			hmp->tx_full = 0;
		} else
			netif_stop_tx_queue(dev);
	} else
		netif_unpause_tx_queue(dev);		/* Typical path */
	dev->trans_start = jiffies;

	if (hmp->msg_level & NETIF_MSG_TX_QUEUED) {
		printk(KERN_DEBUG "%s: Hamachi transmit frame #%d length %d queued "
			   "in slot %d.\n", dev->name, hmp->cur_tx, (int)skb->len, entry);
	}
	return 0;
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void hamachi_interrupt(int irq, void *dev_instance, struct pt_regs *rgs)
{
	struct net_device *dev = (struct net_device *)dev_instance;
	struct hamachi_private *hmp;
	long ioaddr;
	int boguscnt = max_interrupt_work;

#ifndef final_version			/* Can never occur. */
	if (dev == NULL) {
		printk (KERN_ERR "hamachi_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}
#endif

	ioaddr = dev->base_addr;
	hmp = (struct hamachi_private *)dev->priv;
	if (test_and_set_bit(0, (void*)&hmp->in_interrupt)) {
		printk(KERN_ERR "%s: Re-entering the interrupt handler.\n", dev->name);
		hmp->in_interrupt = 0;	/* Avoid future hang on bug */
		return;
	}

	do {
		u32 intr_status = readl(ioaddr + InterruptClear);

		if (hmp->msg_level & NETIF_MSG_INTR)
			printk(KERN_DEBUG "%s: Hamachi interrupt, status %4.4x.\n",
				   dev->name, intr_status);

		if (intr_status == 0)
			break;

		if (intr_status & IntrRxDone)
			hamachi_rx(dev);

		for (; hmp->cur_tx - hmp->dirty_tx > 0; hmp->dirty_tx++) {
			int entry = hmp->dirty_tx % TX_RING_SIZE;
			if (!(hmp->tx_ring[entry].status_n_length & cpu_to_le32(DescOwn)))
				break;
			if (hmp->msg_level & NETIF_MSG_TX_DONE)
				printk(KERN_DEBUG "%s: Transmit done, Tx status %8.8x.\n",
					   dev->name, hmp->tx_ring[entry].status_n_length);
			/* Free the original skb. */
			dev_free_skb_irq(hmp->tx_skbuff[entry]);
			hmp->tx_skbuff[entry] = 0;
			hmp->stats.tx_packets++;
		}
		if (hmp->tx_full
			&& hmp->cur_tx - hmp->dirty_tx < TX_QUEUE_LEN - 4) {
			/* The ring is no longer full, clear tbusy. */
			hmp->tx_full = 0;
			netif_resume_tx_queue(dev);
		}

		/* Abnormal error summary/uncommon events handlers. */
		if (intr_status &
			(IntrTxPCIFault | IntrTxPCIErr | IntrRxPCIFault | IntrRxPCIErr |
			 LinkChange | NegotiationChange | StatsMax))
			hamachi_error(dev, intr_status);

		if (--boguscnt < 0) {
			printk(KERN_WARNING "%s: Too much work at interrupt, "
				   "status=0x%4.4x.\n",
				   dev->name, intr_status);
			break;
		}
	} while (1);

	if (hmp->msg_level & NETIF_MSG_INTR)
		printk(KERN_DEBUG "%s: exiting interrupt, status=%#4.4x.\n",
			   dev->name, (int)readl(ioaddr + IntrStatus));
	clear_bit(0, (void*)&hmp->in_interrupt);
	return;
}

/* This routine is logically part of the interrupt handler, but separated
   for clarity and better register allocation. */
static int hamachi_rx(struct net_device *dev)
{
	struct hamachi_private *hmp = (struct hamachi_private *)dev->priv;
	int entry = hmp->cur_rx % RX_RING_SIZE;
	int boguscnt = hmp->dirty_rx + RX_RING_SIZE - hmp->cur_rx;

	if (hmp->msg_level & NETIF_MSG_RX_STATUS) {
		printk(KERN_DEBUG " In hamachi_rx(), entry %d status %4.4x.\n",
			   entry, hmp->rx_ring[entry].status_n_length);
	}

	/* If EOP is set on the next entry, it's a new packet. Send it up. */
	while ( ! (hmp->rx_head_desc->status_n_length & cpu_to_le32(DescOwn))) {
		struct hamachi_desc *desc = hmp->rx_head_desc;
		u32 desc_status = le32_to_cpu(desc->status_n_length);
		u16 data_size = desc_status; 		/* Implicit truncate */
		u8 *buf_addr = hmp->rx_skbuff[entry]->tail;
		s32 frame_status =
			le32_to_cpu(get_unaligned((s32*)&(buf_addr[data_size - 12])));

		if (hmp->msg_level & NETIF_MSG_RX_STATUS)
			printk(KERN_DEBUG "  hamachi_rx() status was %8.8x.\n",
				   frame_status);
		if (--boguscnt < 0)
			break;
		if ( ! (desc_status & DescEndPacket)) {
			printk(KERN_WARNING "%s: Oversized Ethernet frame spanned "
				   "multiple buffers, entry %#x length %d status %4.4x!\n",
				   dev->name, hmp->cur_rx, data_size, desc_status);
			printk(KERN_WARNING "%s: Oversized Ethernet frame %p vs %p.\n",
				   dev->name, desc, &hmp->rx_ring[hmp->cur_rx % RX_RING_SIZE]);
			printk(KERN_WARNING "%s: Oversized Ethernet frame -- next status"
				   " %x last status %x.\n", dev->name,
				   hmp->rx_ring[(hmp->cur_rx+1) % RX_RING_SIZE].status_n_length,
				   hmp->rx_ring[(hmp->cur_rx-1) % RX_RING_SIZE].status_n_length);
			hmp->stats.rx_length_errors++;
		} /* else  Omit for prototype errata??? */
		if (frame_status & 0x00380000) {
			/* There was a error. */
			if (hmp->msg_level & NETIF_MSG_RX_ERR)
				printk(KERN_DEBUG "  hamachi_rx() Rx error was %8.8x.\n",
					   frame_status);
			hmp->stats.rx_errors++;
			if (frame_status & 0x00600000) hmp->stats.rx_length_errors++;
			if (frame_status & 0x00080000) hmp->stats.rx_frame_errors++;
			if (frame_status & 0x00100000) hmp->stats.rx_crc_errors++;
			if (frame_status < 0) hmp->stats.rx_dropped++;
		} else {
			struct sk_buff *skb;
			u16 pkt_len = (frame_status & 0x07ff) - 4;	/* Omit CRC */

#if ! defined(final_version)  &&  0
			if (hmp->msg_level & NETIF_MSG_RX_STATUS)
				printk(KERN_DEBUG "  hamachi_rx() normal Rx pkt length %d"
					   " of %d, bogus_cnt %d.\n",
					   pkt_len, data_size, boguscnt);
			if (hmp->msg_level & NETIF_MSG_PKTDATA)
				printk(KERN_DEBUG"%s:  rx status %8.8x %8.8x %8.8x %8.8x %8.8x.\n",
					   dev->name,
					   *(s32*)&(buf_addr[data_size - 20]),
					   *(s32*)&(buf_addr[data_size - 16]),
					   *(s32*)&(buf_addr[data_size - 12]),
					   *(s32*)&(buf_addr[data_size - 8]),
					   *(s32*)&(buf_addr[data_size - 4]));
#endif
			/* Check if the packet is long enough to accept without copying
			   to a minimally-sized skbuff. */
			if (pkt_len < rx_copybreak
				&& (skb = dev_alloc_skb(pkt_len + 2)) != NULL) {
				skb->dev = dev;
				skb_reserve(skb, 2);	/* 16 byte align the IP header */
				eth_copy_and_sum(skb, hmp->rx_skbuff[entry]->tail, pkt_len, 0);
				skb_put(skb, pkt_len);
			} else {
				char *temp = skb_put(skb = hmp->rx_skbuff[entry], pkt_len);
				hmp->rx_skbuff[entry] = NULL;
#if ! defined(final_version)
				if (bus_to_virt(desc->addr) != temp)
					printk(KERN_ERR "%s: Internal fault: The skbuff addresses "
						   "do not match in hamachi_rx: %p vs. %p / %p.\n",
						   dev->name, bus_to_virt(desc->addr),
						   skb->head, temp);
#endif
			}
			skb->protocol = eth_type_trans(skb, dev);
			/* Note: checksum -> skb->ip_summed = CHECKSUM_UNNECESSARY; */
			netif_rx(skb);
			dev->last_rx = jiffies;
			hmp->stats.rx_packets++;
		}
		entry = (++hmp->cur_rx) % RX_RING_SIZE;
		hmp->rx_head_desc = &hmp->rx_ring[entry];
	}

	/* Refill the Rx ring buffers. */
	for (; hmp->cur_rx - hmp->dirty_rx > 0; hmp->dirty_rx++) {
		struct sk_buff *skb;
		entry = hmp->dirty_rx % RX_RING_SIZE;
		if (hmp->rx_skbuff[entry] == NULL) {
			skb = dev_alloc_skb(hmp->rx_buf_sz);
			hmp->rx_skbuff[entry] = skb;
			if (skb == NULL)
				break;			/* Better luck next round. */
			skb->dev = dev;			/* Mark as being used by this device. */
			skb_reserve(skb, 2);	/* Align IP on 16 byte boundaries */
			hmp->rx_ring[entry].addr = virt_to_desc(skb->tail);
		}
		if (entry >= RX_RING_SIZE-1)		 /* Wrap ring */
			hmp->rx_ring[entry].status_n_length =
				cpu_to_le32(DescOwn|DescEndPacket|DescEndRing|DescIntr | hmp->rx_buf_sz);
		else
			hmp->rx_ring[entry].status_n_length =
				cpu_to_le32(DescOwn|DescEndPacket|DescIntr | hmp->rx_buf_sz);
	}

	/* Restart Rx engine if stopped. */
	writew(1, dev->base_addr + RxCmd);
	return 0;
}

/* This is more properly named "uncommon interrupt events", as it covers more
   than just errors. */
static void hamachi_error(struct net_device *dev, int intr_status)
{
	long ioaddr = dev->base_addr;
	struct hamachi_private *hmp = (struct hamachi_private *)dev->priv;

	if (intr_status & (LinkChange|NegotiationChange)) {
		if (hmp->msg_level & NETIF_MSG_LINK)
			printk(KERN_INFO "%s: Link changed: AutoNegotiation Ctrl"
				   " %4.4x, Status %4.4x %4.4x Intr status %4.4x.\n",
				   dev->name, (int)readw(ioaddr + 0x0E0),
				   (int)readw(ioaddr + 0x0E2),
				   (int)readw(ioaddr + ANLinkPartnerAbility),
				   (int)readl(ioaddr + IntrStatus));
		if (readw(ioaddr + ANStatus) & 0x20) {
			writeb(0x01, ioaddr + LEDCtrl);
			netif_link_up(dev);
		} else {
			writeb(0x03, ioaddr + LEDCtrl);
			netif_link_down(dev);
		}
	}
	if (intr_status & StatsMax) {
		hamachi_get_stats(dev);
		/* Read the overflow bits to clear. */
		readl(ioaddr + 0x36C);
		readl(ioaddr + 0x3F0);
	}
	if ((intr_status & ~(LinkChange|StatsMax|NegotiationChange))
		&& (hmp->msg_level & NETIF_MSG_DRV))
		printk(KERN_ERR "%s: Something Wicked happened! %4.4x.\n",
			   dev->name, intr_status);
	/* Hmmmmm, it's not clear how to recover from PCI faults. */
	if (intr_status & (IntrTxPCIErr | IntrTxPCIFault))
		hmp->stats.tx_fifo_errors++;
	if (intr_status & (IntrRxPCIErr | IntrRxPCIFault))
		hmp->stats.rx_fifo_errors++;
}

static int hamachi_close(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct hamachi_private *hmp = (struct hamachi_private *)dev->priv;
	int i;

	netif_stop_tx_queue(dev);

	if (hmp->msg_level & NETIF_MSG_IFDOWN) {
		printk(KERN_DEBUG "%s: Shutting down ethercard, status was Tx %4.4x "
			   "Rx %4.4x Int %2.2x.\n",
			   dev->name, (int)readw(ioaddr + TxStatus),
			   (int)readw(ioaddr + RxStatus), (int)readl(ioaddr + IntrStatus));
		printk(KERN_DEBUG "%s: Queue pointers were Tx %d / %d,  Rx %d / %d.\n",
			   dev->name, hmp->cur_tx, hmp->dirty_tx, hmp->cur_rx,
			   hmp->dirty_rx);
	}

	/* Disable interrupts by clearing the interrupt mask. */
	writel(0x0000, ioaddr + InterruptEnable);

	/* Stop the chip's Tx and Rx processes. */
	writel(2, ioaddr + RxCmd);
	writew(2, ioaddr + TxCmd);

	del_timer(&hmp->timer);

#ifdef __i386__
	if (hmp->msg_level & NETIF_MSG_IFDOWN) {
		printk("\n"KERN_DEBUG"  Tx ring at %8.8x:\n",
			   (int)virt_to_bus(hmp->tx_ring));
		for (i = 0; i < TX_RING_SIZE; i++)
			printk(" %c #%d desc. %8.8x %8.8x.\n",
				   readl(ioaddr + TxCurPtr) == (long)&hmp->tx_ring[i] ? '>' : ' ',
				   i, hmp->tx_ring[i].status_n_length, hmp->tx_ring[i].addr);
		printk("\n"KERN_DEBUG "  Rx ring %8.8x:\n",
			   (int)virt_to_bus(hmp->rx_ring));
		for (i = 0; i < RX_RING_SIZE; i++) {
			printk(KERN_DEBUG " %c #%d desc. %8.8x %8.8x\n",
				   readl(ioaddr + RxCurPtr) == (long)&hmp->rx_ring[i] ? '>' : ' ',
				   i, hmp->rx_ring[i].status_n_length, hmp->rx_ring[i].addr);
			if (*(u8*)hmp->rx_ring[i].addr != 0x69) {
				int j;
				for (j = 0; j < 0x50; j++)
					printk(" %4.4x", ((u16*)hmp->rx_ring[i].addr)[j]);
				printk("\n");
			}
		}
	}
#endif /* __i386__ debugging only */

	free_irq(dev->irq, dev);

	/* Free all the skbuffs in the Rx queue. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		hmp->rx_ring[i].status_n_length = 0;
		hmp->rx_ring[i].addr = 0xBADF00D0; /* An invalid address. */
		if (hmp->rx_skbuff[i]) {
#if LINUX_VERSION_CODE < 0x20100
			hmp->rx_skbuff[i]->free = 1;
#endif
			dev_free_skb(hmp->rx_skbuff[i]);
		}
		hmp->rx_skbuff[i] = 0;
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		if (hmp->tx_skbuff[i])
			dev_free_skb(hmp->tx_skbuff[i]);
		hmp->tx_skbuff[i] = 0;
	}

	writeb(0x00, ioaddr + LEDCtrl);

	MOD_DEC_USE_COUNT;

	return 0;
}

static struct net_device_stats *hamachi_get_stats(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct hamachi_private *hmp = (struct hamachi_private *)dev->priv;

	/* We should lock this segment of code for SMP eventually, although
	   the vulnerability window is very small and statistics are
	   non-critical. */
#if LINUX_VERSION_CODE >= 0x20119
	hmp->stats.rx_bytes += readl(ioaddr + 0x330); /* Total Uni+Brd+Multi */
	hmp->stats.tx_bytes += readl(ioaddr + 0x3B0); /* Total Uni+Brd+Multi */
#endif
	hmp->stats.multicast		+= readl(ioaddr + 0x320); /* Multicast Rx */

	hmp->stats.rx_length_errors	+= readl(ioaddr + 0x368); /* Over+Undersized */
	hmp->stats.rx_over_errors	+= readl(ioaddr + 0x35C); /* Jabber */
	hmp->stats.rx_crc_errors	+= readl(ioaddr + 0x360);
	hmp->stats.rx_frame_errors	+= readl(ioaddr + 0x364); /* Symbol Errs */
	hmp->stats.rx_missed_errors	+= readl(ioaddr + 0x36C); /* Dropped */

	return &hmp->stats;
}

static void set_rx_mode(struct net_device *dev)
{
	struct hamachi_private *np = (void *)dev->priv;
	long ioaddr = dev->base_addr;
	int new_rx_mode;

	if (dev->flags & IFF_PROMISC) {			/* Set promiscuous. */
		/* Unconditionally log net taps. */
		printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n", dev->name);
		new_rx_mode = 0x000F;
	} else if (dev->mc_count > np->multicast_filter_limit ||
			   (dev->flags & IFF_ALLMULTI)) {
		/* Too many to match, or accept all multicasts. */
		new_rx_mode = 0x000B;
	} else if (dev->mc_count > 0) { /* Must use the CAM filter. */
		struct dev_mc_list *mclist;
		int i;
		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
			 i++, mclist = mclist->next) {
			writel(*(u32*)(mclist->dmi_addr), ioaddr + 0x100 + i*8);
			writel(0x20000 | (*(u16*)&mclist->dmi_addr[4]),
				   ioaddr + 0x104 + i*8);
		}
		/* Clear remaining entries. */
		for (; i < 64; i++)
			writel(0, ioaddr + 0x104 + i*8);
		new_rx_mode = 0x0003;
	} else {					/* Normal, unicast/broadcast-only mode. */
		new_rx_mode = 0x0001;
	}
	if (np->rx_mode != new_rx_mode) {
		np->rx_mode = new_rx_mode;
		writew(new_rx_mode, ioaddr + AddrMode);
	}
}

static int mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct hamachi_private *np = (void *)dev->priv;
	long ioaddr = dev->base_addr;
	u16 *data = (u16 *)&rq->ifr_data;
	u32 *data32 = (void *)&rq->ifr_data;

	switch(cmd) {
	case 0x8947: case 0x89F0:
		/* SIOCGMIIPHY: Get the address of the PHY in use. */
		data[0] = np->phys[0] & 0x1f;
		/* Fall Through */
	case 0x8948: case 0x89F1:
		/* SIOCGMIIREG: Read the specified MII register. */
		data[3] = mdio_read(ioaddr, data[0] & 0x1f, data[1] & 0x1f);
		return 0;
	case 0x8949: case 0x89F2:
		/* SIOCSMIIREG: Write the specified MII register */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		/* We are always full duplex.  Skip recording the advertised value. */
		mdio_write(ioaddr, data[0] & 0x1f, data[1] & 0x1f, data[2]);
		return 0;
	case SIOCGPARAMS:
		data32[0] = np->msg_level;
		data32[1] = np->multicast_filter_limit;
		data32[2] = np->max_interrupt_work;
		data32[3] = np->rx_copybreak;
		return 0;
	case SIOCSPARAMS: {
		/* Set rx,tx intr params, from Eric Kasten. */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		np->msg_level = data32[0];
		np->max_interrupt_work = data32[2];
		writel(data32[1], dev->base_addr + TxIntrCtrl);
		writel(data32[3], dev->base_addr + RxIntrCtrl);
		printk(KERN_INFO "%s: Set interrupt mitigate paramters tx %08x, "
			   "rx %08x.\n", dev->name,
			   (int) readl(dev->base_addr + TxIntrCtrl),
			   (int) readl(dev->base_addr + RxIntrCtrl));
		return 0;
	}
	default:
		return -EOPNOTSUPP;
	}
}

#ifdef HAVE_CHANGE_MTU
static int change_mtu(struct net_device *dev, int new_mtu)
{
	if ((new_mtu < 68) || (new_mtu > 1536))
		return -EINVAL;
	if (netif_running(dev))
		return -EBUSY;
	printk(KERN_NOTICE "%s: Changing MTU to %d.\n", dev->name, new_mtu);
	dev->mtu = new_mtu;
	return 0;
}
#endif


#ifdef MODULE
int init_module(void)
{
	if (debug >= NETIF_MSG_DRV)	/* Emit version even if no cards detected. */
		printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	return pci_drv_register(&hamachi_drv_id, NULL);
}

void cleanup_module(void)
{
	struct net_device *next_dev;

	pci_drv_unregister(&hamachi_drv_id);

	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	while (root_hamachi_dev) {
		struct hamachi_private *hmp = (void *)(root_hamachi_dev->priv);
		unregister_netdev(root_hamachi_dev);
		iounmap((char *)root_hamachi_dev->base_addr);
		next_dev = hmp->next_module;
		if (hmp->priv_addr)
			kfree(hmp->priv_addr);
		kfree(root_hamachi_dev);
		root_hamachi_dev = next_dev;
	}
}

#endif  /* MODULE */

/*
 * Local variables:
 *  compile-command: "make KERNVER=`uname -r` hamachi.o"
 *  compile-cmd: "gcc -DMODULE -Wall -Wstrict-prototypes -O6 -c hamachi.c"
 *  simple-compile-command: "gcc -DMODULE -O6 -c hamachi.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
