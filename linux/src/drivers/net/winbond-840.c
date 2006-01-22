/* winbond-840.c: A Linux network device driver for the Winbond W89c840. */
/*
	Written 1998-2003 by Donald Becker.

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	914 Bay Ridge Road, Suite 220
	Annapolis MD 21403

	Support information and updates available at
		http://www.scyld.com/network/drivers.html
	The information and support mailing lists are based at
		http://www.scyld.com/mailman/listinfo/

	Do not remove the copyright infomation.
	Do not change the version information unless an improvement has been made.
	Merely removing my name, as Compex has done in the past, does not count
	as an improvement.
*/

/* These identify the driver base version and may not be removed. */
static const char version1[] =
"winbond-840.c:v1.10 7/22/2003  Donald Becker <becker@scyld.com>\n";
static const char version2[] =
"  http://www.scyld.com/network/drivers.html\n";

/* Automatically extracted configuration info:
probe-func: winbond840_probe
config-in: tristate 'Winbond W89c840 Ethernet support' CONFIG_WINBOND_840

c-help-name: Winbond W89c840 PCI Ethernet support
c-help-symbol: CONFIG_WINBOND_840
c-help: The winbond-840.c driver is for the Winbond W89c840 chip.
c-help: This chip is named TX9882 on the Compex RL100-ATX board.
c-help: More specific information and updates are available from
c-help: http://www.scyld.com/network/drivers.html
*/

/* The user-configurable values.
   These may be modified when a driver module is loaded.*/

/* Message enable level: 0..31 = no..all messages.  See NETIF_MSG docs. */
static int debug = 2;

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The '840 uses a 64 element hash table based on the Ethernet CRC.  */
static int multicast_filter_limit = 32;

/* Set the copy breakpoint for the copy-only-tiny-frames scheme.
   Setting to > 1518 effectively disables this feature. */
static int rx_copybreak = 0;

/* Used to pass the media type, etc.
   Both 'options[]' and 'full_duplex[]' should exist for driver
   interoperability, however setting full_duplex[] is deprecated.
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

/* Keep the ring sizes a power of two for compile efficiency.
   The compiler will convert <unsigned>'%'<2^N> into a bit mask.
   Making the Tx ring too large decreases the effectiveness of channel
   bonding and packet priority, confuses the system network buffer limits,
   and wastes memory.
   Larger receive rings merely waste memory.
*/
#define TX_RING_SIZE	16
#define TX_QUEUE_LEN	10		/* Limit ring entries actually used, min 4. */
#define RX_RING_SIZE	32

/* The presumed FIFO size for working around the Tx-FIFO-overflow bug.
   To avoid overflowing we don't queue again until we have room for a
   full-size packet.
 */
#define TX_FIFO_SIZE (2048)
#define TX_BUG_FIFO_LIMIT (TX_FIFO_SIZE-1514-16)

/* Operational parameters that usually are not changed. */
/* Time in jiffies before concluding the transmitter is hung.
   Re-autonegotiation may take up to 3 seconds.
 */
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

/* Configure the PCI bus bursts and FIFO thresholds.
	   486: Set 8 longword cache alignment, 8 longword burst.
	   586: Set 16 longword cache alignment, no burst limit.
	   Cache alignment bits 15:14	     Burst length 13:8
		0000	<not allowed> 		0000 align to cache	0800 8 longwords
		4000	8  longwords		0100 1 longword		1000 16 longwords
		8000	16 longwords		0200 2 longwords	2000 32 longwords
		C000	32  longwords		0400 4 longwords
	Wait the specified 50 PCI cycles after a reset by initializing
	Tx and Rx queues and the address filter list. */
#define TX_DESC_SIZE	16
#if defined(__powerpc__) || defined(__sparc__)		/* Big endian */
static int csr0 = 0x00100000 | 0xE000 | TX_DESC_SIZE;
#elif defined(__alpha__) || defined(__x86_64) || defined(__ia64)
static int csr0 = 0xE000 | TX_DESC_SIZE;
#elif defined(__i386__)
static int csr0 = 0xE000 | TX_DESC_SIZE;
#else
static int csr0 = 0xE000 | TX_DESC_SIZE;
#warning Processor architecture unknown!
#endif



#if (LINUX_VERSION_CODE >= 0x20100)  &&  defined(MODULE)
char kernel_version[] = UTS_RELEASE;
#endif

MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("Winbond W89c840 Ethernet driver");
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
MODULE_PARM_DESC(full_duplex, "Non-zero to set forced full duplex.");
MODULE_PARM_DESC(rx_copybreak,
				 "Breakpoint in bytes for copy-only-tiny-frames");
MODULE_PARM_DESC(multicast_filter_limit,
				 "Multicast addresses before switching to Rx-all-multicast");

/*
				Theory of Operation

I. Board Compatibility

This driver is for the Winbond w89c840 chip.

II. Board-specific settings

None.

III. Driver operation

This chip is very similar to the Digital 21*4* "Tulip" family.  The first
twelve registers and the descriptor format are nearly identical.  Read a
Tulip manual for operational details.

A significant difference is that the multicast filter and station address are
stored in registers rather than loaded through a pseudo-transmit packet.

Unlike the Tulip, transmit buffers are limited to 1KB.  To transmit a
full-sized packet we must use both data buffers in a descriptor.  Thus the
driver uses ring mode where descriptors are implicitly sequential in memory,
rather than using the second descriptor address as a chain pointer to
subsequent descriptors.

IV. Notes

If you are going to almost clone a Tulip, why not go all the way and avoid
the need for a new driver?

IVb. References

http://www.scyld.com/expert/100mbps.html
http://www.scyld.com/expert/NWay.html
http://www.winbond.com.tw/

IVc. Errata

A horrible bug exists in the transmit FIFO.  Apparently the chip doesn't
correctly detect a full FIFO, and queuing more than 2048 bytes may result in
silent data corruption.

*/



