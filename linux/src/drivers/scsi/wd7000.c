/* $Id: wd7000.c,v 1.1 1999/04/26 05:55:18 tb Exp $
 *  linux/drivers/scsi/wd7000.c
 *
 *  Copyright (C) 1992  Thomas Wuensche
 *	closely related to the aha1542 driver from Tommy Thorn
 *	( as close as different hardware allows on a lowlevel-driver :-) )
 *
 *  Revised (and renamed) by John Boyd <boyd@cis.ohio-state.edu> to
 *  accommodate Eric Youngdale's modifications to scsi.c.  Nov 1992.
 *
 *  Additional changes to support scatter/gather.  Dec. 1992.  tw/jb
 *
 *  No longer tries to reset SCSI bus at boot (it wasn't working anyway).
 *  Rewritten to support multiple host adapters.
 *  Miscellaneous cleanup.
 *  So far, still doesn't do reset or abort correctly, since I have no idea
 *  how to do them with this board (8^(.                      Jan 1994 jb
 *
 * This driver now supports both of the two standard configurations (per
 * the 3.36 Owner's Manual, my latest reference) by the same method as
 * before; namely, by looking for a BIOS signature.  Thus, the location of
 * the BIOS signature determines the board configuration.  Until I have
 * time to do something more flexible, users should stick to one of the
 * following:
 *
 * Standard configuration for single-adapter systems:
 *    - BIOS at CE00h
 *    - I/O base address 350h
 *    - IRQ level 15
 *    - DMA channel 6
 * Standard configuration for a second adapter in a system:
 *    - BIOS at C800h
 *    - I/O base address 330h
 *    - IRQ level 11
 *    - DMA channel 5
 *
 * Anyone who can recompile the kernel is welcome to add others as need
 * arises, but unpredictable results may occur if there are conflicts.
 * In any event, if there are multiple adapters in a system, they MUST
 * use different I/O bases, IRQ levels, and DMA channels, since they will be
 * indistinguishable (and in direct conflict) otherwise.
 *
 *   As a point of information, the NO_OP command toggles the CMD_RDY bit
 * of the status port, and this fact could be used as a test for the I/O
 * base address (or more generally, board detection).  There is an interrupt
 * status port, so IRQ probing could also be done.  I suppose the full
 * DMA diagnostic could be used to detect the DMA channel being used.  I
 * haven't done any of this, though, because I think there's too much of
 * a chance that such explorations could be destructive, if some other
 * board's resources are used inadvertently.  So, call me a wimp, but I
 * don't want to try it.  The only kind of exploration I trust is memory
 * exploration, since it's more certain that reading memory won't be
 * destructive.
 *
 * More to my liking would be a LILO boot command line specification, such
 * as is used by the aha152x driver (and possibly others).  I'll look into
 * it, as I have time...
 *
 *   I get mail occasionally from people who either are using or are
 * considering using a WD7000 with Linux.  There is a variety of
 * nomenclature describing WD7000's.  To the best of my knowledge, the
 * following is a brief summary (from an old WD doc - I don't work for
 * them or anything like that):
 *
 * WD7000-FASST2: This is a WD7000 board with the real-mode SST ROM BIOS
 *        installed.  Last I heard, the BIOS was actually done by Columbia
 *        Data Products.  The BIOS is only used by this driver (and thus
 *        by Linux) to identify the board; none of it can be executed under
 *        Linux.
 *
 * WD7000-ASC: This is the original adapter board, with or without BIOS.
 *        The board uses a WD33C93 or WD33C93A SBIC, which in turn is
 *        controlled by an onboard Z80 processor.  The board interface
 *        visible to the host CPU is defined effectively by the Z80's
 *        firmware, and it is this firmware's revision level that is
 *        determined and reported by this driver.  (The version of the
 *        on-board BIOS is of no interest whatsoever.)  The host CPU has
 *        no access to the SBIC; hence the fact that it is a WD33C93 is
 *        also of no interest to this driver.
 *
 * WD7000-AX:
 * WD7000-MX:
 * WD7000-EX: These are newer versions of the WD7000-ASC.  The -ASC is
 *        largely built from discrete components; these boards use more
 *        integration.  The -AX is an ISA bus board (like the -ASC),
 *        the -MX is an MCA (i.e., PS/2) bus board), and the -EX is an
 *        EISA bus board.
 *
 *  At the time of my documentation, the -?X boards were "future" products,
 *  and were not yet available.  However, I vaguely recall that Thomas
 *  Wuensche had an -AX, so I believe at least it is supported by this
 *  driver.  I have no personal knowledge of either -MX or -EX boards.
 *
 *  P.S. Just recently, I've discovered (directly from WD and Future
 *  Domain) that all but the WD7000-EX have been out of production for
 *  two years now.  FD has production rights to the 7000-EX, and are
 *  producing it under a new name, and with a new BIOS.  If anyone has
 *  one of the FD boards, it would be nice to come up with a signature
 *  for it.
 *                                                           J.B. Jan 1994.
 *
 *
 *  Revisions by Miroslav Zagorac <zaga@fly.cc.fer.hr>
 *
 * -- 08/24/1996. --------------------------------------------------------------
 *    Enhancement for wd7000_detect function has been made, so you don't have
 *    to enter BIOS ROM address in initialisation data (see struct Config).
 *    We cannot detect IRQ, DMA and I/O base address for now, so we have to
 *    enter them as arguments while wd_7000 is detected.  If someone has IRQ,
 *    DMA or an I/O base address set to some other value, he can enter them in
 *    a configuration without any problem.
 *    Also I wrote a function wd7000_setup, so now you can enter WD-7000
 *    definition as kernel arguments, as in lilo.conf:
 *
 *       append="wd7000=IRQ,DMA,IO"
 *
 *   PS: If card BIOS ROM is disabled, function wd7000_detect now will recognize
 *       adapter, unlike the old one.  Anyway, BIOS ROM from WD7000 adapter is
 *       useless for Linux. B^)
 *
 * -- 09/06/1996. --------------------------------------------------------------
 *    Auto detecting of an I/O base address from wd7000_detect function is
 *    removed, some little bugs too...
 *
 *    Thanks to Roger Scott for driver debugging.
 *
 * -- 06/07/1997. --------------------------------------------------------------
 *    Added support for /proc file system (/proc/scsi/wd7000/[0...] files).
 *    Now, the driver can handle hard disks with capacity >1GB.
 *
 * -- 01/15/1998. --------------------------------------------------------------
 *    Added support for BUS_ON and BUS_OFF parameters in config line.
 *    Miscellaneous cleanups.  Syntax of the append line is changed to:
 *
 *       append="wd7000=IRQ,DMA,IO[,BUS_ON[,BUS_OFF]]"
 *
 *    , where BUS_ON and BUS_OFF are time in nanoseconds.
 *
 * -- 03/01/1998. --------------------------------------------------------------
 *    The WD7000 driver now works on kernels' >= 2.1.x
 *
 * -- 06/11/1998. --------------------------------------------------------------
 *    Ugly init_scbs, alloc_scbs and free_scb functions are changed with
 *    scbs_init, scb_alloc and scb_free.  Now, source code is identical on
 *    2.0.xx and 2.1.xx kernels.
 *    WD7000 specific definitions are moved from this file to wd7000.h.
 *
 */
