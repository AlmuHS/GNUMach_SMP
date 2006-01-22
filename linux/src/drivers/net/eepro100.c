/* drivers/net/eepro100.c: An Intel i82557-559 Ethernet driver for Linux. */
/*
	Written 1998-2003 by Donald Becker.

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This driver is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	This driver is for the Intel EtherExpress Pro100 (Speedo3) design.
	It should work with all i82557/558/559 boards.

	To use as a module, use the compile-command at the end of the file.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	914 Bay Ridge Road, Suite 220
	Annapolis MD 21403

	For updates see
		http://www.scyld.com/network/eepro100.html
	For installation instructions
		http://www.scyld.com/network/modules.html
	The information and support mailing lists are based at
		http://www.scyld.com/mailman/listinfo/
*/

/* These identify the driver base version and may not be removed. */
static const char version1[] =
"eepro100.c:v1.28 7/22/2003 Donald Becker <becker@scyld.com>\n";
static const char version2[] =
"  http://www.scyld.com/network/eepro100.html\n";


/* The user-configurable values.
   These may be modified when a driver module is loaded.
   The first five are undocumented and spelled per Intel recommendations.
*/

/* Message enable level: 0..31 = no..all messages.  See NETIF_MSG docs. */
static int debug = 2;

static int congenb = 0;		/* Enable congestion control in the DP83840. */
static int txfifo = 8;		/* Tx FIFO threshold in 4 byte units, 0-15 */
static int rxfifo = 8;		/* Rx FIFO threshold, default 32 bytes. */
/* Tx/Rx DMA burst length, 0-127, 0 == no preemption, tx==128 -> disabled. */
static int txdmacount = 128;
static int rxdmacount = 0;

/* Set the copy breakpoint for the copy-only-tiny-frame Rx method.
   Lower values use more memory, but are faster.
   Setting to > 1518 disables this feature. */
static int rx_copybreak = 200;

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;

/* Maximum number of multicast addresses to filter (vs. rx-all-multicast) */
static int multicast_filter_limit = 64;

