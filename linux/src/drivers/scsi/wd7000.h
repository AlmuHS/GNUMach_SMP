/* $Id: wd7000.h,v 1.1 1999/04/26 05:55:19 tb Exp $
 *
 * Header file for the WD-7000 driver for Linux
 *
 * John Boyd <boyd@cis.ohio-state.edu>  Jan 1994:
 * This file has been reduced to only the definitions needed for the
 * WD7000 host structure.
 *
 * Revision by Miroslav Zagorac <zaga@fly.cc.fer.hr>  Jun 1997.
 */
#ifndef _WD7000_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kdev_t.h>

#ifndef NULL
#define NULL 0L
#endif

/*
 *  In this version, sg_tablesize now defaults to WD7000_SG, and will
 *  be set to SG_NONE for older boards.  This is the reverse of the
 *  previous default, and was changed so that the driver-level
 *  Scsi_Host_Template would reflect the driver's support for scatter/
 *  gather.
 *
 *  Also, it has been reported that boards at Revision 6 support scatter/
 *  gather, so the new definition of an "older" board has been changed
 *  accordingly.
 */
#define WD7000_Q    16
#define WD7000_SG   16

#ifdef WD7000_DEFINES
/*
 *  Mailbox structure sizes.
 *  I prefer to keep the number of ICMBs much larger than the number of
 *  OGMBs.  OGMBs are used very quickly by the driver to start one or
 *  more commands, while ICMBs are used by the host adapter per command.
 */
#define OGMB_CNT	16
#define ICMB_CNT	32

/*
 *  Scb's are shared by all active adapters.  If you'd rather conserve
 *  memory, use a smaller number (> 0, of course) - things will should
 *  still work OK.
 */
#define MAX_SCBS	(4 * WD7000_Q)

/*
 *  WD7000-specific mailbox structure
 */
typedef volatile struct {
    unchar status;
    unchar scbptr[3];		/* SCSI-style - MSB first (big endian) */
} Mailbox;

/*
 *  This structure should contain all per-adapter global data.  I.e., any
 *  new global per-adapter data should put in here.
 */
typedef struct {
    struct Scsi_Host *sh;	/* Pointer to Scsi_Host structure    */
    int iobase;			/* This adapter's I/O base address   */
    int irq;			/* This adapter's IRQ level          */
    int dma;			/* This adapter's DMA channel        */
    int int_counter;		/* This adapter's interrupt counter  */
    int bus_on;			/* This adapter's BUS_ON time        */
    int bus_off;		/* This adapter's BUS_OFF time       */
    struct {			/* This adapter's mailboxes          */
	Mailbox ogmb[OGMB_CNT];	/* Outgoing mailboxes                */
	Mailbox icmb[ICMB_CNT];	/* Incoming mailboxes                */
    } mb;
    int next_ogmb;		/* to reduce contention at mailboxes */
    unchar control;		/* shadows CONTROL port value        */
    unchar rev1;		/* filled in by wd7000_revision      */
    unchar rev2;
} Adapter;


/*
 * possible irq range
 */
#define IRQ_MIN		3
#define IRQ_MAX		15
#define IRQS		(IRQ_MAX - IRQ_MIN + 1)

#define BUS_ON		64	/* x 125ns = 8000ns (BIOS default) */
#define BUS_OFF		15	/* x 125ns = 1875ns (BIOS default) */

/*
 *  Standard Adapter Configurations - used by wd7000_detect
 */
typedef struct {
    short irq;		/* IRQ level                                  */
    short dma;		/* DMA channel                                */
    uint iobase;	/* I/O base address                           */
    short bus_on;	/* Time that WD7000 spends on the AT-bus when */
			/* transferring data. BIOS default is 8000ns. */
    short bus_off;	/* Time that WD7000 spends OFF THE BUS after  */
			/* while it is transferring data.             */
			/* BIOS default is 1875ns                     */
} Config;


/*
 *  The following list defines strings to look for in the BIOS that identify
 *  it as the WD7000-FASST2 SST BIOS.  I suspect that something should be
 *  added for the Future Domain version.
 */
typedef struct {
    const char *sig;		/* String to look for            */
    ulong ofs;			/* offset from BIOS base address */
    uint len;			/* length of string              */
} Signature;

/*
 *  I/O Port Offsets and Bit Definitions
 *  4 addresses are used.  Those not defined here are reserved.
 */
#define ASC_STAT	0	/* Status,  Read          */
#define ASC_COMMAND	0	/* Command, Write         */
#define ASC_INTR_STAT	1	/* Interrupt Status, Read */
#define ASC_INTR_ACK	1	/* Acknowledge, Write     */
#define ASC_CONTROL	2	/* Control, Write         */

/*
 * ASC Status Port
 */