#ifdef MODULE
#  include <linux/module.h>
#endif

#if (LINUX_VERSION_CODE >= 0x020100)
#  include <asm/spinlock.h>
#endif

#include <stdarg.h>
#include <linux/kernel.h>
#include <linux/head.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/malloc.h>
#include <asm/system.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/proc_fs.h>
#include <linux/blk.h>
#include <linux/version.h>
#include <linux/stat.h>
#include "scsi.h"
#include "hosts.h"
#include "sd.h"
#include <scsi/scsicam.h>

#undef WD7000_DEBUG	/* general debug         */
#define WD7000_DEFINES	/* This must be defined! */

#include "wd7000.h"


struct proc_dir_entry proc_scsi_wd7000 =
{
    PROC_SCSI_7000FASST,
    6,
    "wd7000",
    S_IFDIR | S_IRUGO | S_IXUGO,
    2
};

/*
 * (linear) base address for ROM BIOS
 */
static const long wd7000_biosaddr[] = {
    0xc0000, 0xc2000, 0xc4000, 0xc6000, 0xc8000, 0xca000, 0xcc000, 0xce000,
    0xd0000, 0xd2000, 0xd4000, 0xd6000, 0xd8000, 0xda000, 0xdc000, 0xde000
};
#define NUM_ADDRS	(sizeof (wd7000_biosaddr) / sizeof (long))

static const ushort wd7000_iobase[] = {
    0x0300, 0x0308, 0x0310, 0x0318, 0x0320, 0x0328, 0x0330, 0x0338,
    0x0340, 0x0348, 0x0350, 0x0358, 0x0360, 0x0368, 0x0370, 0x0378,
    0x0380, 0x0388, 0x0390, 0x0398, 0x03a0, 0x03a8, 0x03b0, 0x03b8,
    0x03c0, 0x03c8, 0x03d0, 0x03d8, 0x03e0, 0x03e8, 0x03f0, 0x03f8
};
#define NUM_IOPORTS	(sizeof (wd7000_iobase) / sizeof (ushort))

static const short wd7000_irq[] = { 3, 4, 5, 7, 9, 10, 11, 12, 14, 15 };
#define NUM_IRQS	(sizeof (wd7000_irq) / sizeof (short))

static const short wd7000_dma[] = { 5, 6, 7 };
#define NUM_DMAS	(sizeof (wd7000_dma) / sizeof (short))

/*
 * The following is set up by wd7000_detect, and used thereafter by
 * wd7000_intr_handle to map the irq level to the corresponding Adapter.
 * Note that if SA_INTERRUPT is not used, wd7000_intr_handle must be
 * changed to pick up the IRQ level correctly.
 */
static struct Scsi_Host *wd7000_host[IRQS];

/*
 * Add here your configuration...
 */
static Config configs[] =
{
    { 15,  6, 0x350, BUS_ON, BUS_OFF },	/* defaults for single adapter */
    { 11,  5, 0x320, BUS_ON, BUS_OFF },	/* defaults for second adapter */
    {  7,  6, 0x350, BUS_ON, BUS_OFF },	/* My configuration (Zaga)     */
    { -1, -1, 0x0,   BUS_ON, BUS_OFF }	/* Empty slot                  */
};
#define NUM_CONFIGS (sizeof(configs)/sizeof(Config))

static const Signature signatures[] =
{
    {"SSTBIOS", 0x0000d, 7}	/* "SSTBIOS" @ offset 0x0000d */
};
#define NUM_SIGNATURES (sizeof(signatures)/sizeof(Signature))

/*
 *  Driver SCB structure pool.
 *
 *  The SCBs declared here are shared by all host adapters; hence, this
 *  structure is not part of the Adapter structure.
 */
static Scb scbs[MAX_SCBS];


/*
 *  END of data/declarations - code follows.
 */
static void setup_error (char *mesg, int *ints)
{
    if (ints[0] == 3)
        printk ("wd7000_setup: \"wd7000=%d,%d,0x%x\" -> %s\n",
                ints[1], ints[2], ints[3], mesg);
    else if (ints[0] == 4)
        printk ("wd7000_setup: \"wd7000=%d,%d,0x%x,%d\" -> %s\n",
                ints[1], ints[2], ints[3], ints[4], mesg);
    else
        printk ("wd7000_setup: \"wd7000=%d,%d,0x%x,%d,%d\" -> %s\n",
                ints[1], ints[2], ints[3], ints[4], ints[5], mesg);
}


/*
 * Note: You can now set these options from the kernel's "command line".
 * The syntax is:
 *
 *     wd7000=<IRQ>,<DMA>,<IO>[,<BUS_ON>[,<BUS_OFF>]]
 *
 * , where BUS_ON and BUS_OFF are in nanoseconds. BIOS default values
 * are 8000ns for BUS_ON and 1875ns for BUS_OFF.
 *
 * eg:
 *     wd7000=7,6,0x350
 *
 * will configure the driver for a WD-7000 controller
 * using IRQ 15 with a DMA channel 6, at IO base address 0x350.
 */