/*
  PCI probe table.
*/
static void *w840_probe1(struct pci_dev *pdev, void *init_dev,
						 long ioaddr, int irq, int chip_idx, int find_cnt);
static int winbond_pwr_event(void *dev_instance, int event);
enum chip_capability_flags {
	CanHaveMII=1, HasBrokenTx=2, AlwaysFDX=4, FDXOnNoMII=8,};
#ifdef USE_IO_OPS
#define W840_FLAGS (PCI_USES_IO | PCI_ADDR0 | PCI_USES_MASTER)
#else
#define W840_FLAGS (PCI_USES_MEM | PCI_ADDR1 | PCI_USES_MASTER)
#endif

static struct pci_id_info pci_id_tbl[] = {
	{"Winbond W89c840",			/* Sometime a Level-One switch card. */
	 { 0x08401050, 0xffffffff, 0x81530000, 0xffff0000 },
	 W840_FLAGS, 128, CanHaveMII | HasBrokenTx | FDXOnNoMII},
	{"Winbond W89c840", { 0x08401050, 0xffffffff, },
	 W840_FLAGS, 128, CanHaveMII | HasBrokenTx},
	{"Compex RL100-ATX", { 0x201111F6, 0xffffffff,},
	 W840_FLAGS, 128, CanHaveMII | HasBrokenTx},
	{0,},						/* 0 terminated list. */
};

struct drv_id_info winbond840_drv_id = {
	"winbond-840", PCI_HOTSWAP, PCI_CLASS_NETWORK_ETHERNET<<8, pci_id_tbl,
	w840_probe1, winbond_pwr_event };

/* This driver was written to use PCI memory space, however some x86 systems
   work only with I/O space accesses.  Pass -DUSE_IO_OPS to use PCI I/O space
   accesses instead of memory space. */

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

/* Offsets to the Command and Status Registers, "CSRs".
   While similar to the Tulip, these registers are longword aligned.
   Note: It's not useful to define symbolic names for every register bit in
   the device.  The name can only partially document the semantics and make
   the driver longer and more difficult to read.
*/
enum w840_offsets {
	PCIBusCfg=0x00, TxStartDemand=0x04, RxStartDemand=0x08,
	RxRingPtr=0x0C, TxRingPtr=0x10,
	IntrStatus=0x14, NetworkConfig=0x18, IntrEnable=0x1C,
	RxMissed=0x20, EECtrl=0x24, MIICtrl=0x24, BootRom=0x28, GPTimer=0x2C,
	CurRxDescAddr=0x30, CurRxBufAddr=0x34,			/* Debug use */
	MulticastFilter0=0x38, MulticastFilter1=0x3C, StationAddr=0x40,
	CurTxDescAddr=0x4C, CurTxBufAddr=0x50,
};

/* Bits in the interrupt status/enable registers. */
/* The bits in the Intr Status/Enable registers, mostly interrupt sources. */
enum intr_status_bits {
	NormalIntr=0x10000, AbnormalIntr=0x8000,
	IntrPCIErr=0x2000, TimerInt=0x800,
	IntrRxDied=0x100, RxNoBuf=0x80, IntrRxDone=0x40,
	TxFIFOUnderflow=0x20, RxErrIntr=0x10,
	TxIdle=0x04, IntrTxStopped=0x02, IntrTxDone=0x01,
};

/* Bits in the NetworkConfig register. */
enum rx_mode_bits {
	TxOn=0x2000, RxOn=0x0002, FullDuplex=0x0200,
	AcceptErr=0x80, AcceptRunt=0x40, 		/* Not used */
	AcceptBroadcast=0x20, AcceptMulticast=0x10, AcceptAllPhys=0x08,
};

enum mii_reg_bits {
	MDIO_ShiftClk=0x10000, MDIO_DataIn=0x80000, MDIO_DataOut=0x20000,
	MDIO_EnbOutput=0x40000, MDIO_EnbIn = 0x00000,
};

/* The Tulip-like Rx and Tx buffer descriptors. */
struct w840_rx_desc {
	s32 status;
	s32 length;
	u32 buffer1;
	u32 next_desc;
};

struct w840_tx_desc {
	s32 status;
	s32 length;
	u32 buffer1, buffer2;				/* We use only buffer 1.  */
	char pad[TX_DESC_SIZE - 16];
};

/* Bits in network_desc.status */
enum desc_status_bits {
	DescOwn=0x80000000, DescEndRing=0x02000000, DescUseLink=0x01000000,
	DescWholePkt=0x60000000, DescStartPkt=0x20000000, DescEndPkt=0x40000000,
	DescIntr=0x80000000,
};

#define PRIV_ALIGN	15 	/* Required alignment mask */
struct netdev_private {
	/* Descriptor rings first for alignment. */
	struct w840_rx_desc rx_ring[RX_RING_SIZE];
	struct w840_tx_desc tx_ring[TX_RING_SIZE];
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
	int csr0, csr6;
	unsigned int polling;				/* Switched to polling mode. */
	int max_interrupt_work;

	struct w840_rx_desc *rx_head_desc;
	unsigned int rx_ring_size;
	unsigned int cur_rx, dirty_rx;		/* Producer/consumer ring indices */
	unsigned int rx_buf_sz;				/* Based on MTU+slack. */
	int rx_copybreak;

	unsigned int tx_ring_size;
	unsigned int cur_tx, dirty_tx;
	unsigned int tx_q_bytes, tx_unq_bytes;
	unsigned int tx_full:1;				/* The Tx queue is full. */

	/* These values track of the transceiver/media in use. */
	unsigned int full_duplex:1;			/* Full-duplex operation requested. */
	unsigned int duplex_lock:1;
	unsigned int medialock:1;			/* Do not sense media. */
	unsigned int default_port;			/* Last dev->if_port value. */
	/* Rx filter. */
	u32 cur_rx_mode;
	u32 rx_filter[2];
	int multicast_filter_limit;