#define INT_IM		0x80	/* Interrupt Image Flag           */
#define CMD_RDY		0x40	/* Command Port Ready             */
#define CMD_REJ		0x20	/* Command Port Byte Rejected     */
#define ASC_INIT        0x10	/* ASC Initialized Flag           */
#define ASC_STATMASK    0xf0	/* The lower 4 Bytes are reserved */

/*
 * COMMAND opcodes
 *
 *  Unfortunately, I have no idea how to properly use some of these commands,
 *  as the OEM manual does not make it clear.  I have not been able to use
 *  enable/disable unsolicited interrupts or the reset commands with any
 *  discernible effect whatsoever.  I think they may be related to certain
 *  ICB commands, but again, the OEM manual doesn't make that clear.
 */
#define NO_OP			0	/* NO-OP toggles CMD_RDY bit in ASC_STAT  */
#define INITIALIZATION		1	/* initialization (10 bytes)              */
#define DISABLE_UNS_INTR	2	/* disable unsolicited interrupts         */
#define ENABLE_UNS_INTR		3	/* enable unsolicited interrupts          */
#define INTR_ON_FREE_OGMB	4	/* interrupt on free OGMB                 */
#define SOFT_RESET		5	/* SCSI bus soft reset                    */
#define HARD_RESET_ACK		6	/* SCSI bus hard reset acknowledge        */
#define START_OGMB		0x80	/* start command in OGMB (n)              */
#define SCAN_OGMBS		0xc0	/* start multiple commands, signature (n) */
					/*    where (n) = lower 6 bits            */
/*
 * For INITIALIZATION:
 */
typedef struct {
    unchar op;			/* command opcode (= 1)                    */
    unchar ID;			/* Adapter's SCSI ID                       */
    unchar bus_on;		/* Bus on time, x 125ns (see below)        */
    unchar bus_off;		/* Bus off time, ""         ""             */
    unchar rsvd;		/* Reserved                                */
    unchar mailboxes[3];	/* Address of Mailboxes, MSB first         */
    unchar ogmbs;		/* Number of outgoing MBs, max 64, 0,1 = 1 */
    unchar icmbs;		/* Number of incoming MBs,   ""       ""   */
} InitCmd;

/*
 * Interrupt Status Port - also returns diagnostic codes at ASC reset
 *
 * if msb is zero, the lower bits are diagnostic status
 * Diagnostics:
 * 01   No diagnostic error occurred
 * 02   RAM failure
 * 03   FIFO R/W failed
 * 04   SBIC register read/write failed
 * 05   Initialization D-FF failed
 * 06   Host IRQ D-FF failed
 * 07   ROM checksum error
 * Interrupt status (bitwise):
 * 10NNNNNN   outgoing mailbox NNNNNN is free
 * 11NNNNNN   incoming mailbox NNNNNN needs service
 */
#define MB_INTR		0xC0	/* Mailbox Service possible/required */
#define IMB_INTR	0x40	/* 1 Incoming / 0 Outgoing           */
#define MB_MASK		0x3f	/* mask for mailbox number           */

/*
 * CONTROL port bits
 */
#define INT_EN		0x08	/* Interrupt Enable */
#define DMA_EN		0x04	/* DMA Enable       */
#define SCSI_RES	0x02	/* SCSI Reset       */
#define ASC_RES		0x01	/* ASC Reset        */

/*
 * Driver data structures:
 *   - mb and scbs are required for interfacing with the host adapter.
 *     An SCB has extra fields not visible to the adapter; mb's
 *     _cannot_ do this, since the adapter assumes they are contiguous in
 *     memory, 4 bytes each, with ICMBs following OGMBs, and uses this fact
 *     to access them.
 *   - An icb is for host-only (non-SCSI) commands.  ICBs are 16 bytes each;
 *     the additional bytes are used only by the driver.
 *   - For now, a pool of SCBs are kept in global storage by this driver,
 *     and are allocated and freed as needed.
 *
 *  The 7000-FASST2 marks OGMBs empty as soon as it has _started_ a command,
 *  not when it has finished.  Since the SCB must be around for completion,
 *  problems arise when SCBs correspond to OGMBs, which may be reallocated
 *  earlier (or delayed unnecessarily until a command completes).
 *  Mailboxes are used as transient data structures, simply for
 *  carrying SCB addresses to/from the 7000-FASST2.
 *
 *  Note also since SCBs are not "permanently" associated with mailboxes,
 *  there is no need to keep a global list of Scsi_Cmnd pointers indexed
 *  by OGMB.   Again, SCBs reference their Scsi_Cmnds directly, so mailbox
 *  indices need not be involved.
 */

/*
 *  WD7000-specific scatter/gather element structure
 */
typedef struct {
    unchar len[3];
    unchar ptr[3];		/* Also SCSI-style - MSB first */
} Sgb;