void wd7000_setup (char *str, int *ints)
{
    static short wd7000_card_num = 0;
    short i, j;

    if (wd7000_card_num >= NUM_CONFIGS) {
	printk ("%s: Too many \"wd7000=\" configurations in "
		"command line!\n", __FUNCTION__);
	return;
    }

    if ((ints[0] < 3) || (ints[0] > 5))
	printk ("%s: Error in command line!  "
		"Usage: wd7000=<IRQ>,<DMA>,<IO>[,<BUS_ON>[,<BUS_OFF>]]\n",
		__FUNCTION__);
    else {
	for (i = 0; i < NUM_IRQS; i++)
	    if (ints[1] == wd7000_irq[i])
		break;

	if (i == NUM_IRQS) {
	    setup_error ("invalid IRQ.", ints);
	    return;
	}
	else
	    configs[wd7000_card_num].irq = ints[1];

	for (i = 0; i < NUM_DMAS; i++)
	    if (ints[2] == wd7000_dma[i])
		break;

	if (i == NUM_DMAS) {
	    setup_error ("invalid DMA channel.", ints);
	    return;
	}
	else
	    configs[wd7000_card_num].dma = ints[2];

	for (i = 0; i < NUM_IOPORTS; i++)
	    if (ints[3] == wd7000_iobase[i])
		break;

	if (i == NUM_IOPORTS) {
	    setup_error ("invalid I/O base address.", ints);
	    return;
	}
	else
	    configs[wd7000_card_num].iobase = ints[3];

	if (ints[0] > 3) {
	    if ((ints[4] < 500) || (ints[4] > 31875)) {
	        setup_error ("BUS_ON value is out of range (500 to 31875 nanoseconds)!",
		             ints);
	        configs[wd7000_card_num].bus_on = BUS_ON;
	    }
	    else
	        configs[wd7000_card_num].bus_on = ints[4] / 125;
	}
	else
	    configs[wd7000_card_num].bus_on = BUS_ON;

	if (ints[0] > 4) {
	    if ((ints[5] < 500) || (ints[5] > 31875)) {
	        setup_error ("BUS_OFF value is out of range (500 to 31875 nanoseconds)!",
		             ints);
	        configs[wd7000_card_num].bus_off = BUS_OFF;
	    }
	    else
	        configs[wd7000_card_num].bus_off = ints[5] / 125;
	}
	else
	    configs[wd7000_card_num].bus_off = BUS_OFF;

	if (wd7000_card_num) {
	    for (i = 0; i < (wd7000_card_num - 1); i++)
		for (j = i + 1; j < wd7000_card_num; j++)
		    if (configs[i].irq == configs[j].irq) {
	                setup_error ("duplicated IRQ!", ints);
			return;
		    }
		    else if (configs[i].dma == configs[j].dma) {
	                setup_error ("duplicated DMA channel!", ints);
			return;
		    }
		    else if (configs[i].iobase == configs[j].iobase) {
	                setup_error ("duplicated I/O base address!", ints);
			return;
		    }
	}

#ifdef WD7000_DEBUG
	printk ("%s: IRQ=%d, DMA=%d, I/O=0x%x, BUS_ON=%dns, BUS_OFF=%dns\n",
	        __FUNCTION__,
		configs[wd7000_card_num].irq,
		configs[wd7000_card_num].dma,
		configs[wd7000_card_num].iobase,
		configs[wd7000_card_num].bus_on * 125,
		configs[wd7000_card_num].bus_off * 125);
#endif

	wd7000_card_num++;
    }
}


/*
 * Since they're used a lot, I've redone the following from the macros
 * formerly in wd7000.h, hopefully to speed them up by getting rid of
 * all the shifting (it may not matter; GCC might have done as well anyway).
 *
 * xany2scsi and xscsi2int were not being used, and are no longer defined.
 * (They were simply 4-byte versions of these routines).
 */
static inline void any2scsi (unchar *scsi, int any)
{
    *scsi++ = ((i_u) any).u[2];
    *scsi++ = ((i_u) any).u[1];
    *scsi   = ((i_u) any).u[0];
}


static inline int scsi2int (unchar *scsi)
{
    i_u result;

    result.i = 0;		/* clears unused bytes */
    result.u[2] = *scsi++;
    result.u[1] = *scsi++;
    result.u[0] = *scsi;

    return (result.i);
}


static inline void wd7000_enable_intr (Adapter *host)
{
    host->control |= INT_EN;
    outb (host->control, host->iobase + ASC_CONTROL);
}


static inline void wd7000_enable_dma (Adapter *host)
{
    host->control |= DMA_EN;
    outb (host->control, host->iobase + ASC_CONTROL);
    set_dma_mode (host->dma, DMA_MODE_CASCADE);
    enable_dma (host->dma);
}


static inline short WAIT (uint port, uint mask, uint allof, uint noneof)
{
    register uint WAITbits;
    register ulong WAITtimeout = jiffies + WAITnexttimeout;

    while (jiffies <= WAITtimeout) {
	WAITbits = inb (port) & mask;

	if (((WAITbits & allof) == allof) && ((WAITbits & noneof) == 0))
	    return (0);
    }

    return (1);
}


static inline void delay (uint how_long)
{
    register ulong time = jiffies + how_long;

    while (jiffies < time);
}


static inline int wd7000_command_out (Adapter *host, unchar *cmd, int len)
{
    if (! WAIT (host->iobase + ASC_STAT, ASC_STATMASK, CMD_RDY, 0)) {
	for ( ; len--; cmd++)
	    do {
		outb (*cmd, host->iobase + ASC_COMMAND);
		WAIT (host->iobase + ASC_STAT, ASC_STATMASK, CMD_RDY, 0);
	    } while (inb (host->iobase + ASC_STAT) & CMD_REJ);

	return (1);
    }

    printk ("%s: WAIT failed (%d)\n", __FUNCTION__, len + 1);

    return (0);
}