	/* MII transceiver section. */
	int mii_cnt;						/* MII device addresses. */
	u16 advertising;					/* NWay media advertisement */
	unsigned char phys[2];				/* MII device addresses. */
};

static int  eeprom_read(long ioaddr, int location);
static int  mdio_read(struct net_device *dev, int phy_id, int location);
static void mdio_write(struct net_device *dev, int phy_id, int location, int value);
static int  netdev_open(struct net_device *dev);
static void check_duplex(struct net_device *dev);
static void netdev_timer(unsigned long data);
static void tx_timeout(struct net_device *dev);
static void init_ring(struct net_device *dev);
static int  start_tx(struct sk_buff *skb, struct net_device *dev);
static void intr_handler(int irq, void *dev_instance, struct pt_regs *regs);
static void netdev_error(struct net_device *dev, int intr_status);
static int  netdev_rx(struct net_device *dev);
static void netdev_error(struct net_device *dev, int intr_status);
static inline unsigned ether_crc(int length, unsigned char *data);
static void set_rx_mode(struct net_device *dev);
static struct net_device_stats *get_stats(struct net_device *dev);
static int mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static int  netdev_close(struct net_device *dev);



/* A list of our installed devices, for removing the driver module. */
static struct net_device *root_net_dev = NULL;

static void *w840_probe1(struct pci_dev *pdev, void *init_dev,
						 long ioaddr, int irq, int chip_idx, int card_idx)
{
	struct net_device *dev;
	struct netdev_private *np;
	void *priv_mem;
	int i, option = card_idx < MAX_UNITS ? options[card_idx] : 0;

	dev = init_etherdev(init_dev, 0);
	if (!dev)
		return NULL;

#if LINUX_VERSION_CODE < 0x20155
	printk(KERN_INFO "%s: %s at 0x%lx, %2.2x:%2.2x",
		   dev->name, pci_id_tbl[chip_idx].name, ioaddr,
		   pci_bus_number(pdev), pci_devfn(pdev)>>3);
#else
	printk(KERN_INFO "%s: %s at 0x%lx, %2.2x:%2.2x",
		   dev->name, pci_id_tbl[chip_idx].name, ioaddr,
		   pdev->bus->number, pdev->devfn>>3);
#endif

	/* Warning: validate for big-endian machines. */
	for (i = 0; i < 3; i++)
		((u16 *)dev->dev_addr)[i] = le16_to_cpu(eeprom_read(ioaddr, i));

	for (i = 0; i < 5; i++)
		printk("%2.2x:", dev->dev_addr[i]);
	printk("%2.2x, IRQ %d.\n", dev->dev_addr[i], irq);

	priv_mem = kmalloc(sizeof(*np) + PRIV_ALIGN, GFP_KERNEL);
	/* Out of memory is very unlikely. */
	if (priv_mem == NULL)
		return NULL;

#ifdef USE_IO_OPS
	request_region(ioaddr, pci_id_tbl[chip_idx].io_size, dev->name);
#endif

	/* Reset the chip to erase previous misconfiguration.
	   No hold time required! */
	writel(0x00000001, ioaddr + PCIBusCfg);

	dev->base_addr = ioaddr;
	dev->irq = irq;

	/* The descriptor lists must be aligned. */
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
	np->tx_ring_size = TX_RING_SIZE;
	np->rx_ring_size = RX_RING_SIZE;

	if (dev->mem_start)
		option = dev->mem_start;

	if ((card_idx < MAX_UNITS  &&  full_duplex[card_idx] > 0)
		|| (np->drv_flags & AlwaysFDX))
		np->full_duplex = 1;

	/* The chip-specific entries in the device structure. */
	dev->open = &netdev_open;
	dev->hard_start_xmit = &start_tx;
	dev->stop = &netdev_close;
	dev->get_stats = &get_stats;
	dev->set_multicast_list = &set_rx_mode;
	dev->do_ioctl = &mii_ioctl;

	if (np->drv_flags & CanHaveMII) {
		int phy, phy_idx = 0;
		for (phy = 1; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(dev, phy, 1);
			if (mii_status != 0xffff  &&  mii_status != 0x0000) {
				np->phys[phy_idx++] = phy;
				np->advertising = mdio_read(dev, phy, 4);
				printk(KERN_INFO "%s: MII PHY found at address %d, status "
					   "0x%4.4x advertising %4.4x.\n",
					   dev->name, phy, mii_status, np->advertising);
			}
		}
		np->mii_cnt = phy_idx;
		if (phy_idx == 0) {
			printk(KERN_WARNING "%s: MII PHY not found -- this device may "
				   "not operate correctly.\n"
				   KERN_WARNING "%s:  If this is a switch card, explicitly "
				   "force full duplex on this interface.\n",
				   dev->name, dev->name);
			if (np->drv_flags & FDXOnNoMII) {
				printk(KERN_INFO "%s:  Assuming a switch card, forcing full "
					   "duplex.\n", dev->name);
				np->full_duplex = np->duplex_lock = 1;
			}
		}
	}
	/* Allow forcing the media type. */
	if (np->full_duplex) {
		printk(KERN_INFO "%s: Set to forced full duplex, autonegotiation"
			   " disabled.\n", dev->name);
		np->duplex_lock = 1;
	}
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


/* Read the EEPROM and MII Management Data I/O (MDIO) interfaces.
   The Winbond NIC uses serial bit streams generated by the host processor. */

/* Delay between EEPROM clock transitions.
   This "delay" is to force out buffered PCI writes. */
#define eeprom_delay(ee_addr)	readl(ee_addr)

enum EEPROM_Ctrl_Bits {
	EE_ShiftClk=0x02, EE_Write0=0x801, EE_Write1=0x805,
	EE_ChipSelect=0x801, EE_DataIn=0x08,
};

/* The EEPROM commands always start with 01.. preamble bits.
   Commands are prepended to the variable-length address. */
enum EEPROM_Cmds {
	EE_WriteCmd=(5 << 6), EE_ReadCmd=(6 << 6), EE_EraseCmd=(7 << 6),
};

static int eeprom_read(long addr, int location)
{
	int i;
	int retval = 0;
	long ee_addr = addr + EECtrl;
	int read_cmd = location | EE_ReadCmd;

	writel(EE_ChipSelect, ee_addr);
	/* Shift the read command bits out. */
	for (i = 10; i >= 0; i--) {
		short dataval = (read_cmd & (1 << i)) ? EE_Write1 : EE_Write0;
		writel(dataval, ee_addr);
		eeprom_delay(ee_addr);
		writel(dataval | EE_ShiftClk, ee_addr);
		eeprom_delay(ee_addr);
	}
	writel(EE_ChipSelect, ee_addr);
	eeprom_delay(ee_addr);

	for (i = 16; i > 0; i--) {
		writel(EE_ChipSelect | EE_ShiftClk, ee_addr);
		eeprom_delay(ee_addr);
		retval = (retval << 1) | ((readl(ee_addr) & EE_DataIn) ? 1 : 0);
		writel(EE_ChipSelect, ee_addr);
		eeprom_delay(ee_addr);
	}

	/* Terminate the EEPROM access. */
	writel(0, ee_addr);
	return retval;
}

/*  MII transceiver control section.
	Read and write the MII registers using software-generated serial
	MDIO protocol.  See the MII specifications or DP83840A data sheet
	for details.

	The maximum data clock rate is 2.5 Mhz.
	The timing is decoupled from the processor clock by flushing the write
	from the CPU write buffer with a following read, and using PCI
	transaction time. */
#define mdio_in(mdio_addr) readl(mdio_addr)
#define mdio_out(value, mdio_addr) writel(value, mdio_addr)
#define mdio_delay(mdio_addr) readl(mdio_addr)

/* Set iff a MII transceiver on any interface requires mdio preamble.
   This only set with older tranceivers, so the extra
   code size of a per-interface flag is not worthwhile. */
static char mii_preamble_required = 1;

#define MDIO_WRITE0 (MDIO_EnbOutput)
#define MDIO_WRITE1 (MDIO_DataOut | MDIO_EnbOutput)

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

static int mdio_read(struct net_device *dev, int phy_id, int location)
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
	for (i = 20; i > 0; i--) {
		mdio_out(MDIO_EnbIn, mdio_addr);
		mdio_delay(mdio_addr);
		retval = (retval << 1) | ((mdio_in(mdio_addr) & MDIO_DataIn) ? 1 : 0);
		mdio_out(MDIO_EnbIn | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
	return (retval>>1) & 0xffff;
}

static void mdio_write(struct net_device *dev, int phy_id, int reg, int value)
{
	long mdio_addr = dev->base_addr + MIICtrl;
	int mii_cmd = (0x5002 << 16) | (phy_id << 23) | (reg<<18) | value;
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
	int i;

	writel(0x00000001, ioaddr + PCIBusCfg);		/* Reset */

	MOD_INC_USE_COUNT;

	if (request_irq(dev->irq, &intr_handler, SA_SHIRQ, dev->name, dev)) {
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}

	if (np->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: w89c840_open() irq %d.\n",
			   dev->name, dev->irq);

	init_ring(dev);

	writel(virt_to_bus(np->rx_ring), ioaddr + RxRingPtr);
	writel(virt_to_bus(np->tx_ring), ioaddr + TxRingPtr);

	for (i = 0; i < 6; i++)
		writeb(dev->dev_addr[i], ioaddr + StationAddr + i);

	/* Initialize other registers. */
	np->csr0 = csr0;
	writel(np->csr0, ioaddr + PCIBusCfg);

	if (dev->if_port == 0)
		dev->if_port = np->default_port;

	writel(0, ioaddr + RxStartDemand);
	np->csr6 = np->full_duplex ? 0x20022202 : 0x20022002;
	check_duplex(dev);
	set_rx_mode(dev);

	netif_start_tx_queue(dev);

	/* Clear and Enable interrupts by setting the interrupt mask.
	   See enum intr_status_bits above for bit guide.
	   We omit: TimerInt, IntrRxDied, IntrTxStopped
	*/
	writel(0x1A0F5, ioaddr + IntrStatus);
	writel(0x1A0F5, ioaddr + IntrEnable);

	if (np->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: Done netdev_open().\n", dev->name);

	/* Set the timer to check for link beat. */
	init_timer(&np->timer);
	np->timer.expires = jiffies + 3*HZ;
	np->timer.data = (unsigned long)dev;
	np->timer.function = &netdev_timer;				/* timer handler */
	add_timer(&np->timer);

	return 0;
}

static void check_duplex(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	int mii_reg5 = mdio_read(dev, np->phys[0], 5);
	int negotiated =  mii_reg5 & np->advertising;
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
		np->csr6 &= ~0x200;
		np->csr6 |= duplex ? 0x200 : 0;
	}
}

static void netdev_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int next_tick = 10*HZ;
	int old_csr6 = np->csr6;
	u32 intr_status = readl(ioaddr + IntrStatus);

	if (np->msg_level & NETIF_MSG_TIMER)
		printk(KERN_DEBUG "%s: Media selection timer tick, status %8.8x "
			   "config %8.8x.\n",
			   dev->name, intr_status, (int)readl(ioaddr + NetworkConfig));
	/* Check for blocked interrupts. */
	if (np->polling) {
		if (intr_status & 0x1ffff) {
			intr_handler(dev->irq, dev, 0);
			next_tick = 1;
			np->polling = 1;
		} else if (++np->polling > 10*HZ)
			np->polling = 0;
		else
			next_tick = 2;
	} else if ((intr_status & 0x1ffff)) {
		np->polling = 1;
	}

	if (netif_queue_paused(dev)  &&
		np->cur_tx - np->dirty_tx > 1  &&
		(jiffies - dev->trans_start) > TX_TIMEOUT) {
		tx_timeout(dev);
	}
	check_duplex(dev);
	if (np->csr6 != old_csr6) {
		writel(np->csr6 & ~0x0002, ioaddr + NetworkConfig);
		writel(np->csr6 | 0x2002, ioaddr + NetworkConfig);
	}
	np->timer.expires = jiffies + next_tick;
	add_timer(&np->timer);
}

static void tx_timeout(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;

	printk(KERN_WARNING "%s: Transmit timed out, status %8.8x,"
		   " resetting...\n", dev->name, (int)readl(ioaddr + IntrStatus));

#ifndef __alpha__
	if (np->msg_level & NETIF_MSG_TX_ERR) {
		int i;
		printk(KERN_DEBUG "  Rx ring %p: ", np->rx_ring);
		for (i = 0; i < np->rx_ring_size; i++)
			printk(" %8.8x", (unsigned int)np->rx_ring[i].status);
		printk("\n"KERN_DEBUG"  Tx ring %p: ", np->tx_ring);
		for (i = 0; i < np->tx_ring_size; i++)
			printk(" %8.8x", np->tx_ring[i].status);
		printk("\n");
	}
#endif

	/* Perhaps we should reinitialize the hardware here.  Just trigger a
	   Tx demand for now. */
	writel(0, ioaddr + TxStartDemand);
	dev->if_port = 0;
	/* Stop and restart the chip's Tx processes . */

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
	np->cur_tx = np->dirty_tx = 0;
	np->tx_q_bytes = np->tx_unq_bytes = 0;

	np->cur_rx = np->dirty_rx = 0;
	np->rx_buf_sz = (dev->mtu <= 1522 ? PKT_BUF_SZ : dev->mtu + 14);
	np->rx_head_desc = &np->rx_ring[0];

	/* Initialize all Rx descriptors. */
	for (i = 0; i < np->rx_ring_size; i++) {
		np->rx_ring[i].length = np->rx_buf_sz;
		np->rx_ring[i].status = 0;
		np->rx_ring[i].next_desc = virt_to_bus(&np->rx_ring[i+1]);
		np->rx_skbuff[i] = 0;
	}
	/* Mark the last entry as wrapping the ring. */
	np->rx_ring[i-1].length |= DescEndRing;
	np->rx_ring[i-1].next_desc = virt_to_bus(&np->rx_ring[0]);

	/* Fill in the Rx buffers.  Handle allocation failure gracefully. */
	for (i = 0; i < np->rx_ring_size; i++) {
		struct sk_buff *skb = dev_alloc_skb(np->rx_buf_sz);
		np->rx_skbuff[i] = skb;
		if (skb == NULL)
			break;
		skb->dev = dev;			/* Mark as being used by this device. */
		np->rx_ring[i].buffer1 = virt_to_bus(skb->tail);
		np->rx_ring[i].status = DescOwn | DescIntr;
	}
	np->dirty_rx = (unsigned int)(i - np->rx_ring_size);

	for (i = 0; i < np->tx_ring_size; i++) {
		np->tx_skbuff[i] = 0;
		np->tx_ring[i].status = 0;
	}
	return;
}

static int start_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
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
	entry = np->cur_tx % np->tx_ring_size;

	np->tx_skbuff[entry] = skb;
	np->tx_ring[entry].buffer1 = virt_to_bus(skb->data);

#define one_buffer
#define BPT 1022
#if defined(one_buffer)
	np->tx_ring[entry].length = DescWholePkt | skb->len;
	if (entry >= np->tx_ring_size-1)		 /* Wrap ring */
		np->tx_ring[entry].length |= DescIntr | DescEndRing;
	np->tx_ring[entry].status = DescOwn;
	np->cur_tx++;
#elif defined(two_buffer)
	if (skb->len > BPT) {
		unsigned int entry1 = ++np->cur_tx % np->tx_ring_size;
		np->tx_ring[entry].length = DescStartPkt | BPT;
		np->tx_ring[entry1].length = DescEndPkt | (skb->len - BPT);
		np->tx_ring[entry1].buffer1 = virt_to_bus((skb->data) + BPT);
		np->tx_ring[entry1].status = DescOwn;
		np->tx_ring[entry].status = DescOwn;
		if (entry >= np->tx_ring_size-1)
			np->tx_ring[entry].length |= DescIntr|DescEndRing;
		else if (entry1 >= np->tx_ring_size-1)
			np->tx_ring[entry1].length |= DescIntr|DescEndRing;
		np->cur_tx++;
	} else {
		np->tx_ring[entry].length = DescWholePkt | skb->len;
		if (entry >= np->tx_ring_size-1)		 /* Wrap ring */
			np->tx_ring[entry].length |= DescIntr | DescEndRing;
		np->tx_ring[entry].status = DescOwn;
		np->cur_tx++;
	}
#elif defined(split_buffer)
	{
		/* Work around the Tx-FIFO-full bug by splitting our transmit packet
		   into two pieces, the first which may be loaded without overflowing
		   the FIFO, and the second which contains the remainder of the
		   packet.  When we get a Tx-done interrupt that frees enough room
		   in the FIFO we mark the remainder of the packet as loadable.

		   This has the problem that the Tx descriptors are written both
		   here and in the interrupt handler.
		*/

		int buf1size = TX_FIFO_SIZE - (np->tx_q_bytes - np->tx_unq_bytes);
		int buf2size = skb->len - buf1size;

		if (buf2size <= 0) {		/* We fit into one descriptor. */
			np->tx_ring[entry].length = DescWholePkt | skb->len;
		} else {				/* We must use two descriptors. */
			unsigned int entry2;
			np->tx_ring[entry].length = DescIntr | DescStartPkt | buf1size;
			if (entry >= np->tx_ring_size-1) {		 /* Wrap ring */
				np->tx_ring[entry].length |= DescEndRing;
				entry2 = 0;
			} else
				entry2 = entry + 1;
			np->cur_tx++;
			np->tx_ring[entry2].buffer1 =
				virt_to_bus(skb->data + buf1size);
			np->tx_ring[entry2].length = DescEndPkt | buf2size;
			if (entry2 >= np->tx_ring_size-1)		 /* Wrap ring */
				np->tx_ring[entry2].length |= DescEndRing;
		}
		np->tx_ring[entry].status = DescOwn;
		np->cur_tx++;
	}
#endif
	np->tx_q_bytes += skb->len;
	writel(0, dev->base_addr + TxStartDemand);

	/* Work around horrible bug in the chip by marking the queue as full
	   when we do not have FIFO room for a maximum sized packet. */
	if (np->cur_tx - np->dirty_tx > TX_QUEUE_LEN) {
		np->tx_full = 1;
		netif_stop_tx_queue(dev);
	} else if ((np->drv_flags & HasBrokenTx)
			   && np->tx_q_bytes - np->tx_unq_bytes > TX_BUG_FIFO_LIMIT) {
		np->tx_full = 1;
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
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int work_limit = np->max_interrupt_work;

	do {
		u32 intr_status = readl(ioaddr + IntrStatus);

		/* Acknowledge all of the current interrupt sources ASAP. */
		writel(intr_status & 0x0001ffff, ioaddr + IntrStatus);

		if (np->msg_level & NETIF_MSG_INTR)
			printk(KERN_DEBUG "%s: Interrupt, status %4.4x.\n",
				   dev->name, intr_status);

		if ((intr_status & (NormalIntr|AbnormalIntr)) == 0
			|| intr_status == 0xffffffff)
			break;

		if (intr_status & (IntrRxDone | RxNoBuf))
			netdev_rx(dev);

		for (; np->cur_tx - np->dirty_tx > 0; np->dirty_tx++) {
			int entry = np->dirty_tx % np->tx_ring_size;
			int tx_status = np->tx_ring[entry].status;

			if (tx_status < 0)
				break;
			if (np->msg_level & NETIF_MSG_TX_DONE)
				printk(KERN_DEBUG "%s: Transmit done, Tx status %8.8x.\n",
					   dev->name, tx_status);
			if (tx_status & 0x8000) { 		/* There was an error, log it. */
				if (np->msg_level & NETIF_MSG_TX_ERR)
					printk(KERN_DEBUG "%s: Transmit error, Tx status %8.8x.\n",
						   dev->name, tx_status);
				np->stats.tx_errors++;
				if (tx_status & 0x0104) np->stats.tx_aborted_errors++;
				if (tx_status & 0x0C80) np->stats.tx_carrier_errors++;
				if (tx_status & 0x0200) np->stats.tx_window_errors++;
				if (tx_status & 0x0002) np->stats.tx_fifo_errors++;
				if ((tx_status & 0x0080) && np->full_duplex == 0)
					np->stats.tx_heartbeat_errors++;
#ifdef ETHER_STATS
				if (tx_status & 0x0100) np->stats.collisions16++;
#endif
			} else {
#ifdef ETHER_STATS
				if (tx_status & 0x0001) np->stats.tx_deferred++;
#endif
#if LINUX_VERSION_CODE > 0x20127
				np->stats.tx_bytes += np->tx_skbuff[entry]->len;
#endif
				np->stats.collisions += (tx_status >> 3) & 15;
				np->stats.tx_packets++;
			}
			/* Free the original skb. */
			np->tx_unq_bytes += np->tx_skbuff[entry]->len;
			dev_free_skb_irq(np->tx_skbuff[entry]);
			np->tx_skbuff[entry] = 0;
		}
		if (np->tx_full &&
			np->cur_tx - np->dirty_tx < TX_QUEUE_LEN - 4
			&&  np->tx_q_bytes - np->tx_unq_bytes < TX_BUG_FIFO_LIMIT) {
			/* The ring is no longer full, allow new TX entries. */
			np->tx_full = 0;
			netif_resume_tx_queue(dev);
		}

		/* Abnormal error summary/uncommon events handlers. */
		if (intr_status & (AbnormalIntr | TxFIFOUnderflow | IntrPCIErr |
						   TimerInt | IntrTxStopped))
			netdev_error(dev, intr_status);

		if (--work_limit < 0) {
			printk(KERN_WARNING "%s: Too much work at interrupt, "
				   "status=0x%4.4x.\n", dev->name, intr_status);
			/* Set the timer to re-enable the other interrupts after
			   10*82usec ticks. */
			writel(AbnormalIntr | TimerInt, ioaddr + IntrEnable);
			writel(10, ioaddr + GPTimer);
			break;
		}
	} while (1);

	if (np->msg_level & NETIF_MSG_INTR)
		printk(KERN_DEBUG "%s: exiting interrupt, status=%#4.4x.\n",
			   dev->name, (int)readl(ioaddr + IntrStatus));

	return;
}

/* This routine is logically part of the interrupt handler, but separated
   for clarity and better register allocation. */
static int netdev_rx(struct net_device *dev)
{
	struct netdev_private *np = (struct netdev_private *)dev->priv;
	int entry = np->cur_rx % np->rx_ring_size;
	int work_limit = np->dirty_rx + np->rx_ring_size - np->cur_rx;

	if (np->msg_level & NETIF_MSG_RX_STATUS) {
		printk(KERN_DEBUG " In netdev_rx(), entry %d status %4.4x.\n",
			   entry, np->rx_ring[entry].status);
	}

	/* If EOP is set on the next entry, it's a new packet. Send it up. */
	while (--work_limit >= 0) {
		struct w840_rx_desc *desc = np->rx_head_desc;
		s32 status = desc->status;

		if (np->msg_level & NETIF_MSG_RX_STATUS)
			printk(KERN_DEBUG "  netdev_rx() status was %8.8x.\n",
				   status);
		if (status < 0)
			break;
		if ((status & 0x38008300) != 0x0300) {
			if ((status & 0x38000300) != 0x0300) {
				/* Ingore earlier buffers. */
				if ((status & 0xffff) != 0x7fff) {
					printk(KERN_WARNING "%s: Oversized Ethernet frame spanned "
						   "multiple buffers, entry %#x status %4.4x!\n",
						   dev->name, np->cur_rx, status);
					np->stats.rx_length_errors++;
				}
			} else if (status & 0x8000) {
				/* There was a fatal error. */
				if (np->msg_level & NETIF_MSG_RX_ERR)
					printk(KERN_DEBUG "%s: Receive error, Rx status %8.8x.\n",
						   dev->name, status);
				np->stats.rx_errors++; /* end of a packet.*/
				if (status & 0x0890) np->stats.rx_length_errors++;
				if (status & 0x004C) np->stats.rx_frame_errors++;
				if (status & 0x0002) np->stats.rx_crc_errors++;
			}
		} else {
			struct sk_buff *skb;
			/* Omit the four octet CRC from the length. */
			int pkt_len = ((status >> 16) & 0x7ff) - 4;

			if (np->msg_level & NETIF_MSG_RX_STATUS)
				printk(KERN_DEBUG "  netdev_rx() normal Rx pkt length %d"
					   " status %x.\n", pkt_len, status);
			/* Check if the packet is long enough to accept without copying
			   to a minimally-sized skbuff. */
			if (pkt_len < np->rx_copybreak
				&& (skb = dev_alloc_skb(pkt_len + 2)) != NULL) {
				skb->dev = dev;
				skb_reserve(skb, 2);	/* 16 byte align the IP header */
				/* Call copy + cksum if available. */
#if HAS_IP_COPYSUM
				eth_copy_and_sum(skb, np->rx_skbuff[entry]->tail, pkt_len, 0);
				skb_put(skb, pkt_len);
#else
				memcpy(skb_put(skb, pkt_len), np->rx_skbuff[entry]->tail,
					   pkt_len);
#endif
			} else {
				char *temp = skb_put(skb = np->rx_skbuff[entry], pkt_len);
				np->rx_skbuff[entry] = NULL;
#ifndef final_version				/* Remove after testing. */
				if (bus_to_virt(desc->buffer1) != temp)
					printk(KERN_ERR "%s: Internal fault: The skbuff addresses "
						   "do not match in netdev_rx: %p vs. %p / %p.\n",
						   dev->name, bus_to_virt(desc->buffer1),
						   skb->head, temp);
#endif
			}
			skb->protocol = eth_type_trans(skb, dev);
			netif_rx(skb);
			dev->last_rx = jiffies;
			np->stats.rx_packets++;
#if LINUX_VERSION_CODE > 0x20127
			np->stats.rx_bytes += pkt_len;
#endif
		}
		entry = (++np->cur_rx) % np->rx_ring_size;
		np->rx_head_desc = &np->rx_ring[entry];
	}

	/* Refill the Rx ring buffers. */
	for (; np->cur_rx - np->dirty_rx > 0; np->dirty_rx++) {
		struct sk_buff *skb;
		entry = np->dirty_rx % np->rx_ring_size;
		if (np->rx_skbuff[entry] == NULL) {
			skb = dev_alloc_skb(np->rx_buf_sz);
			np->rx_skbuff[entry] = skb;
			if (skb == NULL)
				break;				/* Better luck next round. */
			skb->dev = dev;			/* Mark as being used by this device. */
			np->rx_ring[entry].buffer1 = virt_to_bus(skb->tail);
		}
		np->rx_ring[entry].status = DescOwn;
	}

	return 0;
}

static void netdev_error(struct net_device *dev, int intr_status)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = (struct netdev_private *)dev->priv;

	if (np->msg_level & NETIF_MSG_MISC)
		printk(KERN_DEBUG "%s: Abnormal event, %8.8x.\n",
			   dev->name, intr_status);
	if (intr_status == 0xffffffff)
		return;
	if (intr_status & TxFIFOUnderflow) {
		np->csr6 += 0x4000;	/* Bump up the Tx threshold */
		if (np->msg_level & NETIF_MSG_TX_ERR)
			printk(KERN_DEBUG "%s: Tx underflow, increasing threshold to "
				   "%8.8x.\n", dev->name, np->csr6);
		writel(np->csr6, ioaddr + NetworkConfig);
	}
	if (intr_status & IntrRxDied) {		/* Missed a Rx frame. */
		np->stats.rx_errors++;
	}
	if (intr_status & TimerInt) {
		/* Re-enable other interrupts. */
		writel(0x1A0F5, ioaddr + IntrEnable);
	}
	np->stats.rx_missed_errors += readl(ioaddr + RxMissed) & 0xffff;
	writel(0, ioaddr + RxStartDemand);
}