typedef struct {		/* Command Control Block 5.4.1               */
    unchar op;			/* Command Control Block Operation Code      */
    unchar idlun;		/* op=0,2:Target Id, op=1:Initiator Id       */
				/* Outbound data transfer, length is checked */
				/* Inbound data transfer, length is checked  */
				/* Logical Unit Number                       */
    unchar cdb[12];		/* SCSI Command Block                        */
    volatile unchar status;	/* SCSI Return Status                        */
    volatile unchar vue;	/* Vendor Unique Error Code                  */
    unchar maxlen[3];		/* Maximum Data Transfer Length              */
    unchar dataptr[3];		/* SCSI Data Block Pointer                   */
    unchar linkptr[3];		/* Next Command Link Pointer                 */
    unchar direc;		/* Transfer Direction                        */
    unchar reserved2[6];	/* SCSI Command Descriptor Block             */
				/* end of hardware SCB                       */
    Scsi_Cmnd *SCpnt;		/* Scsi_Cmnd using this SCB                  */
    Sgb sgb[WD7000_SG];		/* Scatter/gather list for this SCB          */
    Adapter *host;		/* host adapter                              */
    unchar used;		/* flag                                      */
} Scb;

/*
 *  This driver is written to allow host-only commands to be executed.
 *  These use a 16-byte block called an ICB.  The format is extended by the
 *  driver to 18 bytes, to support the status returned in the ICMB and
 *  an execution phase code.
 *
 *  There are other formats besides these; these are the ones I've tried
 *  to use.  Formats for some of the defined ICB opcodes are not defined
 *  (notably, get/set unsolicited interrupt status) in my copy of the OEM
 *  manual, and others are ambiguous/hard to follow.
 */
#define ICB_OP_MASK		0x80	/* distinguishes scbs from icbs        */
#define ICB_OP_OPEN_RBUF	0x80	/* open receive buffer                 */
#define ICB_OP_RECV_CMD		0x81	/* receive command from initiator      */
#define ICB_OP_RECV_DATA	0x82	/* receive data from initiator         */
#define ICB_OP_RECV_SDATA	0x83	/* receive data with status from init. */
#define ICB_OP_SEND_DATA	0x84	/* send data with status to initiator  */
#define ICB_OP_SEND_STAT	0x86	/* send command status to initiator    */
					/* 0x87 is reserved                    */
#define ICB_OP_READ_INIT	0x88	/* read initialization bytes           */
#define ICB_OP_READ_ID		0x89	/* read adapter's SCSI ID              */
#define ICB_OP_SET_UMASK	0x8A	/* set unsolicited interrupt mask      */
#define ICB_OP_GET_UMASK	0x8B	/* read unsolicited interrupt mask     */
#define ICB_OP_GET_REVISION	0x8C	/* read firmware revision level        */
#define ICB_OP_DIAGNOSTICS	0x8D	/* execute diagnostics                 */
#define ICB_OP_SET_EPARMS	0x8E	/* set execution parameters            */
#define ICB_OP_GET_EPARMS	0x8F	/* read execution parameters           */

typedef struct {
    unchar op;
    unchar IDlun;		/* Initiator SCSI ID/lun     */
    unchar len[3];		/* command buffer length     */
    unchar ptr[3];		/* command buffer address    */
    unchar rsvd[7];		/* reserved                  */
    volatile unchar vue;	/* vendor-unique error code  */
    volatile unchar status;	/* returned (icmb) status    */
    volatile unchar phase;	/* used by interrupt handler */
} IcbRecvCmd;

typedef struct {
    unchar op;
    unchar IDlun;		/* Target SCSI ID/lun                  */
    unchar stat;		/* (outgoing) completion status byte 1 */
    unchar rsvd[12];		/* reserved                            */
    volatile unchar vue;	/* vendor-unique error code            */
    volatile unchar status;	/* returned (icmb) status              */
    volatile unchar phase;	/* used by interrupt handler           */
} IcbSendStat;

typedef struct {
    unchar op;
    volatile unchar primary;	/* primary revision level (returned)   */
    volatile unchar secondary;	/* secondary revision level (returned) */
    unchar rsvd[12];		/* reserved                            */
    volatile unchar vue;	/* vendor-unique error code            */
    volatile unchar status;	/* returned (icmb) status              */
    volatile unchar phase;	/* used by interrupt handler           */
} IcbRevLvl;

typedef struct {	/* I'm totally guessing here */
    unchar op;
    volatile unchar mask[14];	/* mask bits                 */
#if 0
    unchar rsvd[12];		/* reserved                  */
#endif
    volatile unchar vue;	/* vendor-unique error code  */
    volatile unchar status;	/* returned (icmb) status    */
    volatile unchar phase;	/* used by interrupt handler */
} IcbUnsMask;