static inline void scbs_init (void)
{
    short i;

    for (i = 0; i < MAX_SCBS; i++)
	memset ((void *) &(scbs[i]), 0, sizeof (Scb));
}


static inline Scb *scb_alloc (void)
{
    Scb *scb = NULL;
    ulong flags;
    short i;
#ifdef WD7000_DEBUG
    short free_scbs = 0;
#endif

    save_flags (flags);
    cli ();

    for (i = 0; i < MAX_SCBS; i++)
	if (! scbs[i].used) {
	    scbs[i].used = 1;
	    scb = &(scbs[i]);

	    break;
	}

#ifdef WD7000_DEBUG
    for (i = 0; i < MAX_SCBS; i++)
	free_scbs += scbs[i].used ? 0 : 1;

    printk ("wd7000_%s: allocating scb (0x%08x), %d scbs free\n",
	    __FUNCTION__, (int) scb, free_scbs);
#endif

    restore_flags (flags);

    return (scb);
}


static inline void scb_free (Scb *scb)
{
    short i;
    ulong flags;

    save_flags (flags);
    cli ();

    for (i = 0; i < MAX_SCBS; i++)
	if (&(scbs[i]) == scb) {
	    memset ((void *) &(scbs[i]), 0, sizeof (Scb));

	    break;
	}

    if (i == MAX_SCBS)
	printk ("wd7000_%s: trying to free alien scb (0x%08x)...\n",
		__FUNCTION__, (int) scb);
#ifdef WD7000_DEBUG
    else
	printk ("wd7000_%s: freeing scb (0x%08x)\n", __FUNCTION__, (int) scb);
#endif

    restore_flags (flags);
}


static int mail_out (Adapter *host, Scb *scbptr)
/*
 *  Note: this can also be used for ICBs; just cast to the parm type.
 */
{
    register int i, ogmb;
    ulong flags;
    unchar start_ogmb;
    Mailbox *ogmbs = host->mb.ogmb;
    int *next_ogmb = &(host->next_ogmb);

#ifdef WD7000_DEBUG
    printk ("wd7000_%s: 0x%08x", __FUNCTION__, (int) scbptr);
#endif

    /* We first look for a free outgoing mailbox */
    save_flags (flags);
    cli ();

    ogmb = *next_ogmb;
    for (i = 0; i < OGMB_CNT; i++) {
	if (ogmbs[ogmb].status == 0) {
#ifdef WD7000_DEBUG
	    printk (" using OGMB 0x%x", ogmb);
#endif
	    ogmbs[ogmb].status = 1;
	    any2scsi ((unchar *) ogmbs[ogmb].scbptr, (int) scbptr);

	    *next_ogmb = (ogmb + 1) % OGMB_CNT;
	    break;
	}
	else
	    ogmb = (++ogmb) % OGMB_CNT;
    }

    restore_flags (flags);

#ifdef WD7000_DEBUG
    printk (", scb is 0x%08x", (int) scbptr);
#endif

    if (i >= OGMB_CNT) {
	/*
	 *  Alternatively, we might issue the "interrupt on free OGMB",
	 *  and sleep, but it must be ensured that it isn't the init
	 *  task running.  Instead, this version assumes that the caller
	 *  will be persistent, and try again.  Since it's the adapter
	 *  that marks OGMB's free, waiting even with interrupts off
	 *  should work, since they are freed very quickly in most cases.
	 */
#ifdef WD7000_DEBUG
	printk (", no free OGMBs.\n");
#endif
	return (0);
    }

    wd7000_enable_intr (host);

    start_ogmb = START_OGMB | ogmb;
    wd7000_command_out (host, &start_ogmb, 1);

#ifdef WD7000_DEBUG
    printk (", awaiting interrupt.\n");
#endif

    return (1);
}


int make_code (uint hosterr, uint scsierr)
{
#ifdef WD7000_DEBUG
    int in_error = hosterr;
#endif

    switch ((hosterr >> 8) & 0xff) {
	case 0:  /* Reserved */
                 hosterr = DID_ERROR;
                 break;

	case 1:  /* Command Complete, no errors */
                 hosterr = DID_OK;
                 break;

	case 2:  /* Command complete, error logged in scb status (scsierr) */
                 hosterr = DID_OK;
                 break;

	case 4:  /* Command failed to complete - timeout */
                 hosterr = DID_TIME_OUT;
                 break;

	case 5:  /* Command terminated; Bus reset by external device */
                 hosterr = DID_RESET;
                 break;

	case 6:  /* Unexpected Command Received w/ host as target */
                 hosterr = DID_BAD_TARGET;
                 break;

	case 80: /* Unexpected Reselection */
	case 81: /* Unexpected Selection */
                 hosterr = DID_BAD_INTR;
                 break;

	case 82: /* Abort Command Message  */
                 hosterr = DID_ABORT;
                 break;

	case 83: /* SCSI Bus Software Reset */
	case 84: /* SCSI Bus Hardware Reset */
                 hosterr = DID_RESET;
                 break;

	default: /* Reserved */
                 hosterr = DID_ERROR;
    }

#ifdef WD7000_DEBUG
    if (scsierr || hosterr)
	printk ("\nSCSI command error: SCSI 0x%02x host 0x%04x return %d\n",
		scsierr, in_error, hosterr);
#endif

    return (scsierr | (hosterr << 16));
}


static void wd7000_scsi_done (Scsi_Cmnd *SCpnt)
{
#ifdef WD7000_DEBUG
    printk ("%s: 0x%08x\n", __FUNCTION__, (int) SCpnt);
#endif

    SCpnt->SCp.phase = 0;
}


static inline void wd7000_intr_ack (Adapter *host)
{
    outb (0, host->iobase + ASC_INTR_ACK);
}