static struct net_device_stats *get_stats(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = (struct netdev_private *)dev->priv;

	/* The chip only need report frame silently dropped. */
	if (netif_running(dev))
		np->stats.rx_missed_errors += readl(ioaddr + RxMissed) & 0xffff;

	return &np->stats;
}

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
	u32 rx_mode;

	if (dev->flags & IFF_PROMISC) {			/* Set promiscuous. */
		/* Unconditionally log net taps. */
		printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n", dev->name);
		memset(mc_filter, ~0, sizeof(mc_filter));
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptAllPhys;
	} else if ((dev->mc_count > np->multicast_filter_limit)
			   ||  (dev->flags & IFF_ALLMULTI)) {
		/* Too many to match, or accept all multicasts. */
		memset(mc_filter, 0xff, sizeof(mc_filter));
		rx_mode = AcceptBroadcast | AcceptMulticast;
	} else {
		struct dev_mc_list *mclist;
		int i;
		memset(mc_filter, 0, sizeof(mc_filter));
		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
			 i++, mclist = mclist->next) {
			set_bit((ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26) ^ 0x3F,
					mc_filter);
		}
		rx_mode = AcceptBroadcast | AcceptMulticast;
	}
	writel(mc_filter[0], ioaddr + MulticastFilter0);
	writel(mc_filter[1], ioaddr + MulticastFilter1);
	np->csr6 &= ~0x00F8;
	np->csr6 |= rx_mode;
	writel(np->csr6, ioaddr + NetworkConfig);
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