typedef struct {
    unchar op;
    unchar type;		/* diagnostics type code (0-3) */
    unchar len[3];		/* buffer length               */
    unchar ptr[3];		/* buffer address              */
    unchar rsvd[7];		/* reserved                    */
    volatile unchar vue;	/* vendor-unique error code    */
    volatile unchar status;	/* returned (icmb) status      */
    volatile unchar phase;	/* used by interrupt handler   */
} IcbDiag;

#define ICB_DIAG_POWERUP	0	/* Power-up diags only       */
#define ICB_DIAG_WALKING	1	/* walking 1's pattern       */
#define ICB_DIAG_DMA		2	/* DMA - system memory diags */
#define ICB_DIAG_FULL		3	/* do both 1 & 2             */

typedef struct {
    unchar op;
    unchar rsvd1;		/* reserved                  */
    unchar len[3];		/* parms buffer length       */
    unchar ptr[3];		/* parms buffer address      */
    unchar idx[2];		/* index (MSB-LSB)           */
    unchar rsvd2[5];		/* reserved                  */
    volatile unchar vue;	/* vendor-unique error code  */
    volatile unchar status;	/* returned (icmb) status    */
    volatile unchar phase;	/* used by interrupt handler */
} IcbParms;

typedef struct {
    unchar op;
    unchar data[14];		/* format-specific data      */
    volatile unchar vue;	/* vendor-unique error code  */
    volatile unchar status;	/* returned (icmb) status    */
    volatile unchar phase;	/* used by interrupt handler */
} IcbAny;

typedef union {
    unchar op;			/* ICB opcode                     */
    IcbRecvCmd recv_cmd;	/* format for receive command     */
    IcbSendStat send_stat;	/* format for send status         */
    IcbRevLvl rev_lvl;		/* format for get revision level  */
    IcbDiag diag;		/* format for execute diagnostics */
    IcbParms eparms;		/* format for get/set exec parms  */
    IcbAny icb;			/* generic format                 */
    unchar data[18];
} Icb;

#define WAITnexttimeout		200	/* 2 seconds */

typedef union {			/* let's cheat... */
    int i;
    unchar u[sizeof (int)];	/* the sizeof(int) makes it more portable */
} i_u;

#endif /* WD7000_DEFINES */


#if (LINUX_VERSION_CODE >= 0x020100)

#define WD7000 {					\
    proc_dir:		&proc_scsi_wd7000,		\
    proc_info:		wd7000_proc_info,		\
    name:		"Western Digital WD-7000",	\
    detect:		wd7000_detect,			\
    command:		wd7000_command,			\
    queuecommand:	wd7000_queuecommand,		\
    abort:		wd7000_abort,			\
    reset:		wd7000_reset,			\
    bios_param:		wd7000_biosparam,		\
    can_queue:		WD7000_Q,			\
    this_id:		7,				\
    sg_tablesize:	WD7000_SG,			\
    cmd_per_lun:	1,				\
    unchecked_isa_dma:	1,				\
    use_clustering:	ENABLE_CLUSTERING,		\
    use_new_eh_code:	0				\
}

#else /* Use old scsi code */

#define WD7000 {					\
    proc_dir:		&proc_scsi_wd7000,		\
    proc_info:		wd7000_proc_info,		\
    name:		"Western Digital WD-7000",	\
    detect:		wd7000_detect,			\
    command:		wd7000_command,			\
    queuecommand:	wd7000_queuecommand,		\
    abort:		wd7000_abort,			\
    reset:		wd7000_reset,			\
    bios_param:		wd7000_biosparam,		\
    can_queue:		WD7000_Q,			\
    this_id:		7,				\
    sg_tablesize:	WD7000_SG,			\
    cmd_per_lun:	1,				\
    unchecked_isa_dma:	1,				\
    use_clustering:	ENABLE_CLUSTERING,		\
}

#endif /* LINUX_VERSION_CODE */


extern struct proc_dir_entry proc_scsi_wd7000;


#ifdef WD7000_DEFINES
int  wd7000_diagnostics    (Adapter *, int);
int  wd7000_init           (Adapter *);
void wd7000_revision       (Adapter *);
#endif /* WD7000_DEFINES */

void wd7000_setup          (char *, int *);
int  make_code             (uint, uint);
void wd7000_intr_handle    (int, void *, struct pt_regs *);
void do_wd7000_intr_handle (int, void *, struct pt_regs *);
int  wd7000_queuecommand   (Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int  wd7000_command        (Scsi_Cmnd *);
int  wd7000_set_info       (char *, int, struct Scsi_Host *);
int  wd7000_proc_info      (char *, char **, off_t, int, int, int);
int  wd7000_detect         (Scsi_Host_Template *);
int  wd7000_abort          (Scsi_Cmnd *);
int  wd7000_reset          (Scsi_Cmnd *, uint);
int  wd7000_biosparam      (Disk *, kdev_t, int *);

#endif /* _WD7000_H */