void wd7000_intr_handle (int irq, void *dev_id, struct pt_regs *regs)
{
    register int flag, icmb, errstatus, icmb_status;
    register int host_error, scsi_error;
    register Scb *scb;		/* for SCSI commands */
    register IcbAny *icb;	/* for host commands */
    register Scsi_Cmnd *SCpnt;
    Adapter *host = (Adapter *) wd7000_host[irq - IRQ_MIN]->hostdata;	/* This MUST be set!!! */
    Mailbox *icmbs = host->mb.icmb;

    host->int_counter++;

#ifdef WD7000_DEBUG
    printk ("%s: irq = %d, host = 0x%08x\n", __FUNCTION__, irq, (int) host);
#endif

    flag = inb (host->iobase + ASC_INTR_STAT);

#ifdef WD7000_DEBUG
    printk ("%s: intr stat = 0x%02x\n", __FUNCTION__, flag);
#endif

    if (! (inb (host->iobase + ASC_STAT) & INT_IM)) {
	/* NB: these are _very_ possible if IRQ 15 is being used, since
	 * it's the "garbage collector" on the 2nd 8259 PIC.  Specifically,
	 * any interrupt signal into the 8259 which can't be identified
	 * comes out as 7 from the 8259, which is 15 to the host.  Thus, it
	 * is a good thing the WD7000 has an interrupt status port, so we
	 * can sort these out.  Otherwise, electrical noise and other such
	 * problems would be indistinguishable from valid interrupts...
	 */
#ifdef WD7000_DEBUG
	printk ("%s: phantom interrupt...\n", __FUNCTION__);
#endif
	wd7000_intr_ack (host);
	return;
    }

    if (flag & MB_INTR) {
	/* The interrupt is for a mailbox */
	if (! (flag & IMB_INTR)) {
#ifdef WD7000_DEBUG
	    printk ("%s: free outgoing mailbox\n", __FUNCTION__);
#endif
	    /*
	     * If sleep_on() and the "interrupt on free OGMB" command are
	     * used in mail_out(), wake_up() should correspondingly be called
	     * here.  For now, we don't need to do anything special.
	     */
	    wd7000_intr_ack (host);
	    return;
	}
	else {
	    /* The interrupt is for an incoming mailbox */
	    icmb = flag & MB_MASK;
	    icmb_status = icmbs[icmb].status;

	    if (icmb_status & 0x80) {	/* unsolicited - result in ICMB */
#ifdef WD7000_DEBUG
		printk ("%s: unsolicited interrupt 0x%02x\n",
			__FUNCTION__, icmb_status);
#endif
		wd7000_intr_ack (host);
		return;
	    }

	    /* Aaaargh! (Zaga) */
	    scb = (Scb *) bus_to_virt (scsi2int ((unchar *) icmbs[icmb].scbptr));

	    icmbs[icmb].status = 0;
	    if (!(scb->op & ICB_OP_MASK)) {	/* an SCB is done */
		SCpnt = scb->SCpnt;
		if (--(SCpnt->SCp.phase) <= 0) {	/* all scbs are done */
		    host_error = scb->vue | (icmb_status << 8);
		    scsi_error = scb->status;
		    errstatus = make_code (host_error, scsi_error);
		    SCpnt->result = errstatus;

		    scb_free (scb);

		    SCpnt->scsi_done (SCpnt);
		}
	    }
	    else {		/* an ICB is done */
		icb = (IcbAny *) scb;
		icb->status = icmb_status;
		icb->phase = 0;
	    }
	}			/* incoming mailbox */
    }

    wd7000_intr_ack (host);

#ifdef WD7000_DEBUG
    printk ("%s: return from interrupt handler\n", __FUNCTION__);
#endif
}


void do_wd7000_intr_handle (int irq, void *dev_id, struct pt_regs *regs)
{
#if (LINUX_VERSION_CODE >= 0x020100)
    ulong flags;

    spin_lock_irqsave (&io_request_lock, flags);
#endif

    wd7000_intr_handle (irq, dev_id, regs);

#if (LINUX_VERSION_CODE >= 0x020100)
    spin_unlock_irqrestore (&io_request_lock, flags);
#endif
}


int wd7000_queuecommand (Scsi_Cmnd *SCpnt, void (*done) (Scsi_Cmnd *))
{
    register Scb *scb;
    register Sgb *sgb;
    register Adapter *host = (Adapter *) SCpnt->host->hostdata;

    if ((scb = scb_alloc ()) == NULL) {
	printk ("%s: Cannot allocate SCB!\n", __FUNCTION__);
	return (0);
    }

    SCpnt->scsi_done = done;
    SCpnt->SCp.phase = 1;
    SCpnt->host_scribble = (unchar *) scb;
    scb->idlun = ((SCpnt->target << 5) & 0xe0) | (SCpnt->lun & 7);
    scb->direc = 0x40;		/* Disable direction check */
    scb->SCpnt = SCpnt;		/* so we can find stuff later */
    scb->host = host;
    memcpy (scb->cdb, SCpnt->cmnd, SCpnt->cmd_len);

    if (SCpnt->use_sg) {
	struct scatterlist *sg = (struct scatterlist *) SCpnt->request_buffer;
	uint i;

	if (SCpnt->host->sg_tablesize == SG_NONE)
	    panic ("%s: scatter/gather not supported.\n", __FUNCTION__);
#ifdef WD7000_DEBUG
	else
	    printk ("Using scatter/gather with %d elements.\n", SCpnt->use_sg);
#endif

	sgb = scb->sgb;
	scb->op = 1;
	any2scsi (scb->dataptr, (int) sgb);
	any2scsi (scb->maxlen, SCpnt->use_sg * sizeof (Sgb));

	for (i = 0; i < SCpnt->use_sg; i++) {
	    any2scsi (sgb[i].ptr, (int) sg[i].address);
	    any2scsi (sgb[i].len, sg[i].length);
	}
    }
    else {
	scb->op = 0;
	any2scsi (scb->dataptr, (int) SCpnt->request_buffer);
	any2scsi (scb->maxlen, SCpnt->request_bufflen);
    }

    while (! mail_out (host, scb));	/* keep trying */

    return (1);
}


int wd7000_command (Scsi_Cmnd *SCpnt)
{
    if (! wd7000_queuecommand (SCpnt, wd7000_scsi_done))
	return (-1);

    while (SCpnt->SCp.phase > 0)
	barrier ();		/* phase counts scbs down to 0 */

    return (SCpnt->result);
}