/* Used to pass the media type, etc.
   Both 'options[]' and 'full_duplex[]' should exist for driver
   interoperability, however setting full_duplex[] is deprecated.
   The media type is usually passed in 'options[]'.
    Use option values 0x10/0x20 for 10Mbps, 0x100,0x200 for 100Mbps.
    Use option values 0x10 and 0x100 for forcing half duplex fixed speed.
    Use option values 0x20 and 0x200 for forcing full duplex operation.
*/
#define MAX_UNITS 8		/* More are supported, limit only on options */
static int options[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int full_duplex[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};

/* Operational parameters that are set at compile time. */

/* The ring sizes should be a power of two for efficiency. */
#define TX_RING_SIZE	32		/* Effectively 2 entries fewer. */
#define RX_RING_SIZE	32
/* Actual number of TX packets queued, must be <= TX_RING_SIZE-2. */
#define TX_QUEUE_LIMIT  12
#define TX_QUEUE_UNFULL 8		/* Hysteresis marking queue as no longer full. */

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
#include <linux/delay.h>
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

/* Condensed bus+endian portability operations. */
#define virt_to_le32desc(addr)  cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr)  bus_to_virt(le32_to_cpu(addr))

#if (LINUX_VERSION_CODE >= 0x20100)  &&  defined(MODULE)
char kernel_version[] = UTS_RELEASE;
#endif

MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("Intel PCI EtherExpressPro 100 driver");
MODULE_LICENSE("GPL");
MODULE_PARM(debug, "i");
MODULE_PARM(options, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(full_duplex, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(congenb, "i");
MODULE_PARM(txfifo, "i");
MODULE_PARM(rxfifo, "i");
MODULE_PARM(txdmacount, "i");
MODULE_PARM(rxdmacount, "i");
MODULE_PARM(rx_copybreak, "i");
MODULE_PARM(max_interrupt_work, "i");
MODULE_PARM(multicast_filter_limit, "i");
#ifdef MODULE_PARM_DESC
MODULE_PARM_DESC(debug, "EEPro100 message level (0-31)");
MODULE_PARM_DESC(options,
				 "EEPro100: force fixed speed+duplex 0x10 0x20 0x100 0x200");
MODULE_PARM_DESC(max_interrupt_work,
				 "EEPro100 maximum events handled per interrupt");
MODULE_PARM_DESC(full_duplex, "EEPro100 set to forced full duplex when not 0"
				 " (deprecated)");
MODULE_PARM_DESC(rx_copybreak,
				 "EEPro100 copy breakpoint for copy-only-tiny-frames");
MODULE_PARM_DESC(multicast_filter_limit,
				 "EEPro100 breakpoint for switching to Rx-all-multicast");
/* Other settings are undocumented per Intel recommendation. */
#endif

/*
				Theory of Operation

I. Board Compatibility

This device driver is designed for the Intel i82557 "Speedo3" chip, Intel's
single-chip fast Ethernet controller for PCI, as used on the Intel
EtherExpress Pro 100 adapter.

II. Board-specific settings

PCI bus devices are configured by the system at boot time, so no jumpers
need to be set on the board.  The system BIOS should be set to assign the
PCI INTA signal to an otherwise unused system IRQ line.  While it's
possible to share PCI interrupt lines, it negatively impacts performance and
only recent kernels support it.

III. Driver operation

IIIA. General
The Speedo3 is very similar to other Intel network chips, that is to say
"apparently designed on a different planet".  This chips retains the complex
Rx and Tx descriptors and multiple buffers pointers as previous chips, but
also has simplified Tx and Rx buffer modes.  This driver uses the "flexible"
Tx mode, but in a simplified lower-overhead manner: it associates only a
single buffer descriptor with each frame descriptor.

Despite the extra space overhead in each receive skbuff, the driver must use
the simplified Rx buffer mode to assure that only a single data buffer is
associated with each RxFD. The driver implements this by reserving space
for the Rx descriptor at the head of each Rx skbuff.

The Speedo-3 has receive and command unit base addresses that are added to
almost all descriptor pointers.  The driver sets these to zero, so that all
pointer fields are absolute addresses.

The System Control Block (SCB) of some previous Intel chips exists on the
chip in both PCI I/O and memory space.  This driver uses the I/O space
registers, but might switch to memory mapped mode to better support non-x86
processors.

IIIB. Transmit structure

The driver must use the complex Tx command+descriptor mode in order to
have a indirect pointer to the skbuff data section.  Each Tx command block
(TxCB) is associated with two immediately appended Tx Buffer Descriptor
(TxBD).  A fixed ring of these TxCB+TxBD pairs are kept as part of the
speedo_private data structure for each adapter instance.

The i82558 and later explicitly supports this structure, and can read the two
TxBDs in the same PCI burst as the TxCB.

This ring structure is used for all normal transmit packets, but the
transmit packet descriptors aren't long enough for most non-Tx commands such
as CmdConfigure.  This is complicated by the possibility that the chip has
already loaded the link address in the previous descriptor.  So for these
commands we convert the next free descriptor on the ring to a NoOp, and point
that descriptor's link to the complex command.

An additional complexity of these non-transmit commands are that they may be
added asynchronous to the normal transmit queue, so we set a lock
whenever the Tx descriptor ring is manipulated.

A notable aspect of these special configure commands is that they do
work with the normal Tx ring entry scavenge method.  The Tx ring scavenge
is done at interrupt time using the 'dirty_tx' index, and checking for the
command-complete bit.  While the setup frames may have the NoOp command on the
Tx ring marked as complete, but not have completed the setup command, this
is not a problem.  The tx_ring entry can be still safely reused, as the
tx_skbuff[] entry is always empty for config_cmd and mc_setup frames.

Commands may have bits set e.g. CmdSuspend in the command word to either
suspend or stop the transmit/command unit.  This driver always initializes
the current command with CmdSuspend before erasing the CmdSuspend in the
previous command, and only then issues a CU_RESUME.

Note: In previous generation Intel chips, restarting the command unit was a
notoriously slow process.  This is presumably no longer true.

IIIC. Receive structure

Because of the bus-master support on the Speedo3 this driver uses the
SKBUFF_RX_COPYBREAK scheme, rather than a fixed intermediate receive buffer.
This scheme allocates full-sized skbuffs as receive buffers.  The value
SKBUFF_RX_COPYBREAK is used as the copying breakpoint: it is chosen to
trade-off the memory wasted by passing the full-sized skbuff to the queue
layer for all frames vs. the copying cost of copying a frame to a
correctly-sized skbuff.

For small frames the copying cost is negligible (esp. considering that we
are pre-loading the cache with immediately useful header information), so we
allocate a new, minimally-sized skbuff.  For large frames the copying cost
is non-trivial, and the larger copy might flush the cache of useful data, so
we pass up the skbuff the packet was received into.

IIID. Synchronization
The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->tbusy flag.  The other thread is the interrupt handler, which is single
threaded by the hardware and other software.

The send packet thread has partial control over the Tx ring and 'dev->tbusy'
flag.  It sets the tbusy flag whenever it's queuing a Tx packet. If the next
queue slot is empty, it clears the tbusy flag when finished otherwise it sets
the 'sp->tx_full' flag.

The interrupt handler has exclusive control over the Rx ring and records stats
from the Tx ring.  (The Tx-done interrupt can't be selectively turned off, so
we can't avoid the interrupt overhead by having the Tx routine reap the Tx
stats.)	 After reaping the stats, it marks the queue entry as empty by setting
the 'base' to zero.	 Iff the 'sp->tx_full' flag is set, it clears both the
tx_full and tbusy flags.

IV. Notes

Thanks to Steve Williams of Intel for arranging the non-disclosure agreement
that stated that I could disclose the information.  But I still resent
having to sign an Intel NDA when I'm helping Intel sell their own product!

*/

/* This table drives the PCI probe routines. */
static void *speedo_found1(struct pci_dev *pdev, void *init_dev,
						   long ioaddr, int irq, int chip_idx, int fnd_cnt);
static int speedo_pwr_event(void *dev_instance, int event);
enum chip_capability_flags { ResetMII=1, HasChksum=2};

/* I/O registers beyond 0x18 do not exist on the i82557. */
#ifdef USE_IO_OPS
#define SPEEDO_IOTYPE   PCI_USES_MASTER|PCI_USES_IO|PCI_ADDR1
#define SPEEDO_SIZE		32
#else
#define SPEEDO_IOTYPE   PCI_USES_MASTER|PCI_USES_MEM|PCI_ADDR0
#define SPEEDO_SIZE		0x1000
#endif

struct pci_id_info static pci_id_tbl[] = {
	{"Intel PCI EtherExpress Pro100 82865",		{ 0x12278086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel PCI EtherExpress Pro100 Smart (i960RP/RD)",
	 { 0x12288086, 0xffffffff,}, SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel i82559 rev 8",			{ 0x12298086, ~0, 0,0, 8,0xff},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, HasChksum, },
	{"Intel PCI EtherExpress Pro100",			{ 0x12298086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel EtherExpress Pro/100+ i82559ER",	{ 0x12098086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, ResetMII, },
	{"Intel EtherExpress Pro/100 type 1029",	{ 0x10298086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel EtherExpress Pro/100 type 1030",	{ 0x10308086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 V Network",					{ 0x24498086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel PCI LAN0 Controller 82801E",		{ 0x24598086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel PCI LAN1 Controller 82801E",		{ 0x245D8086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VE (type 1031)",			{ 0x10318086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VE (type 1032)",			{ 0x10328086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VE (type 1033)",			{ 0x10338086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VE (type 1034)",			{ 0x10348086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VE (type 1035)",			{ 0x10358086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VM (type 1038)",			{ 0x10388086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VM (type 1039)",			{ 0x10398086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VM (type 103a)",			{ 0x103a8086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"HP/Compaq D510 Intel Pro/100 VM",
	 { 0x103b8086, 0xffffffff, 0x00120e11, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VM (type 103b)",			{ 0x103b8086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VE (type 103D)",			{ 0x103d8086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VE (type 103E)",			{ 0x103e8086, 0xffffffff,},
	 SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel EtherExpress Pro/100 865G Northbridge type 1051",
	 { 0x10518086, 0xffffffff,}, SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel PCI to PCI Bridge EtherExpress Pro100 Server Adapter",
	 { 0x52008086, 0xffffffff,}, SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel PCI EtherExpress Pro100 Server Adapter",
	 { 0x52018086, 0xffffffff,}, SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 VM (unknown type series 1030)",
	 { 0x10308086, 0xfff0ffff,}, SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{"Intel Pro/100 (unknown type series 1050)",
	 { 0x10508086, 0xfff0ffff,}, SPEEDO_IOTYPE, SPEEDO_SIZE, 0, },
	{0,},						/* 0 terminated list. */
};

struct drv_id_info eepro100_drv_id = {
	"eepro100", PCI_HOTSWAP, PCI_CLASS_NETWORK_ETHERNET<<8, pci_id_tbl,
	speedo_found1, speedo_pwr_event, };

#ifndef USE_IO_OPS
#undef inb
#undef inw
#undef inl
#undef outb
#undef outw
#undef outl
#define inb readb
#define inw readw
#define inl readl
#define outb writeb
#define outw writew
#define outl writel
#endif

/* Offsets to the various registers.
   All accesses need not be longword aligned. */
enum speedo_offsets {
	SCBStatus = 0, SCBCmd = 2,	/* Rx/Command Unit command and status. */
	SCBPointer = 4,				/* General purpose pointer. */
	SCBPort = 8,				/* Misc. commands and operands.  */
	SCBflash = 12, SCBeeprom = 14, /* EEPROM and flash memory control. */
	SCBCtrlMDI = 16,			/* MDI interface control. */
	SCBEarlyRx = 20,			/* Early receive byte count. */
};
/* Commands that can be put in a command list entry. */
enum commands {
	CmdNOp = 0, CmdIASetup = 0x10000, CmdConfigure = 0x20000,
	CmdMulticastList = 0x30000, CmdTx = 0x40000, CmdTDR = 0x50000,
	CmdDump = 0x60000, CmdDiagnose = 0x70000,
	CmdSuspend = 0x40000000,	/* Suspend after completion. */
	CmdIntr = 0x20000000,		/* Interrupt after completion. */
	CmdTxFlex = 0x00080000,		/* Use "Flexible mode" for CmdTx command. */
};
/* Do atomically if possible. */
#if defined(__i386__)
#define clear_suspend(cmd)   ((char *)(&(cmd)->cmd_status))[3] &= ~0x40
#elif defined(__alpha__) || defined(__x86_64) || defined(__ia64)
#define clear_suspend(cmd)   clear_bit(30, &(cmd)->cmd_status)
#elif defined(__powerpc__) || defined(__sparc__) || (__BIG_ENDIAN)
#define clear_suspend(cmd)	clear_bit(6, &(cmd)->cmd_status)
#else
#warning Undefined architecture.
#define clear_suspend(cmd)	(cmd)->cmd_status &= cpu_to_le32(~CmdSuspend)
#endif

enum SCBCmdBits {
	SCBMaskCmdDone=0x8000, SCBMaskRxDone=0x4000, SCBMaskCmdIdle=0x2000,
	SCBMaskRxSuspend=0x1000, SCBMaskEarlyRx=0x0800, SCBMaskFlowCtl=0x0400,
	SCBTriggerIntr=0x0200, SCBMaskAll=0x0100,
	/* The rest are Rx and Tx commands. */
	CUStart=0x0010, CUResume=0x0020, CUHiPriStart=0x0030, CUStatsAddr=0x0040,
	CUShowStats=0x0050,
	CUCmdBase=0x0060,  /* CU Base address (set to zero) . */
	CUDumpStats=0x0070, /* Dump then reset stats counters. */
	CUHiPriResume=0x00b0, /* Resume for the high priority Tx queue. */
	RxStart=0x0001, RxResume=0x0002, RxAbort=0x0004, RxAddrLoad=0x0006,
	RxResumeNoResources=0x0007,
};

enum intr_status_bits {
	IntrCmdDone=0x8000,  IntrRxDone=0x4000, IntrCmdIdle=0x2000,
	IntrRxSuspend=0x1000, IntrMIIDone=0x0800, IntrDrvrIntr=0x0400,
	IntrAllNormal=0xfc00,
};

enum SCBPort_cmds {
	PortReset=0, PortSelfTest=1, PortPartialReset=2, PortDump=3,
};

/* The Speedo3 Rx and Tx frame/buffer descriptors. */
struct descriptor {			/* A generic descriptor. */
	s32 cmd_status;			/* All command and status fields. */
	u32 link;					/* struct descriptor *  */
	unsigned char params[0];
};

/* The Speedo3 Rx and Tx buffer descriptors. */
struct RxFD {					/* Receive frame descriptor. */
	s32 status;
	u32 link;					/* struct RxFD * */
	u32 rx_buf_addr;			/* void * */
	u32 count;
};

/* Selected elements of the Tx/RxFD.status word. */
enum RxFD_bits {
	RxComplete=0x8000, RxOK=0x2000,
	RxErrCRC=0x0800, RxErrAlign=0x0400, RxErrTooBig=0x0200, RxErrSymbol=0x0010,
	RxEth2Type=0x0020, RxNoMatch=0x0004, RxNoIAMatch=0x0002,
	TxUnderrun=0x1000,  StatusComplete=0x8000,
};

struct TxFD {					/* Transmit frame descriptor set. */
	s32 status;
	u32 link;					/* void * */
	u32 tx_desc_addr;			/* Always points to the tx_buf_addr element. */
	s32 count;					/* # of TBD (=1), Tx start thresh., etc. */
	/* This constitutes two "TBD" entries. Non-zero-copy uses only one. */
	u32 tx_buf_addr0;			/* void *, frame to be transmitted.  */
	s32 tx_buf_size0;			/* Length of Tx frame. */
	u32 tx_buf_addr1;			/* Used only for zero-copy data section. */
	s32 tx_buf_size1;			/* Length of second data buffer (0). */
};

/* Elements of the dump_statistics block. This block must be lword aligned. */
struct speedo_stats {
	u32 tx_good_frames;
	u32 tx_coll16_errs;
	u32 tx_late_colls;
	u32 tx_underruns;
	u32 tx_lost_carrier;
	u32 tx_deferred;
	u32 tx_one_colls;
	u32 tx_multi_colls;
	u32 tx_total_colls;
	u32 rx_good_frames;
	u32 rx_crc_errs;
	u32 rx_align_errs;
	u32 rx_resource_errs;
	u32 rx_overrun_errs;
	u32 rx_colls_errs;
	u32 rx_runt_errs;
	u32 done_marker;
};

/* Do not change the position (alignment) of the first few elements!
   The later elements are grouped for cache locality. */
struct speedo_private {
	struct TxFD	tx_ring[TX_RING_SIZE];	/* Commands (usually CmdTxPacket). */
	struct RxFD *rx_ringp[RX_RING_SIZE];	/* Rx descriptor, used as ring. */
	struct speedo_stats lstats;			/* Statistics and self-test region */

	/* The addresses of a Tx/Rx-in-place packets/buffers. */
	struct sk_buff* tx_skbuff[TX_RING_SIZE];
	struct sk_buff* rx_skbuff[RX_RING_SIZE];

	/* Transmit and other commands control. */
	struct descriptor  *last_cmd;	/* Last command sent. */
	unsigned int cur_tx, dirty_tx;	/* The ring entries to be free()ed. */
	spinlock_t lock;				/* Group with Tx control cache line. */
	u32 tx_threshold;					/* The value for txdesc.count. */
	unsigned long last_cmd_time;

	/* Rx control, one cache line. */
	struct RxFD *last_rxf;				/* Most recent Rx frame. */
	unsigned int cur_rx, dirty_rx;		/* The next free ring entry */
	unsigned int rx_buf_sz;				/* Based on MTU+slack. */
	long last_rx_time;			/* Last Rx, in jiffies, to handle Rx hang. */
	int rx_copybreak;

	int msg_level;
	int max_interrupt_work;
	struct net_device *next_module;
	void *priv_addr;					/* Unaligned address for kfree */
	struct net_device_stats stats;
	int alloc_failures;
	int chip_id, drv_flags;
	struct pci_dev *pci_dev;
	unsigned char acpi_pwr;
	struct timer_list timer;	/* Media selection timer. */
	/* Multicast filter command. */
	int mc_setup_frm_len;			 	/* The length of an allocated.. */
	struct descriptor *mc_setup_frm; 	/* ..multicast setup frame. */
	int mc_setup_busy;					/* Avoid double-use of setup frame. */
	int multicast_filter_limit;

	int in_interrupt;					/* Word-aligned dev->interrupt */
	int rx_mode;						/* Current PROMISC/ALLMULTI setting. */
	unsigned int tx_full:1;				/* The Tx queue is full. */
	unsigned int full_duplex:1;			/* Full-duplex operation requested. */
	unsigned int flow_ctrl:1;			/* Use 802.3x flow control. */
	unsigned int rx_bug:1;				/* Work around receiver hang errata. */
	unsigned int rx_bug10:1;			/* Receiver might hang at 10mbps. */
	unsigned int rx_bug100:1;			/* Receiver might hang at 100mbps. */
	unsigned int polling:1;				/* Hardware blocked interrupt line. */
	unsigned int medialock:1;			/* The media speed/duplex is fixed. */
	unsigned char default_port;			/* Last dev->if_port value. */
	unsigned short phy[2];				/* PHY media interfaces available. */
	unsigned short advertising;			/* Current PHY advertised caps. */
	unsigned short partner;				/* Link partner caps. */
	long last_reset;
};

/* Our internal RxMode state, not tied to the hardware bits. */
enum rx_mode_bits {
	AcceptAllMulticast=0x01, AcceptAllPhys=0x02, 
	AcceptErr=0x80, AcceptRunt=0x10,
	AcceptBroadcast=0x08, AcceptMulticast=0x04,
	AcceptMyPhys=0x01, RxInvalidMode=0x7f
};

/* The parameters for a CmdConfigure operation.
   There are so many options that it would be difficult to document each bit.
   We mostly use the default or recommended settings. */
const char i82557_config_cmd[22] = {
	22, 0x08, 0, 0,  0, 0, 0x32, 0x03,  1, /* 1=Use MII  0=Use AUI */
	0, 0x2E, 0,  0x60, 0,
	0xf2, 0x48,   0, 0x40, 0xf2, 0x80, 		/* 0x40=Force full-duplex */
	0x3f, 0x05, };
const char i82558_config_cmd[22] = {
	22, 0x08, 0, 1,  0, 0, 0x22, 0x03,  1, /* 1=Use MII  0=Use AUI */
	0, 0x2E, 0,  0x60, 0x08, 0x88,
	0x68, 0, 0x40, 0xf2, 0xBD, 		/* 0xBD->0xFD=Force full-duplex */
	0x31, 0x05, };

/* PHY media interface chips, defined by the databook. */
static const char *phys[] = {
	"None", "i82553-A/B", "i82553-C", "i82503",
	"DP83840", "80c240", "80c24", "i82555",
	"unknown-8", "unknown-9", "DP83840A", "unknown-11",
	"unknown-12", "unknown-13", "unknown-14", "unknown-15", };
enum phy_chips { NonSuchPhy=0, I82553AB, I82553C, I82503, DP83840, S80C240,
					 S80C24, I82555, DP83840A=10, };
static const char is_mii[] = { 0, 1, 1, 0, 1, 1, 0, 1 };

/* Standard serial configuration EEPROM commands. */
#define EE_READ_CMD		(6)

static int do_eeprom_cmd(long ioaddr, int cmd, int cmd_len);
static int mdio_read(struct net_device *dev, int phy_id, int location);
static int mdio_write(long ioaddr, int phy_id, int location, int value);
static int speedo_open(struct net_device *dev);
static void speedo_resume(struct net_device *dev);
static void speedo_timer(unsigned long data);
static void speedo_init_rx_ring(struct net_device *dev);
static void speedo_tx_timeout(struct net_device *dev);
static int speedo_start_xmit(struct sk_buff *skb, struct net_device *dev);
static int speedo_rx(struct net_device *dev);
static void speedo_interrupt(int irq, void *dev_instance, struct pt_regs *regs);
static int speedo_close(struct net_device *dev);
static struct net_device_stats *speedo_get_stats(struct net_device *dev);
static int speedo_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static void set_rx_mode(struct net_device *dev);



#ifdef honor_default_port
/* Optional driver feature to allow forcing the transceiver setting.
   Not recommended. */
static int mii_ctrl[8] = { 0x3300, 0x3100, 0x0000, 0x0100,
						   0x2000, 0x2100, 0x0400, 0x3100};
#endif

/* A list of all installed Speedo devices, for removing the driver module. */
static struct net_device *root_speedo_dev = NULL;

static void *speedo_found1(struct pci_dev *pdev, void *init_dev,
						   long ioaddr, int irq, int chip_idx, int card_idx)
{
	struct net_device *dev;
	struct speedo_private *sp;
	void *priv_mem;
	int i, option;
	u16 eeprom[0x100];
	int acpi_idle_state = 0;

	dev = init_etherdev(init_dev, 0);
	if (!dev)
		return NULL;

	if (dev->mem_start > 0)
		option = dev->mem_start;
	else if (card_idx >= 0  &&  options[card_idx] >= 0)
		option = options[card_idx];
	else
		option = -1;

	acpi_idle_state = acpi_set_pwr_state(pdev, ACPI_D0);

	/* Read the station address EEPROM before doing the reset.
	   Nominally his should even be done before accepting the device, but
	   then we wouldn't have a device name with which to report the error.
	   The size test is for 6 bit vs. 8 bit address serial EEPROMs.
	*/
	{
		u16 sum = 0;
		int j;
		int read_cmd, ee_size;

		if ((do_eeprom_cmd(ioaddr, EE_READ_CMD << 24, 27) & 0xffe0000)
			== 0xffe0000) {
			ee_size = 0x100;
			read_cmd = EE_READ_CMD << 24;
		} else {
			ee_size = 0x40;
			read_cmd = EE_READ_CMD << 22;
		}

		for (j = 0, i = 0; i < ee_size; i++) {
			u16 value = do_eeprom_cmd(ioaddr, read_cmd | (i << 16), 27);
			eeprom[i] = value;
			sum += value;
			if (i < 3) {
				dev->dev_addr[j++] = value;
				dev->dev_addr[j++] = value >> 8;
			}
		}
		if (sum != 0xBABA)
			printk(KERN_WARNING "%s: Invalid EEPROM checksum %#4.4x, "
				   "check settings before activating this device!\n",
				   dev->name, sum);
		/* Don't  unregister_netdev(dev);  as the EEPro may actually be
		   usable, especially if the MAC address is set later. */
	}

	/* Reset the chip: stop Tx and Rx processes and clear counters.
	   This takes less than 10usec and will easily finish before the next
	   action. */
	outl(PortReset, ioaddr + SCBPort);

	printk(KERN_INFO "%s: %s%s at %#3lx, ", dev->name,
		   eeprom[3] & 0x0100 ? "OEM " : "", pci_id_tbl[chip_idx].name,
		   ioaddr);

	for (i = 0; i < 5; i++)
		printk("%2.2X:", dev->dev_addr[i]);
	printk("%2.2X, IRQ %d.\n", dev->dev_addr[i], irq);

	/* We have decided to accept this device. */
	/* Allocate cached private storage.
	   The PCI coherent descriptor rings are allocated at each open. */
	sp = priv_mem = kmalloc(sizeof(*sp), GFP_KERNEL);
	/* Check for the very unlikely case of no memory. */
	if (priv_mem == NULL)
		return NULL;
	dev->base_addr = ioaddr;
	dev->irq = irq;

#ifndef kernel_bloat
	/* OK, this is pure kernel bloat.  I don't like it when other drivers
	   waste non-pageable kernel space to emit similar messages, but I need
	   them for bug reports. */
	{
		const char *connectors[] = {" RJ45", " BNC", " AUI", " MII"};
		/* The self-test results must be paragraph aligned. */
		s32 *volatile self_test_results;
		int boguscnt = 16000;	/* Timeout for set-test. */
		printk(KERN_INFO "  Board assembly %4.4x%2.2x-%3.3d, Physical"
			   " connectors present:",
			   eeprom[8], eeprom[9]>>8, eeprom[9] & 0xff);
		for (i = 0; i < 4; i++)
			if (eeprom[5] & (1<<i))
				printk(connectors[i]);
		printk("\n"KERN_INFO"  Primary interface chip %s PHY #%d.\n",
			   phys[(eeprom[6]>>8)&15], eeprom[6] & 0x1f);
		if (eeprom[7] & 0x0700)
			printk(KERN_INFO "    Secondary interface chip %s.\n",
				   phys[(eeprom[7]>>8)&7]);
		if (((eeprom[6]>>8) & 0x3f) == DP83840
			||  ((eeprom[6]>>8) & 0x3f) == DP83840A) {
			int mdi_reg23 = mdio_read(dev, eeprom[6] & 0x1f, 23) | 0x0422;
			if (congenb)
			  mdi_reg23 |= 0x0100;
			printk(KERN_INFO"  DP83840 specific setup, setting register 23 to %4.4x.\n",
				   mdi_reg23);
			mdio_write(ioaddr, eeprom[6] & 0x1f, 23, mdi_reg23);
		}
		if ((option >= 0) && (option & 0x330)) {
			printk(KERN_INFO "  Forcing %dMbs %s-duplex operation.\n",
				   (option & 0x300 ? 100 : 10),
				   (option & 0x220 ? "full" : "half"));
			mdio_write(ioaddr, eeprom[6] & 0x1f, 0,
					   ((option & 0x300) ? 0x2000 : 0) | 	/* 100mbps? */
					   ((option & 0x220) ? 0x0100 : 0)); /* Full duplex? */
		} else {
			int mii_bmcrctrl = mdio_read(dev, eeprom[6] & 0x1f, 0);
			/* Reset out of a transceiver left in 10baseT-fixed mode. */
			if ((mii_bmcrctrl & 0x3100) == 0)
				mdio_write(ioaddr, eeprom[6] & 0x1f, 0, 0x8000);
		}
		if (eeprom[10] & 0x0002)
			printk(KERN_INFO "\n" KERN_INFO "  ** The configuration "
				   "EEPROM enables Sleep Mode.\n" KERN_INFO "\n"
				   "  ** This will cause PCI bus errors!\n"
				   KERN_INFO "  ** Update the configuration EEPROM "
				   "with the eepro100-diag program.\n"  );
		if (eeprom[6] == 0)
			printk(KERN_INFO "  ** The configuration EEPROM does not have a "
				   "transceiver type set.\n" KERN_INFO "\n"
				   "  ** This will cause configuration problems and prevent "
				   "monitoring the link!\n"
				   KERN_INFO "  ** Update the configuration EEPROM "
				   "with the eepro100-diag program.\n"  );

		/* Perform a system self-test. */
		self_test_results = (s32*)(&sp->lstats);
		self_test_results[0] = 0;
		self_test_results[1] = -1;
		outl(virt_to_bus(self_test_results) | PortSelfTest, ioaddr + SCBPort);
		do {
			udelay(10);
		} while (self_test_results[1] == -1  &&  --boguscnt >= 0);

		if (boguscnt < 0) {		/* Test optimized out. */
			printk(KERN_ERR "Self test failed, status %8.8x:\n"
				   KERN_ERR " Failure to initialize the i82557.\n"
				   KERN_ERR " Verify that the card is a bus-master"
				   " capable slot.\n",
				   self_test_results[1]);
		} else
			printk(KERN_INFO "  General self-test: %s.\n"
				   KERN_INFO "  Serial sub-system self-test: %s.\n"
				   KERN_INFO "  Internal registers self-test: %s.\n"
				   KERN_INFO "  ROM checksum self-test: %s (%#8.8x).\n",
				   self_test_results[1] & 0x1000 ? "failed" : "passed",
				   self_test_results[1] & 0x0020 ? "failed" : "passed",
				   self_test_results[1] & 0x0008 ? "failed" : "passed",
				   self_test_results[1] & 0x0004 ? "failed" : "passed",
				   self_test_results[0]);
	}
#endif  /* kernel_bloat */

	outl(PortReset, ioaddr + SCBPort);

	/* Return the chip to its original power state. */
	acpi_set_pwr_state(pdev, acpi_idle_state);

	/* We do a request_region() only to register /proc/ioports info. */
	request_region(ioaddr, pci_id_tbl[chip_idx].io_size, dev->name);

	dev->priv = sp;				/* Allocated above. */
	memset(sp, 0, sizeof(*sp));
	sp->next_module = root_speedo_dev;
	root_speedo_dev = dev;

	sp->priv_addr = priv_mem;
	sp->pci_dev = pdev;
	sp->chip_id = chip_idx;
	sp->drv_flags = pci_id_tbl[chip_idx].drv_flags;
	sp->acpi_pwr = acpi_idle_state;
	sp->msg_level = (1 << debug) - 1;
	sp->rx_copybreak = rx_copybreak;
	sp->max_interrupt_work = max_interrupt_work;
	sp->multicast_filter_limit = multicast_filter_limit;

	sp->full_duplex = option >= 0 && (option & 0x220) ? 1 : 0;
	if (card_idx >= 0) {
		if (full_duplex[card_idx] >= 0)
			sp->full_duplex = full_duplex[card_idx];
	}
	sp->default_port = option >= 0 ? (option & 0x0f) : 0;
	if (sp->full_duplex)
		sp->medialock = 1;

	sp->phy[0] = eeprom[6];
	sp->phy[1] = eeprom[7];
	sp->rx_bug = (eeprom[3] & 0x03) == 3 ? 0 : 1;

	if (sp->rx_bug)
		printk(KERN_INFO "  Receiver lock-up workaround activated.\n");

	/* The Speedo-specific entries in the device structure. */
	dev->open = &speedo_open;
	dev->hard_start_xmit = &speedo_start_xmit;
	dev->stop = &speedo_close;
	dev->get_stats = &speedo_get_stats;
	dev->set_multicast_list = &set_rx_mode;
	dev->do_ioctl = &speedo_ioctl;

	return dev;
}

/* How to wait for the command unit to accept a command.
   Typically this takes 0 ticks. */

static inline void wait_for_cmd_done(struct net_device *dev)
{
	long cmd_ioaddr = dev->base_addr + SCBCmd;
	int wait = 0;
	int delayed_cmd;
	do
		if (inb(cmd_ioaddr) == 0) return;
	while(++wait <= 100);
	delayed_cmd = inb(cmd_ioaddr);
	do
		if (inb(cmd_ioaddr) == 0) break;
	while(++wait <= 10000);
	printk(KERN_ERR "%s: Command %2.2x was not immediately accepted, "
		   "%d ticks!\n",
		   dev->name, delayed_cmd, wait);
}

/* Perform a SCB command known to be slow.
   This function checks the status both before and after command execution. */
static void do_slow_command(struct net_device *dev, int cmd)
{
	long cmd_ioaddr = dev->base_addr + SCBCmd;
	int wait = 0;
	do
		if (inb(cmd_ioaddr) == 0) break;
	while(++wait <= 200);
	if (wait > 100)
		printk(KERN_ERR "%s: Command %4.4x was never accepted (%d polls)!\n",
			   dev->name, inb(cmd_ioaddr), wait);
	outb(cmd, cmd_ioaddr);
	for (wait = 0; wait <= 100; wait++)
		if (inb(cmd_ioaddr) == 0) return;
	for (; wait <= 20000; wait++)
		if (inb(cmd_ioaddr) == 0) return;
		else udelay(1);
	printk(KERN_ERR "%s: Command %4.4x was not accepted after %d polls!"
		   "  Current status %8.8x.\n",
		   dev->name, cmd, wait, (int)inl(dev->base_addr + SCBStatus));
}


/* Serial EEPROM section.
   A "bit" grungy, but we work our way through bit-by-bit :->. */
/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x01	/* EEPROM shift clock. */
#define EE_CS			0x02	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x04	/* EEPROM chip data in. */
#define EE_DATA_READ	0x08	/* EEPROM chip data out. */
#define EE_ENB			(0x4800 | EE_CS)
#define EE_WRITE_0		0x4802
#define EE_WRITE_1		0x4806
#define EE_OFFSET		SCBeeprom

/* Delay between EEPROM clock transitions.
   The code works with no delay on 33Mhz PCI.  */
#ifndef USE_IO_OPS
#define eeprom_delay(ee_addr)	writew(readw(ee_addr), ee_addr)
#else
#define eeprom_delay(ee_addr)	inw(ee_addr)
#endif

static int do_eeprom_cmd(long ioaddr, int cmd, int cmd_len)
{
	unsigned retval = 0;
	long ee_addr = ioaddr + SCBeeprom;

	outw(EE_ENB | EE_SHIFT_CLK, ee_addr);

	/* Shift the command bits out. */
	do {
		short dataval = (cmd & (1 << cmd_len)) ? EE_WRITE_1 : EE_WRITE_0;
		outw(dataval, ee_addr);
		eeprom_delay(ee_addr);
		outw(dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay(ee_addr);
		retval = (retval << 1) | ((inw(ee_addr) & EE_DATA_READ) ? 1 : 0);
	} while (--cmd_len >= 0);
	outw(EE_ENB, ee_addr);

	/* Terminate the EEPROM access. */
	outw(EE_ENB & ~EE_CS, ee_addr);
	return retval;
}

static int mdio_read(struct net_device *dev, int phy_id, int location)
{
	long ioaddr = dev->base_addr;
	int val, boguscnt = 64*10;		/* <64 usec. to complete, typ 27 ticks */

	outl(0x08000000 | (location<<16) | (phy_id<<21), ioaddr + SCBCtrlMDI);
	do {
		val = inl(ioaddr + SCBCtrlMDI);
		if (--boguscnt < 0) {
			printk(KERN_ERR "%s: mdio_read() timed out with val = %8.8x.\n",
				   dev->name, val);
			break;
		}
	} while (! (val & 0x10000000));
	return val & 0xffff;
}

static int mdio_write(long ioaddr, int phy_id, int location, int value)
{
	int val, boguscnt = 64*10;		/* <64 usec. to complete, typ 27 ticks */
	outl(0x04000000 | (location<<16) | (phy_id<<21) | value,
		 ioaddr + SCBCtrlMDI);
	do {
		val = inl(ioaddr + SCBCtrlMDI);
		if (--boguscnt < 0) {
			printk(KERN_ERR" mdio_write() timed out with val = %8.8x.\n", val);
			break;
		}
	} while (! (val & 0x10000000));
	return val & 0xffff;
}


static int
speedo_open(struct net_device *dev)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	long ioaddr = dev->base_addr;

	MOD_INC_USE_COUNT;
	acpi_set_pwr_state(sp->pci_dev, ACPI_D0);

	if (sp->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: speedo_open() irq %d.\n", dev->name, dev->irq);

	/* Set up the Tx queue early.. */
	sp->cur_tx = 0;
	sp->dirty_tx = 0;
	sp->last_cmd = 0;
	sp->tx_full = 0;
	sp->lock = (spinlock_t) SPIN_LOCK_UNLOCKED;
	sp->polling = sp->in_interrupt = 0;

	dev->if_port = sp->default_port;

	if ((sp->phy[0] & 0x8000) == 0)
		sp->advertising = mdio_read(dev, sp->phy[0] & 0x1f, 4);
	/* With some transceivers we must retrigger negotiation to reset
	   power-up errors. */
	if ((sp->drv_flags & ResetMII) &&
		(sp->phy[0] & 0x8000) == 0) {
		int phy_addr = sp->phy[0] & 0x1f ;
		/* Use 0x3300 for restarting NWay, other values to force xcvr:
		   0x0000 10-HD
		   0x0100 10-FD
		   0x2000 100-HD
		   0x2100 100-FD
		*/
#ifdef honor_default_port
		mdio_write(ioaddr, phy_addr, 0, mii_ctrl[dev->default_port & 7]);
#else
		mdio_write(ioaddr, phy_addr, 0, 0x3300);
#endif
	}

	/* We can safely take handler calls during init.
	   Doing this after speedo_init_rx_ring() results in a memory leak. */
	if (request_irq(dev->irq, &speedo_interrupt, SA_SHIRQ, dev->name, dev)) {
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}

	speedo_init_rx_ring(dev);

	/* Fire up the hardware. */
	speedo_resume(dev);
	netif_start_tx_queue(dev);

	/* Setup the chip and configure the multicast list. */
	sp->mc_setup_frm = NULL;
	sp->mc_setup_frm_len = 0;
	sp->mc_setup_busy = 0;
	sp->rx_mode = RxInvalidMode;		/* Invalid -> always reset the mode. */
	sp->flow_ctrl = sp->partner = 0;
	set_rx_mode(dev);

	if (sp->msg_level & NETIF_MSG_IFUP)
		printk(KERN_DEBUG "%s: Done speedo_open(), status %8.8x.\n",
			   dev->name, (int)inw(ioaddr + SCBStatus));

	/* Set the timer.  The timer serves a dual purpose:
	   1) to monitor the media interface (e.g. link beat) and perhaps switch
	   to an alternate media type
	   2) to monitor Rx activity, and restart the Rx process if the receiver
	   hangs. */
	init_timer(&sp->timer);
	sp->timer.expires = jiffies + 3*HZ;
	sp->timer.data = (unsigned long)dev;
	sp->timer.function = &speedo_timer;					/* timer handler */
	add_timer(&sp->timer);

	/* No need to wait for the command unit to accept here. */
	if ((sp->phy[0] & 0x8000) == 0)
		mdio_read(dev, sp->phy[0] & 0x1f, 0);
	return 0;
}

/* Start the chip hardware after a full reset. */
static void speedo_resume(struct net_device *dev)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	long ioaddr = dev->base_addr;

	outw(SCBMaskAll, ioaddr + SCBCmd);

	/* Start with a Tx threshold of 256 (0x..20.... 8 byte units). */
	sp->tx_threshold = 0x01208000;

	/* Set the segment registers to '0'. */
	wait_for_cmd_done(dev);
	if (inb(ioaddr + SCBCmd)) {
		outl(PortPartialReset, ioaddr + SCBPort);
		udelay(10);
	}
	outl(0, ioaddr + SCBPointer);
	inl(ioaddr + SCBPointer);				/* Flush to PCI. */
	udelay(10);					/* Bogus, but it avoids the bug. */
	/* Note: these next two operations can take a while. */
	do_slow_command(dev, RxAddrLoad);
	do_slow_command(dev, CUCmdBase);

	/* Load the statistics block and rx ring addresses. */
	outl(virt_to_bus(&sp->lstats), ioaddr + SCBPointer);
	inl(ioaddr + SCBPointer);				/* Flush to PCI. */
	outb(CUStatsAddr, ioaddr + SCBCmd);
	sp->lstats.done_marker = 0;
	wait_for_cmd_done(dev);

	outl(virt_to_bus(sp->rx_ringp[sp->cur_rx % RX_RING_SIZE]),
		 ioaddr + SCBPointer);
	inl(ioaddr + SCBPointer);				/* Flush to PCI. */
	/* Note: RxStart should complete instantly. */
	do_slow_command(dev, RxStart);
	do_slow_command(dev, CUDumpStats);

	/* Fill the first command with our physical address. */
	{
		int entry = sp->cur_tx++ % TX_RING_SIZE;
		struct descriptor *cur_cmd = (struct descriptor *)&sp->tx_ring[entry];

		/* Avoid a bug(?!) here by marking the command already completed. */
		cur_cmd->cmd_status = cpu_to_le32((CmdSuspend | CmdIASetup) | 0xa000);
		cur_cmd->link =
			virt_to_le32desc(&sp->tx_ring[sp->cur_tx % TX_RING_SIZE]);
		memcpy(cur_cmd->params, dev->dev_addr, 6);
		if (sp->last_cmd)
			clear_suspend(sp->last_cmd);
		sp->last_cmd = cur_cmd;
	}

	/* Start the chip's Tx process and unmask interrupts. */
	outl(virt_to_bus(&sp->tx_ring[sp->dirty_tx % TX_RING_SIZE]),
		 ioaddr + SCBPointer);
	outw(CUStart, ioaddr + SCBCmd);
}

/* Media monitoring and control. */
static void speedo_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int phy_num = sp->phy[0] & 0x1f;
	int status = inw(ioaddr + SCBStatus);

	if (sp->msg_level & NETIF_MSG_TIMER)
		printk(KERN_DEBUG "%s: Interface monitor tick, chip status %4.4x.\n",
			   dev->name, status);

	/* Normally we check every two seconds. */
	sp->timer.expires = jiffies + 2*HZ;

	if (sp->polling) {
		/* Continue to be annoying. */
		if (status & 0xfc00) {
			speedo_interrupt(dev->irq, dev, 0);
			if (jiffies - sp->last_reset > 10*HZ) {
				printk(KERN_ERR "%s: IRQ %d is still blocked!\n",
					   dev->name, dev->irq);
				sp->last_reset = jiffies;
			}
		} else if (jiffies - sp->last_reset > 10*HZ)
			sp->polling = 0;
		sp->timer.expires = jiffies + 2;
	}
	/* We have MII and lost link beat. */
	if ((sp->phy[0] & 0x8000) == 0) {
		int partner = mdio_read(dev, phy_num, 5);
		if (partner != sp->partner) {
			int flow_ctrl = sp->advertising & partner & 0x0400 ? 1 : 0;
			sp->partner = partner;
			if (flow_ctrl != sp->flow_ctrl) {
				sp->flow_ctrl = flow_ctrl;
				sp->rx_mode = RxInvalidMode;	/* Trigger a reload. */
			}
			/* Clear sticky bit. */
			mdio_read(dev, phy_num, 1);
			/* If link beat has returned... */
			if (mdio_read(dev, phy_num, 1) & 0x0004)
				netif_link_up(dev);
			else
				netif_link_down(dev);
		}
	}

	/* This no longer has a false-trigger window. */
	if (sp->cur_tx - sp->dirty_tx > 1 &&
		(jiffies - dev->trans_start) > TX_TIMEOUT  &&
		(jiffies - sp->last_cmd_time) > TX_TIMEOUT) {
		if (status == 0xffff) {
			if (jiffies - sp->last_reset > 10*HZ) {
				sp->last_reset = jiffies;
				printk(KERN_ERR "%s: The EEPro100 chip is missing!\n",
					   dev->name);
			}
		} else if (status & 0xfc00) {
			/* We have a blocked IRQ line.  This should never happen, but
			   we recover as best we can.*/
			if ( ! sp->polling) {
				if (jiffies - sp->last_reset > 10*HZ) {
					printk(KERN_ERR "%s: IRQ %d is physically blocked! (%4.4x)"
						   "Failing back to low-rate polling.\n",
						   dev->name, dev->irq, status);
					sp->last_reset = jiffies;
				}
				sp->polling = 1;
			}
			speedo_interrupt(dev->irq, dev, 0);
			sp->timer.expires = jiffies + 2;	/* Avoid  */
		} else {
			speedo_tx_timeout(dev);
			sp->last_reset = jiffies;
		}
	}
	if (sp->rx_mode == RxInvalidMode  ||
		(sp->rx_bug  && jiffies - sp->last_rx_time > 2*HZ)) {
		/* We haven't received a packet in a Long Time.  We might have been
		   bitten by the receiver hang bug.  This can be cleared by sending
		   a set multicast list command. */
		set_rx_mode(dev);
	}
	add_timer(&sp->timer);
}

static void speedo_show_state(struct net_device *dev)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	int phy_num = sp->phy[0] & 0x1f;
	int i;

	/* Print a few items for debugging. */
	if (sp->msg_level & NETIF_MSG_DRV) {
		int i;
		printk(KERN_DEBUG "%s: Tx ring dump,  Tx queue %d / %d:\n", dev->name,
			   sp->cur_tx, sp->dirty_tx);
		for (i = 0; i < TX_RING_SIZE; i++)
			printk(KERN_DEBUG "%s: %c%c%d %8.8x.\n", dev->name,
				   i == sp->dirty_tx % TX_RING_SIZE ? '*' : ' ',
				   i == sp->cur_tx % TX_RING_SIZE ? '=' : ' ',
				   i, sp->tx_ring[i].status);
	}
	printk(KERN_DEBUG "%s:Printing Rx ring (next to receive into %d).\n",
		   dev->name, sp->cur_rx);

	for (i = 0; i < RX_RING_SIZE; i++)
		printk(KERN_DEBUG "  Rx ring entry %d  %8.8x.\n",
			   i, sp->rx_ringp[i] ? (int)sp->rx_ringp[i]->status : 0);

	for (i = 0; i < 16; i++) {
		if (i == 6) i = 21;
		printk(KERN_DEBUG "  PHY index %d register %d is %4.4x.\n",
			   phy_num, i, mdio_read(dev, phy_num, i));
	}

}

/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void
speedo_init_rx_ring(struct net_device *dev)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	struct RxFD *rxf, *last_rxf = NULL;
	int i;

	sp->cur_rx = 0;
#if defined(CONFIG_VLAN)
	/* Note that buffer sizing is not a run-time check! */
	sp->rx_buf_sz = dev->mtu + 14 + sizeof(struct RxFD) + 4;
#else
	sp->rx_buf_sz = dev->mtu + 14 + sizeof(struct RxFD);
#endif
	if (sp->rx_buf_sz < PKT_BUF_SZ)
		sp->rx_buf_sz = PKT_BUF_SZ;

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb;
		skb = dev_alloc_skb(sp->rx_buf_sz);
		sp->rx_skbuff[i] = skb;
		if (skb == NULL)
			break;			/* OK.  Just initially short of Rx bufs. */
		skb->dev = dev;			/* Mark as being used by this device. */
		rxf = (struct RxFD *)skb->tail;
		sp->rx_ringp[i] = rxf;
		skb_reserve(skb, sizeof(struct RxFD));
		if (last_rxf)
			last_rxf->link = virt_to_le32desc(rxf);
		last_rxf = rxf;
		rxf->status = cpu_to_le32(0x00000001);	/* '1' is flag value only. */
		rxf->link = 0;						/* None yet. */
		/* This field unused by i82557, we use it as a consistency check. */
#ifdef final_version
		rxf->rx_buf_addr = 0xffffffff;
#else
		rxf->rx_buf_addr = virt_to_bus(skb->tail);
#endif
		rxf->count = cpu_to_le32((sp->rx_buf_sz - sizeof(struct RxFD)) << 16);
	}
	sp->dirty_rx = (unsigned int)(i - RX_RING_SIZE);
	/* Mark the last entry as end-of-list. */
	last_rxf->status = cpu_to_le32(0xC0000002);	/* '2' is flag value only. */
	sp->last_rxf = last_rxf;
}

static void speedo_tx_timeout(struct net_device *dev)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int status = inw(ioaddr + SCBStatus);

	printk(KERN_WARNING "%s: Transmit timed out: status %4.4x "
		   " %4.4x at %d/%d commands %8.8x %8.8x %8.8x.\n",
		   dev->name, status, (int)inw(ioaddr + SCBCmd),
		   sp->dirty_tx, sp->cur_tx,
		   sp->tx_ring[(sp->dirty_tx+0) % TX_RING_SIZE].status,
		   sp->tx_ring[(sp->dirty_tx+1) % TX_RING_SIZE].status,
		   sp->tx_ring[(sp->dirty_tx+2) % TX_RING_SIZE].status);

	/* Trigger a stats dump to give time before the reset. */
	speedo_get_stats(dev);

	speedo_show_state(dev);
	if ((status & 0x00C0) != 0x0080
		&&  (status & 0x003C) == 0x0010  &&  0) {
		/* Only the command unit has stopped. */
		printk(KERN_WARNING "%s: Trying to restart the transmitter...\n",
			   dev->name);
		outl(virt_to_bus(&sp->tx_ring[sp->dirty_tx % TX_RING_SIZE]),
			 ioaddr + SCBPointer);
		outw(CUStart, ioaddr + SCBCmd);
	} else {
		printk(KERN_WARNING "%s: Restarting the chip...\n",
			   dev->name);
		/* Reset the Tx and Rx units. */
		outl(PortReset, ioaddr + SCBPort);
		if (sp->msg_level & NETIF_MSG_TX_ERR)
			speedo_show_state(dev);
		udelay(10);
		speedo_resume(dev);
	}
	/* Reset the MII transceiver, suggested by Fred Young @ scalable.com. */
	if ((sp->phy[0] & 0x8000) == 0) {
		int phy_addr = sp->phy[0] & 0x1f;
		int advertising = mdio_read(dev, phy_addr, 4);
		int mii_bmcr = mdio_read(dev, phy_addr, 0);
		mdio_write(ioaddr, phy_addr, 0, 0x0400);
		mdio_write(ioaddr, phy_addr, 1, 0x0000);
		mdio_write(ioaddr, phy_addr, 4, 0x0000);
		mdio_write(ioaddr, phy_addr, 0, 0x8000);
#ifdef honor_default_port
		mdio_write(ioaddr, phy_addr, 0, mii_ctrl[dev->default_port & 7]);
#else
		mdio_read(dev, phy_addr, 0);
		mdio_write(ioaddr, phy_addr, 0, mii_bmcr);
		mdio_write(ioaddr, phy_addr, 4, advertising);
#endif
	}
	sp->stats.tx_errors++;
	dev->trans_start = jiffies;
	return;
}

/* Handle the interrupt cases when something unexpected happens. */
static void speedo_intr_error(struct net_device *dev, int intr_status)
{
	long ioaddr = dev->base_addr;
	struct speedo_private *sp = (struct speedo_private *)dev->priv;

	if (intr_status & IntrRxSuspend) {
		if ((intr_status & 0x003c) == 0x0028) /* No more Rx buffers. */
			outb(RxResumeNoResources, ioaddr + SCBCmd);
		else if ((intr_status & 0x003c) == 0x0008) { /* No resources (why?!) */
			printk(KERN_DEBUG "%s: Unknown receiver error, status=%#4.4x.\n",
				   dev->name, intr_status);
			/* No idea of what went wrong.  Restart the receiver. */
			outl(virt_to_bus(sp->rx_ringp[sp->cur_rx % RX_RING_SIZE]),
				 ioaddr + SCBPointer);
			outb(RxStart, ioaddr + SCBCmd);
		}
		sp->stats.rx_errors++;
	}
}


static int
speedo_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int entry;

	/* Block a timer-based transmit from overlapping.  This could better be
	   done with atomic_swap(1, dev->tbusy), but set_bit() works as well.
	   If this ever occurs the queue layer is doing something evil! */
	if (netif_pause_tx_queue(dev) != 0) {
		int tickssofar = jiffies - dev->trans_start;
		if (tickssofar < TX_TIMEOUT - 2)
			return 1;
		if (tickssofar < TX_TIMEOUT) {
			/* Reap sent packets from the full Tx queue. */
			outw(SCBTriggerIntr, ioaddr + SCBCmd);
			return 1;
		}
		speedo_tx_timeout(dev);
		return 1;
	}

	/* Caution: the write order is important here, set the base address
	   with the "ownership" bits last. */

	{	/* Prevent interrupts from changing the Tx ring from underneath us. */
		unsigned long flags;

		spin_lock_irqsave(&sp->lock, flags);
		/* Calculate the Tx descriptor entry. */
		entry = sp->cur_tx % TX_RING_SIZE;

		sp->tx_skbuff[entry] = skb;
		/* Todo: be a little more clever about setting the interrupt bit. */
		sp->tx_ring[entry].status =
			cpu_to_le32(CmdSuspend | CmdTx | CmdTxFlex);
		sp->cur_tx++;
		sp->tx_ring[entry].link =
			virt_to_le32desc(&sp->tx_ring[sp->cur_tx % TX_RING_SIZE]);
		/* We may nominally release the lock here. */
		sp->tx_ring[entry].tx_desc_addr =
			virt_to_le32desc(&sp->tx_ring[entry].tx_buf_addr0);
		/* The data region is always in one buffer descriptor. */
		sp->tx_ring[entry].count = cpu_to_le32(sp->tx_threshold);
		sp->tx_ring[entry].tx_buf_addr0 = virt_to_le32desc(skb->data);
		sp->tx_ring[entry].tx_buf_size0 = cpu_to_le32(skb->len);
		/* Todo: perhaps leave the interrupt bit set if the Tx queue is more
		   than half full.  Argument against: we should be receiving packets
		   and scavenging the queue.  Argument for: if so, it shouldn't
		   matter. */
		{
			struct descriptor *last_cmd = sp->last_cmd;
			sp->last_cmd = (struct descriptor *)&sp->tx_ring[entry];
			clear_suspend(last_cmd);
		}
		if (sp->cur_tx - sp->dirty_tx >= TX_QUEUE_LIMIT) {
			sp->tx_full = 1;
			netif_stop_tx_queue(dev);
		} else
			netif_unpause_tx_queue(dev);
		spin_unlock_irqrestore(&sp->lock, flags);
	}
	wait_for_cmd_done(dev);
	outb(CUResume, ioaddr + SCBCmd);
	dev->trans_start = jiffies;

	return 0;
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void speedo_interrupt(int irq, void *dev_instance, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)dev_instance;
	struct speedo_private *sp;
	long ioaddr;
	int work_limit;
	u16 status;

	ioaddr = dev->base_addr;
	sp = (struct speedo_private *)dev->priv;
	work_limit = sp->max_interrupt_work;
#ifndef final_version
	/* A lock to prevent simultaneous entry on SMP machines. */
	if (test_and_set_bit(0, (void*)&sp->in_interrupt)) {
		printk(KERN_ERR"%s: SMP simultaneous entry of an interrupt handler.\n",
			   dev->name);
		sp->in_interrupt = 0;	/* Avoid halting machine. */
		return;
	}
#endif

	do {
		status = inw(ioaddr + SCBStatus);

		if ((status & IntrAllNormal) == 0  ||  status == 0xffff)
			break;
		/* Acknowledge all of the current interrupt sources ASAP. */
		outw(status & IntrAllNormal, ioaddr + SCBStatus);

		if (sp->msg_level & NETIF_MSG_INTR)
			printk(KERN_DEBUG "%s: interrupt  status=%#4.4x.\n",
				   dev->name, status);

		if (status & (IntrRxDone|IntrRxSuspend))
			speedo_rx(dev);

		/* The command unit did something, scavenge finished Tx entries. */
		if (status & (IntrCmdDone | IntrCmdIdle | IntrDrvrIntr)) {
			unsigned int dirty_tx;
			/* We should nominally not need this lock. */
			spin_lock(&sp->lock);

			dirty_tx = sp->dirty_tx;
			while (sp->cur_tx - dirty_tx > 0) {
				int entry = dirty_tx % TX_RING_SIZE;
				int status = le32_to_cpu(sp->tx_ring[entry].status);

				if (sp->msg_level & NETIF_MSG_INTR)
					printk(KERN_DEBUG " scavenge candidate %d status %4.4x.\n",
						   entry, status);
				if ((status & StatusComplete) == 0) {
					/* Special case error check: look for descriptor that the
					   chip skipped(?). */
					if (sp->cur_tx - dirty_tx > 2  &&
						(sp->tx_ring[(dirty_tx+1) % TX_RING_SIZE].status
						 & cpu_to_le32(StatusComplete))) {
						printk(KERN_ERR "%s: Command unit failed to mark "
							   "command %8.8x as complete at %d.\n",
							   dev->name, status, dirty_tx);
					} else
						break;			/* It still hasn't been processed. */
				}
				if ((status & TxUnderrun) &&
					(sp->tx_threshold < 0x01e08000)) {
					sp->tx_threshold += 0x00040000;
					if (sp->msg_level & NETIF_MSG_TX_ERR)
						printk(KERN_DEBUG "%s: Tx threshold increased, "
							   "%#8.8x.\n", dev->name, sp->tx_threshold);
				}
				/* Free the original skb. */
				if (sp->tx_skbuff[entry]) {
					sp->stats.tx_packets++;	/* Count only user packets. */
#if LINUX_VERSION_CODE > 0x20127
					sp->stats.tx_bytes += sp->tx_skbuff[entry]->len;
#endif
					dev_free_skb_irq(sp->tx_skbuff[entry]);
					sp->tx_skbuff[entry] = 0;
				} else if ((status & 0x70000) == CmdNOp)
					sp->mc_setup_busy = 0;
				dirty_tx++;
			}

#ifndef final_version
			if (sp->cur_tx - dirty_tx > TX_RING_SIZE) {
				printk(KERN_ERR "out-of-sync dirty pointer, %d vs. %d,"
					   " full=%d.\n",
					   dirty_tx, sp->cur_tx, sp->tx_full);
				dirty_tx += TX_RING_SIZE;
			}
#endif

			sp->dirty_tx = dirty_tx;
			if (sp->tx_full
				&&  sp->cur_tx - dirty_tx < TX_QUEUE_UNFULL) {
				/* The ring is no longer full, clear tbusy. */
				sp->tx_full = 0;
				netif_resume_tx_queue(dev);
			}
			spin_unlock(&sp->lock);
		}

		if (status & IntrRxSuspend)
			speedo_intr_error(dev, status);

		if (--work_limit < 0) {
			printk(KERN_ERR "%s: Too much work at interrupt, status=0x%4.4x.\n",
				   dev->name, status);
			/* Clear all interrupt sources. */
			outl(0xfc00, ioaddr + SCBStatus);
			break;
		}
	} while (1);

	if (sp->msg_level & NETIF_MSG_INTR)
		printk(KERN_DEBUG "%s: exiting interrupt, status=%#4.4x.\n",
			   dev->name, (int)inw(ioaddr + SCBStatus));

	clear_bit(0, (void*)&sp->in_interrupt);
	return;
}

static int
speedo_rx(struct net_device *dev)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	int entry = sp->cur_rx % RX_RING_SIZE;
	int status;
	int rx_work_limit = sp->dirty_rx + RX_RING_SIZE - sp->cur_rx;

	if (sp->msg_level & NETIF_MSG_RX_STATUS)
		printk(KERN_DEBUG " In speedo_rx().\n");
	/* If we own the next entry, it's a new packet. Send it up. */
	while (sp->rx_ringp[entry] != NULL &&
		   (status = le32_to_cpu(sp->rx_ringp[entry]->status)) & RxComplete) {
		int desc_count = le32_to_cpu(sp->rx_ringp[entry]->count);
		int pkt_len = desc_count & 0x07ff;

		if (--rx_work_limit < 0)
			break;
		if (sp->msg_level & NETIF_MSG_RX_STATUS)
			printk(KERN_DEBUG "  speedo_rx() status %8.8x len %d.\n", status,
				   pkt_len);
		if ((status & (RxErrTooBig|RxOK|0x0f90)) != RxOK) {
			if (status & RxErrTooBig)
				printk(KERN_ERR "%s: Ethernet frame overran the Rx buffer, "
					   "status %8.8x!\n", dev->name, status);
			else if ( ! (status & RxOK)) {
				/* There was a fatal error.  This *should* be impossible. */
				sp->stats.rx_errors++;
				printk(KERN_ERR "%s: Anomalous event in speedo_rx(), "
					   "status %8.8x.\n", dev->name, status);
			}
		} else {
			struct sk_buff *skb;

			if (sp->drv_flags & HasChksum)
				pkt_len -= 2;

			/* Check if the packet is long enough to just accept without
			   copying to a properly sized skbuff. */
			if (pkt_len < sp->rx_copybreak
				&& (skb = dev_alloc_skb(pkt_len + 2)) != 0) {
				skb->dev = dev;
				skb_reserve(skb, 2);	/* Align IP on 16 byte boundaries */
				/* 'skb_put()' points to the start of sk_buff data area. */
				/* Packet is in one chunk -- we can copy + cksum. */
				eth_copy_and_sum(skb, sp->rx_skbuff[entry]->tail, pkt_len, 0);
				skb_put(skb, pkt_len);
			} else {
				void *temp;
				/* Pass up the already-filled skbuff. */
				skb = sp->rx_skbuff[entry];
				if (skb == NULL) {
					printk(KERN_ERR "%s: Inconsistent Rx descriptor chain.\n",
						   dev->name);
					break;
				}
				sp->rx_skbuff[entry] = NULL;
				temp = skb_put(skb, pkt_len);
#if !defined(final_version) && !defined(__powerpc__)
				if (bus_to_virt(sp->rx_ringp[entry]->rx_buf_addr) != temp)
					printk(KERN_ERR "%s: Rx consistency error -- the skbuff "
						   "addresses do not match in speedo_rx: %p vs. %p "
						   "/ %p.\n", dev->name,
						   bus_to_virt(sp->rx_ringp[entry]->rx_buf_addr),
						   skb->head, temp);
#endif
				sp->rx_ringp[entry] = NULL;
			}
			skb->protocol = eth_type_trans(skb, dev);
			if (sp->drv_flags & HasChksum) {
#if 0
				u16 csum = get_unaligned((u16*)(skb->head + pkt_len))
				if (desc_count & 0x8000)
					skb->ip_summed = CHECKSUM_UNNECESSARY;
#endif
			}
			netif_rx(skb);
			sp->stats.rx_packets++;
#if LINUX_VERSION_CODE > 0x20127
			sp->stats.rx_bytes += pkt_len;
#endif
		}
		entry = (++sp->cur_rx) % RX_RING_SIZE;
	}

	/* Refill the Rx ring buffers. */
	for (; sp->cur_rx - sp->dirty_rx > 0; sp->dirty_rx++) {
		struct RxFD *rxf;
		entry = sp->dirty_rx % RX_RING_SIZE;
		if (sp->rx_skbuff[entry] == NULL) {
			struct sk_buff *skb;
			/* Get a fresh skbuff to replace the consumed one. */
			skb = dev_alloc_skb(sp->rx_buf_sz);
			sp->rx_skbuff[entry] = skb;
			if (skb == NULL) {
				sp->rx_ringp[entry] = NULL;
				sp->alloc_failures++;
				break;			/* Better luck next time!  */
			}
			rxf = sp->rx_ringp[entry] = (struct RxFD *)skb->tail;
			skb->dev = dev;
			skb_reserve(skb, sizeof(struct RxFD));
			rxf->rx_buf_addr = virt_to_le32desc(skb->tail);
		} else {
			rxf = sp->rx_ringp[entry];
		}
		rxf->status = cpu_to_le32(0xC0000001); 	/* '1' for driver use only. */
		rxf->link = 0;			/* None yet. */
		rxf->count = cpu_to_le32((sp->rx_buf_sz - sizeof(struct RxFD)) << 16);
		sp->last_rxf->link = virt_to_le32desc(rxf);
		sp->last_rxf->status &= cpu_to_le32(~0xC0000000);
		sp->last_rxf = rxf;
	}

	sp->last_rx_time = jiffies;
	return 0;
}

static int
speedo_close(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	int i;

	netif_stop_tx_queue(dev);

	if (sp->msg_level & NETIF_MSG_IFDOWN)
		printk(KERN_DEBUG "%s: Shutting down ethercard, status was %4.4x.\n"
			   KERN_DEBUG "%s:   Cumlative allocation failures: %d.\n",
			   dev->name, (int)inw(ioaddr + SCBStatus),
			   dev->name, sp->alloc_failures);

	/* Shut off the media monitoring timer. */
	del_timer(&sp->timer);

	/* Shutting down the chip nicely fails to disable flow control. So.. */
	outl(PortPartialReset, ioaddr + SCBPort);

	free_irq(dev->irq, dev);

	/* Free all the skbuffs in the Rx and Tx queues. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = sp->rx_skbuff[i];
		sp->rx_skbuff[i] = 0;
		/* Clear the Rx descriptors. */
		if (skb) {
#if LINUX_VERSION_CODE < 0x20100
			skb->free = 1;
#endif
			dev_free_skb(skb);
		}
	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		struct sk_buff *skb = sp->tx_skbuff[i];
		sp->tx_skbuff[i] = 0;
		/* Clear the Tx descriptors. */
		if (skb)
			dev_free_skb(skb);
	}
	if (sp->mc_setup_frm) {
		kfree(sp->mc_setup_frm);
		sp->mc_setup_frm_len = 0;
	}

	/* Print a few items for debugging. */
	if (sp->msg_level & NETIF_MSG_IFDOWN)
		speedo_show_state(dev);

	/* Alt: acpi_set_pwr_state(pdev, sp->acpi_pwr); */
	acpi_set_pwr_state(sp->pci_dev, ACPI_D2);
	MOD_DEC_USE_COUNT;

	return 0;
}

/* The Speedo-3 has an especially awkward and unusable method of getting
   statistics out of the chip.  It takes an unpredictable length of time
   for the dump-stats command to complete.  To avoid a busy-wait loop we
   update the stats with the previous dump results, and then trigger a
   new dump.

   These problems are mitigated by the current /proc implementation, which
   calls this routine first to judge the output length, and then to emit the
   output.

   Oh, and incoming frames are dropped while executing dump-stats!
   */
static struct net_device_stats *speedo_get_stats(struct net_device *dev)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	long ioaddr = dev->base_addr;

	/* Update only if the previous dump finished. */
	if (sp->lstats.done_marker == le32_to_cpu(0xA007)) {
		sp->stats.tx_aborted_errors += le32_to_cpu(sp->lstats.tx_coll16_errs);
		sp->stats.tx_window_errors += le32_to_cpu(sp->lstats.tx_late_colls);
		sp->stats.tx_fifo_errors += le32_to_cpu(sp->lstats.tx_underruns);
		sp->stats.tx_fifo_errors += le32_to_cpu(sp->lstats.tx_lost_carrier);
		/*sp->stats.tx_deferred += le32_to_cpu(sp->lstats.tx_deferred);*/
		sp->stats.collisions += le32_to_cpu(sp->lstats.tx_total_colls);
		sp->stats.rx_crc_errors += le32_to_cpu(sp->lstats.rx_crc_errs);
		sp->stats.rx_frame_errors += le32_to_cpu(sp->lstats.rx_align_errs);
		sp->stats.rx_over_errors += le32_to_cpu(sp->lstats.rx_resource_errs);
		sp->stats.rx_fifo_errors += le32_to_cpu(sp->lstats.rx_overrun_errs);
		sp->stats.rx_length_errors += le32_to_cpu(sp->lstats.rx_runt_errs);
		sp->lstats.done_marker = 0x0000;
		if (netif_running(dev)) {
			wait_for_cmd_done(dev);
			outb(CUDumpStats, ioaddr + SCBCmd);
		}
	}
	return &sp->stats;
}

static int speedo_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	long ioaddr = dev->base_addr;
	u16 *data = (u16 *)&rq->ifr_data;
	u32 *data32 = (void *)&rq->ifr_data;
	int phy = sp->phy[0] & 0x1f;
	int saved_acpi;

	switch(cmd) {
	case 0x8947: case 0x89F0:
		/* SIOCGMIIPHY: Get the address of the PHY in use. */
		data[0] = phy;
		/* Fall Through */
	case 0x8948: case 0x89F1:
		/* SIOCGMIIREG: Read the specified MII register. */
		saved_acpi = acpi_set_pwr_state(sp->pci_dev, ACPI_D0);
		data[3] = mdio_read(dev, data[0], data[1]);
		acpi_set_pwr_state(sp->pci_dev, saved_acpi);
		return 0;
	case 0x8949: case 0x89F2:
		/* SIOCSMIIREG: Write the specified MII register */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		if (data[0] == sp->phy[0]) {
			u16 value = data[2];
			switch (data[1]) {
			case 0:
				/* Check for autonegotiation on or reset. */
				sp->medialock = (value & 0x9000) ? 0 : 1;
				if (sp->medialock) {
					sp->full_duplex = (value & 0x0100) ? 1 : 0;
					sp->rx_mode = RxInvalidMode;
				}
				break;
			case 4: sp->advertising = value; break;
			}
		}
		saved_acpi = acpi_set_pwr_state(sp->pci_dev, ACPI_D0);
		mdio_write(ioaddr, data[0], data[1], data[2]);
		acpi_set_pwr_state(sp->pci_dev, saved_acpi);
		return 0;
	case SIOCGPARAMS:
		data32[0] = sp->msg_level;
		data32[1] = sp->multicast_filter_limit;
		data32[2] = sp->max_interrupt_work;
		data32[3] = sp->rx_copybreak;
#if 0
		/* No room in the ioctl() to set these. */
		data32[4] = txfifo;
		data32[5] = rxfifo;
#endif
		return 0;
	case SIOCSPARAMS:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		sp->msg_level = data32[0];
		sp->multicast_filter_limit = data32[1];
		sp->max_interrupt_work = data32[2];
		sp->rx_copybreak = data32[3];
#if 0
		/* No room in the ioctl() to set these. */
		if (data32[4] < 16)
			txfifo = data32[4];
		if (data32[5] < 16)
			rxfifo = data32[5];
#endif
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

/* Set or clear the multicast filter for this adaptor.
   This is very ugly with Intel chips -- we usually have to execute an
   entire configuration command, plus process a multicast command.
   This is complicated.  We must put a large configuration command and
   an arbitrarily-sized multicast command in the transmit list.
   To minimize the disruption -- the previous command might have already
   loaded the link -- we convert the current command block, normally a Tx
   command, into a no-op and link it to the new command.
*/
static void set_rx_mode(struct net_device *dev)
{
	struct speedo_private *sp = (struct speedo_private *)dev->priv;
	long ioaddr = dev->base_addr;
	struct descriptor *last_cmd;
	char new_rx_mode;
	unsigned long flags;
	int entry, i;

	if (dev->flags & IFF_PROMISC) {			/* Set promiscuous. */
		new_rx_mode = AcceptAllMulticast | AcceptAllPhys;
	} else if ((dev->flags & IFF_ALLMULTI)  ||
			   dev->mc_count > sp->multicast_filter_limit) {
		new_rx_mode = AcceptAllMulticast;
	} else
		new_rx_mode = 0;

	if (sp->cur_tx - sp->dirty_tx >= TX_RING_SIZE - 1) {
	  /* The Tx ring is full -- don't add anything!  Presumably the new mode
		 is in config_cmd_data and will be added anyway, otherwise we wait
		 for a timer tick or the mode to change again. */
		sp->rx_mode = RxInvalidMode;
		return;
	}

	if (new_rx_mode != sp->rx_mode) {
		u8 *config_cmd_data;

		spin_lock_irqsave(&sp->lock, flags);
		entry = sp->cur_tx % TX_RING_SIZE;
		last_cmd = sp->last_cmd;
		sp->last_cmd = (struct descriptor *)&sp->tx_ring[entry];

		sp->tx_skbuff[entry] = 0;			/* Redundant. */
		sp->tx_ring[entry].status = cpu_to_le32(CmdSuspend | CmdConfigure);
		sp->cur_tx++;
		sp->tx_ring[entry].link =
			virt_to_le32desc(&sp->tx_ring[(entry + 1) % TX_RING_SIZE]);
		/* We may nominally release the lock here. */

		config_cmd_data = (void *)&sp->tx_ring[entry].tx_desc_addr;
		/* Construct a full CmdConfig frame. */
		memcpy(config_cmd_data, i82558_config_cmd, sizeof(i82558_config_cmd));
		config_cmd_data[1] = (txfifo << 4) | rxfifo;
		config_cmd_data[4] = rxdmacount;
		config_cmd_data[5] = txdmacount + 0x80;
		config_cmd_data[6] |= (new_rx_mode & AcceptErr) ? 0x80 : 0;
		config_cmd_data[7] &= (new_rx_mode & AcceptRunt) ? ~0x01 : ~0;
		if (sp->drv_flags & HasChksum)
			config_cmd_data[9] |= 1;
		config_cmd_data[15] |= (new_rx_mode & AcceptAllPhys) ? 1 : 0;
		config_cmd_data[19] = sp->flow_ctrl ? 0xBD : 0x80;
		config_cmd_data[19] |= sp->full_duplex ? 0x40 : 0;
		config_cmd_data[21] = (new_rx_mode & AcceptAllMulticast) ? 0x0D : 0x05;
		if (sp->phy[0] & 0x8000) {			/* Use the AUI port instead. */
			config_cmd_data[15] |= 0x80;
			config_cmd_data[8] = 0;
		}
		/* Trigger the command unit resume. */
		wait_for_cmd_done(dev);
		clear_suspend(last_cmd);
		outb(CUResume, ioaddr + SCBCmd);
		spin_unlock_irqrestore(&sp->lock, flags);
		sp->last_cmd_time = jiffies;
	}

	if (new_rx_mode == 0  &&  dev->mc_count < 4) {
		/* The simple case of 0-3 multicast list entries occurs often, and
		   fits within one tx_ring[] entry. */
		struct dev_mc_list *mclist;
		u16 *setup_params, *eaddrs;

		spin_lock_irqsave(&sp->lock, flags);
		entry = sp->cur_tx % TX_RING_SIZE;
		last_cmd = sp->last_cmd;
		sp->last_cmd = (struct descriptor *)&sp->tx_ring[entry];

		sp->tx_skbuff[entry] = 0;
		sp->tx_ring[entry].status = cpu_to_le32(CmdSuspend | CmdMulticastList);
		sp->cur_tx++;
		sp->tx_ring[entry].link =
			virt_to_le32desc(&sp->tx_ring[(entry + 1) % TX_RING_SIZE]);
		/* We may nominally release the lock here. */
		sp->tx_ring[entry].tx_desc_addr = 0; /* Really MC list count. */
		setup_params = (u16 *)&sp->tx_ring[entry].tx_desc_addr;
		*setup_params++ = cpu_to_le16(dev->mc_count*6);
		/* Fill in the multicast addresses. */
		for (i = 0, mclist = dev->mc_list; i < dev->mc_count;
			 i++, mclist = mclist->next) {
			eaddrs = (u16 *)mclist->dmi_addr;
			*setup_params++ = *eaddrs++;
			*setup_params++ = *eaddrs++;
			*setup_params++ = *eaddrs++;
		}

		wait_for_cmd_done(dev);
		clear_suspend(last_cmd);
		/* Immediately trigger the command unit resume. */
		outb(CUResume, ioaddr + SCBCmd);
		spin_unlock_irqrestore(&sp->lock, flags);
		sp->last_cmd_time = jiffies;
	} else if (new_rx_mode == 0) {
		struct dev_mc_list *mclist;
		u16 *setup_params, *eaddrs;
		struct descriptor *mc_setup_frm = sp->mc_setup_frm;
		int i;

		if (sp->mc_setup_frm_len < 10 + dev->mc_count*6
			|| sp->mc_setup_frm == NULL) {
			/* Allocate a full setup frame, 10bytes + <max addrs>. */
			if (sp->mc_setup_frm)
				kfree(sp->mc_setup_frm);
			sp->mc_setup_busy = 0;
			sp->mc_setup_frm_len = 10 + sp->multicast_filter_limit*6;
			sp->mc_setup_frm = kmalloc(sp->mc_setup_frm_len, GFP_ATOMIC);
			if (sp->mc_setup_frm == NULL) {
				printk(KERN_ERR "%s: Failed to allocate a setup frame.\n",
					   dev->name);
				sp->rx_mode = RxInvalidMode; /* We failed, try again. */
				return;
			}
		}
		/* If we are busy, someone might be quickly adding to the MC list.
		   Try again later when the list updates stop. */
		if (sp->mc_setup_busy) {
			sp->rx_mode = RxInvalidMode;
			return;
		}
		mc_setup_frm = sp->mc_setup_frm;
		/* Fill the setup frame. */
		if (sp->msg_level & NETIF_MSG_RXFILTER)
			printk(KERN_DEBUG "%s: Constructing a setup frame at %p, "
				   "%d bytes.\n",
				   dev->name, sp->mc_setup_frm, sp->mc_setup_frm_len);
		mc_setup_frm->cmd_status =
			cpu_to_le32(CmdSuspend | CmdIntr | CmdMulticastList);
		/* Link set below. */
		setup_params = (u16 *)&mc_setup_frm->params;
		*setup_params++ = cpu_to_le16(dev->mc_count*6);
		/* Fill in the multicast addresses. */
		for (i = 0, mclist = dev->mc_list; i < dev->mc_count;
			 i++, mclist = mclist->next) {
			eaddrs = (u16 *)mclist->dmi_addr;
			*setup_params++ = *eaddrs++;
			*setup_params++ = *eaddrs++;
			*setup_params++ = *eaddrs++;
		}

		/* Disable interrupts while playing with the Tx Cmd list. */
		spin_lock_irqsave(&sp->lock, flags);
		entry = sp->cur_tx % TX_RING_SIZE;
		last_cmd = sp->last_cmd;
		sp->last_cmd = mc_setup_frm;
		sp->mc_setup_busy++;

		/* Change the command to a NoOp, pointing to the CmdMulti command. */
		sp->tx_skbuff[entry] = 0;
		sp->tx_ring[entry].status = cpu_to_le32(CmdNOp);
		sp->cur_tx++;
		sp->tx_ring[entry].link = virt_to_le32desc(mc_setup_frm);
		/* We may nominally release the lock here. */

		/* Set the link in the setup frame. */
		mc_setup_frm->link =
			virt_to_le32desc(&(sp->tx_ring[(entry+1) % TX_RING_SIZE]));

		wait_for_cmd_done(dev);
		clear_suspend(last_cmd);
		/* Immediately trigger the command unit resume. */
		outb(CUResume, ioaddr + SCBCmd);
		spin_unlock_irqrestore(&sp->lock, flags);
		sp->last_cmd_time = jiffies;
		if (sp->msg_level & NETIF_MSG_RXFILTER)
			printk(KERN_DEBUG " CmdMCSetup frame length %d in entry %d.\n",
				   dev->mc_count, entry);
	}

	sp->rx_mode = new_rx_mode;
}

static int speedo_pwr_event(void *dev_instance, int event)
{
	struct net_device *dev = dev_instance;
	struct speedo_private *np = (struct speedo_private *)dev->priv;
	long ioaddr = dev->base_addr;

	if (np->msg_level & NETIF_MSG_LINK)
		printk(KERN_DEBUG "%s: Handling power event %d.\n", dev->name, event);
	switch(event) {
	case DRV_ATTACH:
		MOD_INC_USE_COUNT;
		break;
	case DRV_SUSPEND:
		outl(PortPartialReset, ioaddr + SCBPort);
		break;
	case DRV_RESUME:
		speedo_resume(dev);
		np->rx_mode = RxInvalidMode;
		np->flow_ctrl = np->partner = 0;
		set_rx_mode(dev);
		break;
	case DRV_DETACH: {
		struct net_device **devp, **next;
		if (dev->flags & IFF_UP) {
			dev_close(dev);
			dev->flags &= ~(IFF_UP|IFF_RUNNING);
		}
		unregister_netdev(dev);
		release_region(dev->base_addr, pci_id_tbl[np->chip_id].io_size);
#ifndef USE_IO_OPS
		iounmap((char *)dev->base_addr);
#endif
		for (devp = &root_speedo_dev; *devp; devp = next) {
			next = &((struct speedo_private *)(*devp)->priv)->next_module;
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
	case DRV_PWR_DOWN:
	case DRV_PWR_UP:
		acpi_set_pwr_state(np->pci_dev, event==DRV_PWR_DOWN ? ACPI_D3:ACPI_D0);
		break;
	case DRV_PWR_WakeOn:
	default:
		return -1;
	}

	return 0;
}


#if defined(MODULE) || (LINUX_VERSION_CODE >= 0x020400)

int init_module(void)
{
	int cards_found;

	/* Emit version even if no cards detected. */
	printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	cards_found = pci_drv_register(&eepro100_drv_id, NULL);
	if (cards_found < 0)
		printk(KERN_INFO "eepro100: No cards found, driver not installed.\n");
	return cards_found;
}

void cleanup_module(void)
{
	struct net_device *next_dev;

	pci_drv_unregister(&eepro100_drv_id);

	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	while (root_speedo_dev) {
		struct speedo_private *sp = (void *)root_speedo_dev->priv;
		unregister_netdev(root_speedo_dev);
#ifdef USE_IO_OPS
		release_region(root_speedo_dev->base_addr,
					   pci_id_tbl[sp->chip_id].io_size);
#else
		iounmap((char *)root_speedo_dev->base_addr);
#endif
		acpi_set_pwr_state(sp->pci_dev, sp->acpi_pwr);
		next_dev = sp->next_module;
		if (sp->priv_addr)
			kfree(sp->priv_addr);
		kfree(root_speedo_dev);
		root_speedo_dev = next_dev;
	}
}

#if (LINUX_VERSION_CODE >= 0x020400)  && 0
module_init(init_module);
module_exit(cleanup_module);
#endif

#else   /* not MODULE */

int eepro100_probe(struct net_device *dev)
{
	int cards_found =  pci_drv_register(&eepro100_drv_id, dev);

	/* Only emit the version if the driver is being used. */
	if (cards_found >= 0)
		printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);

	return cards_found;
}
#endif  /* MODULE */

/*
 * Local variables:
 *  compile-command: "make KERNVER=`uname -r` eepro100.o"
 *  compile-cmd: "gcc -DMODULE -Wall -Wstrict-prototypes -O6 -c eepro100.c"
 *  simple-compile-command: "gcc -DMODULE -O6 -c eepro100.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