static void empty_rings(struct net_device *dev)
{
	struct netdev_private *np = (void *)dev->priv;
	int i;

	/* Free all the skbuffs in the Rx queue. */
	for (i = 0; i < np->rx_ring_size; i++) {
		np->rx_ring[i].status = 0;
		if (np->rx_skbuff[i]) {
#if LINUX_VERSION_CODE < 0x20100
			np->rx_skbuff[i]->free = 1;
#endif
			dev_free_skb(np->rx_skbuff[i]);
		}
		np->rx_skbuff[i] = 0;
	}
	for (i = 0; i < np->tx_ring_size; i++) {
		if (np->tx_skbuff[i])
			dev_free_skb(np->tx_skbuff[i]);
		np->tx_skbuff[i] = 0;
	}
}

static int netdev_close(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = (struct netdev_private *)dev->priv;

	netif_stop_tx_queue(dev);

	if (np->msg_level & NETIF_MSG_IFDOWN) {
		printk(KERN_DEBUG "%s: Shutting down ethercard, status was %8.8x "
			   "Config %8.8x.\n", dev->name, (int)readl(ioaddr + IntrStatus),
			   (int)readl(ioaddr + NetworkConfig));
		printk(KERN_DEBUG "%s: Queue pointers were Tx %d / %d,  Rx %d / %d.\n",
			   dev->name, np->cur_tx, np->dirty_tx, np->cur_rx, np->dirty_rx);
	}

	/* Disable interrupts by clearing the interrupt mask. */
	writel(0x0000, ioaddr + IntrEnable);

	/* Stop the chip's Tx and Rx processes. */
	writel(np->csr6 &= ~0x20FA, ioaddr + NetworkConfig);

	del_timer(&np->timer);
	if (readl(ioaddr + NetworkConfig) != 0xffffffff)
		np->stats.rx_missed_errors += readl(ioaddr + RxMissed) & 0xffff;

#ifdef __i386__
	if (np->msg_level & NETIF_MSG_IFDOWN) {
		int i;
		printk("\n"KERN_DEBUG"  Tx ring at %8.8x:\n",
			   (int)virt_to_bus(np->tx_ring));
		for (i = 0; i < np->tx_ring_size; i++)
			printk(KERN_DEBUG " #%d desc. %4.4x %8.8x %8.8x.\n",
				   i, np->tx_ring[i].length,
				   np->tx_ring[i].status, np->tx_ring[i].buffer1);
		printk(KERN_DEBUG "\n" KERN_DEBUG "  Rx ring %8.8x:\n",
			   (int)virt_to_bus(np->rx_ring));
		for (i = 0; i < np->rx_ring_size; i++) {
			printk(KERN_DEBUG " #%d desc. %4.4x %4.4x %8.8x\n",
				   i, np->rx_ring[i].length,
				   np->rx_ring[i].status, np->rx_ring[i].buffer1);
		}
	}
#endif /* __i386__ debugging only */

	free_irq(dev->irq, dev);
	empty_rings(dev);

	MOD_DEC_USE_COUNT;

	return 0;
}