int wd7000_diagnostics (Adapter *host, int code)
{
    static IcbDiag icb = { ICB_OP_DIAGNOSTICS };
    static unchar buf[256];
    ulong timeout;

    /*
     * This routine is only called at init, so there should be OGMBs
     * available.  I'm assuming so here.  If this is going to
     * fail, I can just let the timeout catch the failure.
     */
    icb.type = code;
    any2scsi (icb.len, sizeof (buf));
    any2scsi (icb.ptr, (int) &buf);
    icb.phase = 1;

    mail_out (host, (Scb *) &icb);

    /*
     * Wait up to 2 seconds for completion.
     */
    for (timeout = jiffies + WAITnexttimeout; icb.phase && (jiffies < timeout); )
	barrier ();

    if (icb.phase) {
	printk ("%s: timed out.\n", __FUNCTION__);
	return (0);
    }

    if (make_code (icb.vue | (icb.status << 8), 0)) {
	printk ("%s: failed (0x%02x,0x%02x)\n", __FUNCTION__, icb.vue, icb.status);
	return (0);
    }

    return (1);
}


int wd7000_init (Adapter *host)
{
    InitCmd init_cmd =
    {
	INITIALIZATION,
 	7,
	host->bus_on,
	host->bus_off,
	0,
	{ 0, 0, 0 },
	OGMB_CNT,
	ICMB_CNT
    };
    int diag;

    /*
     *  Reset the adapter - only.  The SCSI bus was initialized at power-up,
     *  and we need to do this just so we control the mailboxes, etc.
     */
    outb (ASC_RES, host->iobase + ASC_CONTROL);
    delay (1);			/* reset pulse: this is 10ms, only need 25us */
    outb (0, host->iobase + ASC_CONTROL);
    host->control = 0;		/* this must always shadow ASC_CONTROL */

    if (WAIT (host->iobase + ASC_STAT, ASC_STATMASK, CMD_RDY, 0)) {
	printk ("%s: WAIT timed out.\n", __FUNCTION__);
	return (0);		/* 0 = not ok */
    }

    if ((diag = inb (host->iobase + ASC_INTR_STAT)) != 1) {
	printk ("%s: ", __FUNCTION__);

	switch (diag) {
	    case 2:  printk ("RAM failure.\n");
		     break;

	    case 3:  printk ("FIFO R/W failed\n");
		     break;

	    case 4:  printk ("SBIC register R/W failed\n");
		     break;

	    case 5:  printk ("Initialization D-FF failed.\n");
		     break;

	    case 6:  printk ("Host IRQ D-FF failed.\n");
		     break;

	    case 7:  printk ("ROM checksum error.\n");
		     break;

	    default: printk ("diagnostic code 0x%02Xh received.\n", diag);
	}

	return (0);
    }

    /* Clear mailboxes */
    memset (&(host->mb), 0, sizeof (host->mb));

    /* Execute init command */
    any2scsi ((unchar *) &(init_cmd.mailboxes), (int) &(host->mb));

    if (! wd7000_command_out (host, (unchar *) &init_cmd, sizeof (init_cmd))) {
	printk ("%s: adapter initialization failed.\n", __FUNCTION__);
	return (0);
    }

    if (WAIT (host->iobase + ASC_STAT, ASC_STATMASK, ASC_INIT, 0)) {
	printk ("%s: WAIT timed out.\n", __FUNCTION__);
	return (0);
    }

    if (request_irq (host->irq, do_wd7000_intr_handle, SA_INTERRUPT, "wd7000", NULL)) {
	printk ("%s: can't get IRQ %d.\n", __FUNCTION__, host->irq);
	return (0);
    }

    if (request_dma (host->dma, "wd7000")) {
	printk ("%s: can't get DMA channel %d.\n", __FUNCTION__, host->dma);
	free_irq (host->irq, NULL);
	return (0);
    }

    wd7000_enable_dma (host);
    wd7000_enable_intr (host);

    if (! wd7000_diagnostics (host, ICB_DIAG_FULL)) {
	free_dma (host->dma);
	free_irq (host->irq, NULL);
	return (0);
    }

    return (1);
}


void wd7000_revision (Adapter *host)
{
    static IcbRevLvl icb = { ICB_OP_GET_REVISION };

    /*
     * Like diagnostics, this is only done at init time, in fact, from
     * wd7000_detect, so there should be OGMBs available.  If it fails,
     * the only damage will be that the revision will show up as 0.0,
     * which in turn means that scatter/gather will be disabled.
     */
    icb.phase = 1;
    mail_out (host, (Scb *) &icb);

    while (icb.phase)
	barrier ();		/* wait for completion */

    host->rev1 = icb.primary;
    host->rev2 = icb.secondary;
}


#undef SPRINTF
#define SPRINTF(args...)	{ if (pos < (buffer + length)) pos += sprintf (pos, ## args); }

int wd7000_set_info (char *buffer, int length, struct Scsi_Host *host)
{
    ulong flags;

    save_flags (flags);
    cli ();

#ifdef WD7000_DEBUG
    printk ("Buffer = <%.*s>, length = %d\n", length, buffer, length);
#endif

    /*
     * Currently this is a no-op
     */
    printk ("Sorry, this function is currently out of order...\n");

    restore_flags (flags);

    return (length);
}


int wd7000_proc_info (char *buffer, char **start, off_t offset, int length, int hostno, int inout)
{
    struct Scsi_Host *host = NULL;
    Scsi_Device *scd;
    Adapter *adapter;
    ulong flags;
    char *pos = buffer;
    short i;

#ifdef WD7000_DEBUG
    Mailbox *ogmbs, *icmbs;
    short count;
#endif

    /*
     * Find the specified host board.
     */
    for (i = 0; i < IRQS; i++)
	if (wd7000_host[i] && (wd7000_host[i]->host_no == hostno)) {
	    host = wd7000_host[i];

	    break;
	}

    /*
     * Host not found!
     */
    if (! host)
	return (-ESRCH);

    /*
     * Has data been written to the file ?
     */
    if (inout)
	return (wd7000_set_info (buffer, length, host));

    adapter = (Adapter *) host->hostdata;

    save_flags (flags);
    cli ();

    SPRINTF ("Host scsi%d: Western Digital WD-7000 (rev %d.%d)\n", hostno, adapter->rev1, adapter->rev2);
    SPRINTF ("  IO base:      0x%x\n", adapter->iobase);
    SPRINTF ("  IRQ:          %d\n", adapter->irq);
    SPRINTF ("  DMA channel:  %d\n", adapter->dma);
    SPRINTF ("  Interrupts:   %d\n", adapter->int_counter);
    SPRINTF ("  BUS_ON time:  %d nanoseconds\n", adapter->bus_on * 125);
    SPRINTF ("  BUS_OFF time: %d nanoseconds\n", adapter->bus_off * 125);

#ifdef WD7000_DEBUG
    ogmbs = adapter->mb.ogmb;
    icmbs = adapter->mb.icmb;

    SPRINTF ("\nControl port value: 0x%x\n", adapter->control);
    SPRINTF ("Incoming mailbox:\n");
    SPRINTF ("  size: %d\n", ICMB_CNT);
    SPRINTF ("  queued messages: ");

    for (i = count = 0; i < ICMB_CNT; i++)
	if (icmbs[i].status) {
	    count++;
	    SPRINTF ("0x%x ", i);
	}

    SPRINTF (count ? "\n" : "none\n");

    SPRINTF ("Outgoing mailbox:\n");
    SPRINTF ("  size: %d\n", OGMB_CNT);
    SPRINTF ("  next message: 0x%x\n", adapter->next_ogmb);
    SPRINTF ("  queued messages: ");

    for (i = count = 0; i < OGMB_CNT; i++)
	if (ogmbs[i].status) {
	    count++;
	    SPRINTF ("0x%x ", i);
	}

    SPRINTF (count ? "\n" : "none\n");
#endif

    /*
     * Display driver information for each device attached to the board.
     */
#if (LINUX_VERSION_CODE >= 0x020100)
    scd = host->host_queue;
#else
    scd = scsi_devices;
#endif
   
    SPRINTF ("\nAttached devices: %s\n", scd ? "" : "none");

    for ( ; scd; scd = scd->next)
	if (scd->host->host_no == hostno) {
	    SPRINTF ("  [Channel: %02d, Id: %02d, Lun: %02d]  ",
		     scd->channel, scd->id, scd->lun);
	    SPRINTF ("%s ", (scd->type < MAX_SCSI_DEVICE_CODE) ?
		     scsi_device_types[(short) scd->type] : "Unknown device");

	    for (i = 0; (i < 8) && (scd->vendor[i] >= 0x20); i++)
		SPRINTF ("%c", scd->vendor[i]);
	    SPRINTF (" ");

	    for (i = 0; (i < 16) && (scd->model[i] >= 0x20); i++)
		SPRINTF ("%c", scd->model[i]);
	    SPRINTF ("\n");
	}

    SPRINTF ("\n");

    restore_flags (flags);

    /*
     * Calculate start of next buffer, and return value.
     */
    *start = buffer + offset;

    if ((pos - buffer) < offset)
	return (0);
    else if ((pos - buffer - offset) < length)
	return (pos - buffer - offset);
    else
	return (length);
}


/*
 *  Returns the number of adapters this driver is supporting.
 *
 *  The source for hosts.c says to wait to call scsi_register until 100%
 *  sure about an adapter.  We need to do it a little sooner here; we
 *  need the storage set up by scsi_register before wd7000_init, and
 *  changing the location of an Adapter structure is more trouble than
 *  calling scsi_unregister.
 *
 */