static int winbond_pwr_event(void *dev_instance, int event)
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
	case DRV_SUSPEND: {
		int csr6 = readl(ioaddr + NetworkConfig);
		/* Disable interrupts, stop the chip, gather stats. */
		if (csr6 != 0xffffffff) {
			int csr8 = readl(ioaddr + RxMissed);
			writel(0x00000000, ioaddr + IntrEnable);
			writel(csr6 & ~TxOn & ~RxOn, ioaddr + NetworkConfig);
			np->stats.rx_missed_errors += (unsigned short)csr8;
		}
		empty_rings(dev);
		break;
	}
	case DRV_RESUME:
		writel(np->csr0, ioaddr + PCIBusCfg);
		init_ring(dev);
		writel(virt_to_bus(np->rx_ring), ioaddr + RxRingPtr);
		writel(virt_to_bus(np->tx_ring), ioaddr + TxRingPtr);
		writel(0x1A0F5, ioaddr + IntrStatus);
		writel(0x1A0F5, ioaddr + IntrEnable);
		writel(np->csr6 | TxOn | RxOn, ioaddr + NetworkConfig);
		writel(0, ioaddr + RxStartDemand);		/* Rx poll demand */
		set_rx_mode(dev);
		break;
	case DRV_DETACH: {
		struct net_device **devp, **next;
		if (dev->flags & IFF_UP) {
			printk(KERN_ERR "%s: Winbond-840 NIC removed while still "
				   "active.\n", dev->name);
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
	default:
		break;
	}

	return 0;
}


#ifdef MODULE
int init_module(void)
{
	if (debug >= NETIF_MSG_DRV)	/* Emit version even if no cards detected. */
		printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	return pci_drv_register(&winbond840_drv_id, NULL);
}

void cleanup_module(void)
{
	struct net_device *next_dev;

	pci_drv_unregister(&winbond840_drv_id);

	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	while (root_net_dev) {
		struct netdev_private *np = (void *)(root_net_dev->priv);
		unregister_netdev(root_net_dev);
#ifdef USE_IO_OPS
		release_region(root_net_dev->base_addr,
					   pci_id_tbl[np->chip_id].io_size);
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
#else
int winbond840_probe(struct net_device *dev)
{
	if (pci_drv_register(&winbond840_drv_id, dev) < 0)
		return -ENODEV;
	printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	return 0;
}
#endif  /* MODULE */


/*
 * Local variables:
 *  compile-command: "make KERNVER=`uname -r` winbond-840.o"
 *  compile-cmd: "gcc -DMODULE -Wall -Wstrict-prototypes -O6 -c winbond-840.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