int wd7000_detect (Scsi_Host_Template *tpnt)
{
    short present = 0, biosaddr_ptr, sig_ptr, i, pass;
    short biosptr[NUM_CONFIGS];
    uint iobase;
    Adapter *host = NULL;
    struct Scsi_Host *sh;

#ifdef WD7000_DEBUG
    printk ("%s: started\n", __FUNCTION__);
#endif

    /*
     * Set up SCB free list, which is shared by all adapters
     */
    scbs_init ();

    for (i = 0; i < IRQS; wd7000_host[i++] = NULL);
    for (i = 0; i < NUM_CONFIGS; biosptr[i++] = -1);

    tpnt->proc_dir = &proc_scsi_wd7000;
    tpnt->proc_info = &wd7000_proc_info;

    for (pass = 0; pass < NUM_CONFIGS; pass++) {
	short bios_match = 1;

#ifdef WD7000_DEBUG
	printk ("%s: pass %d\n", __FUNCTION__, pass + 1);
#endif

	/*
	 * First, search for BIOS SIGNATURE...
	 */
	for (biosaddr_ptr = 0; bios_match && (biosaddr_ptr < NUM_ADDRS); biosaddr_ptr++)
	    for (sig_ptr = 0; bios_match && (sig_ptr < NUM_SIGNATURES); sig_ptr++) {
		for (i = 0; i < pass; i++)
		    if (biosptr[i] == biosaddr_ptr)
			break;

		if (i == pass) {
#if (LINUX_VERSION_CODE >= 0x020100)
		    char *biosaddr = (char *) ioremap (wd7000_biosaddr[biosaddr_ptr] +
							signatures[sig_ptr].ofs,
							signatures[sig_ptr].len);
#else
		    char *biosaddr = (char *) (wd7000_biosaddr[biosaddr_ptr] +
						signatures[sig_ptr].ofs);
#endif
		    bios_match = memcmp (biosaddr, signatures[sig_ptr].sig,
						signatures[sig_ptr].len);

#if (LINUX_VERSION_CODE >= 0x020100)
		    iounmap (biosaddr);
#else
#endif
		    if (! bios_match) {
			/*
			 * BIOS SIGNATURE has been found.
			 */
			biosptr[pass] = biosaddr_ptr;
#ifdef WD7000_DEBUG
			printk ("WD-7000 SST BIOS detected at 0x%lx: checking...\n",
				wd7000_biosaddr[biosaddr_ptr]);
#endif
		    }
		}
	    }

#ifdef WD7000_DEBUG
	if (bios_match)
	    printk ("WD-7000 SST BIOS not detected...\n");
#endif

	if (configs[pass].irq < 0)
	    continue;

	iobase = configs[pass].iobase;

#ifdef WD7000_DEBUG
	printk ("%s: check IO 0x%x region...\n", __FUNCTION__, iobase);
#endif

	if (! check_region (iobase, 4)) {
#ifdef WD7000_DEBUG
	    printk ("%s: ASC reset (IO 0x%x) ...", __FUNCTION__, iobase);
#endif
	    /*
	     * ASC reset...
	     */
	    outb (ASC_RES, iobase + ASC_CONTROL);
	    delay (1);
	    outb (0, iobase + ASC_CONTROL);

	    if (WAIT (iobase + ASC_STAT, ASC_STATMASK, CMD_RDY, 0))
#ifdef WD7000_DEBUG
	    {
		printk ("failed!\n");
		continue;
	    }
	    else
		printk ("ok!\n");
#else
		continue;
#endif

	    if (inb (iobase + ASC_INTR_STAT) == 1) {
		/*
		 *  We register here, to get a pointer to the extra space,
		 *  which we'll use as the Adapter structure (host) for
		 *  this adapter.  It is located just after the registered
		 *  Scsi_Host structure (sh), and is located by the empty
		 *  array hostdata.
		 */
		sh = scsi_register (tpnt, sizeof (Adapter));
		host = (Adapter *) sh->hostdata;

#ifdef WD7000_DEBUG
		printk ("%s: adapter allocated at 0x%x\n", __FUNCTION__, (int) host);
#endif

		memset (host, 0, sizeof (Adapter));

		host->irq = configs[pass].irq;
		host->dma = configs[pass].dma;
		host->iobase = iobase;
		host->int_counter = 0;
		host->bus_on = configs[pass].bus_on;
		host->bus_off = configs[pass].bus_off;
		host->sh = wd7000_host[host->irq - IRQ_MIN] = sh;

#ifdef WD7000_DEBUG
		printk ("%s: Trying to init WD-7000 card at IO 0x%x, IRQ %d, DMA %d...\n",
			__FUNCTION__, host->iobase, host->irq, host->dma);
#endif

		if (! wd7000_init (host)) {	/* Initialization failed */
		    scsi_unregister (sh);
		    continue;
		}

		/*
		 *  OK from here - we'll use this adapter/configuration.
		 */
		wd7000_revision (host);		/* important for scatter/gather */

		/*
		 * Register our ports.
		 */
		request_region (host->iobase, 4, "wd7000");

		/*
		 *  For boards before rev 6.0, scatter/gather isn't supported.
		 */
		if (host->rev1 < 6)
		    sh->sg_tablesize = SG_NONE;

		present++;	/* count it */

		printk ("Western Digital WD-7000 (rev %d.%d) ",
			host->rev1, host->rev2);
		printk ("using IO 0x%x, IRQ %d, DMA %d.\n",
			host->iobase, host->irq, host->dma);
                printk ("  BUS_ON time: %dns, BUS_OFF time: %dns\n",
                        host->bus_on * 125, host->bus_off * 125);
	    }
	}

#ifdef WD7000_DEBUG
	else
	    printk ("%s: IO 0x%x region is already allocated!\n", __FUNCTION__, iobase);
#endif

    }

    if (! present)
	printk ("Failed initialization of WD-7000 SCSI card!\n");

    return (present);
}


/*
 *  I have absolutely NO idea how to do an abort with the WD7000...
 */
int wd7000_abort (Scsi_Cmnd *SCpnt)
{
    Adapter *host = (Adapter *) SCpnt->host->hostdata;

    if (inb (host->iobase + ASC_STAT) & INT_IM) {
	printk ("%s: lost interrupt\n", __FUNCTION__);
	wd7000_intr_handle (host->irq, NULL, NULL);

	return (SCSI_ABORT_SUCCESS);
    }

    return (SCSI_ABORT_SNOOZE);
}


/*
 *  I also have no idea how to do a reset...
 */
int wd7000_reset (Scsi_Cmnd *SCpnt, uint flags)
{
    return (SCSI_RESET_PUNT);
}


/*
 *  This was borrowed directly from aha1542.c. (Zaga)
 */
int wd7000_biosparam (Disk *disk, kdev_t dev, int *ip)
{
#ifdef WD7000_DEBUG
    printk ("%s: dev=%s, size=%d, ", __FUNCTION__, kdevname (dev), disk->capacity);
#endif

    /*
     * try default translation
     */
    ip[0] = 64;
    ip[1] = 32;
    ip[2] = disk->capacity / (64 * 32);

    /*
     * for disks >1GB do some guessing
     */
    if (ip[2] >= 1024) {
	int info[3];

	/*
	 * try to figure out the geometry from the partition table
	 */
	if ((scsicam_bios_param (disk, dev, info) < 0) ||
	    !(((info[0] == 64) && (info[1] == 32)) ||
	      ((info[0] == 255) && (info[1] == 63)))) {
	    printk ("%s: unable to verify geometry for disk with >1GB.\n"
		    "                  using extended translation.\n",
		    __FUNCTION__);

	    ip[0] = 255;
	    ip[1] = 63;
	    ip[2] = disk->capacity / (255 * 63);
	}
	else {
	    ip[0] = info[0];
	    ip[1] = info[1];
	    ip[2] = info[2];

	    if (info[0] == 255)
		printk ("%s: current partition table is using extended translation.\n",
			__FUNCTION__);
	}
    }

#ifdef WD7000_DEBUG
    printk ("bios geometry: head=%d, sec=%d, cyl=%d\n", ip[0], ip[1], ip[2]);
    printk ("WARNING: check, if the bios geometry is correct.\n");
#endif

    return (0);
}

#ifdef MODULE
/* Eventually this will go into an include file, but this will be later */
Scsi_Host_Template driver_template = WD7000;

#include "scsi_module.c"
#endif
