/***********************************************************************
 *	FILE NAME : TMSCSIW.C					       *
 *	     BY   : C.L. Huang	  (ching@tekram.com.tw) 	       *
 *	Description: Device Driver for Tekram DC-390W/U/F (T) PCI SCSI *
 *		     Bus Master Host Adapter			       *
 * (C)Copyright 1995-1996 Tekram Technology Co., Ltd.		       *
 ***********************************************************************/
/*	Minor enhancements and bugfixes by				*
 *	Kurt Garloff <K.Garloff@ping.de>				*
 ***********************************************************************/
/*	HISTORY:							*
 *									*
 *	REV#	DATE	NAME	DESCRIPTION				*
 *	1.00  04/03/96	CLH	First release				*
 *	1.01  04/11/96	CLH	Maximum support up to 4 Adapters,	*
 *				support KV 1_3_85			*
 *	1.02  04/26/96	CLH	fixed bug about EEpromBuf when >1 HA	*
 *	1.03  06/12/96	CLH	fixed bug of Media Change for Removable *
 *				Device, scan all LUN. Support Pre2.0.10 *
 *	1.04  06/18/96	CLH	fixed bug of Command timeout ....	*
 *	1.05  10/04/96	CLH	Updating for support KV 2.0.0, 2.0.20	*
 *	1.06  10/30/96	KG	Fixed bug in DC390W_abort(), module	*
 *				support, added tmscsiw_proc_info()	*
 *	1.07  11/09/96	KG	Fixed bug in tmscsiw_proc_info()	*
 *	1.08  11/18/96	CLH/KG	ditto, null ptr in DC390W_Disconnect()	*
 *	1.09  11/30/96	KG	Fixed bug in CheckEEpromCheckSum(),	*
 *				add register the allocated IO space	*
 *	1.10  12/05/96	CLH	Modify in tmscsiw_proc_info() and add	*
 *				in DC390W_initAdapter() for 53C875	*
 *				Rev. F with double clock.		*
 *	1.11  02/04/97	CLH	Fixed bug of Formatting a partition that*
 *				across 1GB boundary, with bad sector	*
 *				checking.				*
 *	1.12  02/17/97	CLH	Fixed bug in CheckEEpromCheckSum()	*
 ***********************************************************************/


#define DC390W_DEBUG
/* #define CHK_UNDER_RUN */

#define SCSI_MALLOC

#ifdef MODULE
#include <linux/module.h>
#endif

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/bios32.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/config.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE < 66354 /* 1.3.50 */
#include "../block/blk.h"
#else
#include <linux/blk.h>
#endif

#include "scsi.h"
#include "hosts.h"
#include "tmscsiw.h"
#include "constants.h"
#include "sd.h"
#include "scripts.h"
#include <linux/stat.h>

#include "dc390w.h"

#ifndef VERSION_ELF_1_2_13
struct proc_dir_entry	proc_scsi_tmscsiw ={
       PROC_SCSI_DC390WUF, 7 ,"tmscsiw",
       S_IFDIR | S_IRUGO | S_IXUGO, 2
       };
#endif

static void DC390W_StartSCSI( PACB pACB, PDCB pDCB, PSRB pSRB );
static void PrepareSG( PACB pACB, PDCB pDCB, PSRB pSRB );
static void DoingSRB_Done( PACB pACB );
static void ExceptionHandler(ULONG wlval, PACB pACB, PDCB pDCB);
static void ParityError( PACB pACB, PDCB pDCB );
static void PhaseMismatch( PACB pACB );
static void DC390W_ScsiRstDetect( PACB pACB );
static void DC390W_ResetSCSIBus( PACB pACB );
static void DC390W_ResetSCSIBus2( PACB pACB );
static void AdjustTemp( PACB pACB, PDCB pDCB, PSRB pSRB );
static void SetXferRate( PACB pACB, PDCB pDCB );
static void DataIOcommon( PACB pACB, ULONG  Swlval, ULONG  Cwlval );
static void SRBdone( PACB pACB, PDCB pDCB, PSRB pSRB );
static void RequestSense( PACB pACB, PDCB pDCB, PSRB pSRB );

static void DC390W_CmdCompleted( PACB pACB );
static void DC390W_Reselected( PACB pACB );
static void DC390W_Reselected1( PACB pACB );
static void DC390W_ReselectedT( PACB pACB );
static void DC390W_Disconnected( PACB pACB );
static void DC390W_MessageExtnd( PACB pACB );
static void DC390W_Signal( PACB pACB );
static void DC390W_UnknownMsg( PACB pACB );
static void DC390W_MessageOut( PACB pACB );
static void DC390W_FatalError( PACB pACB );
static void DC390W_MessageSync( PACB pACB );
static void DC390W_MessageWide( PACB pACB );
static void DC390W_RestorePtr( PACB pACB );
static void DC390W_MsgReject( PACB pACB );
static void DC390W_Debug( PACB pACB );
static void DC390W_download_script (struct Scsi_Host *host);

int    DC390W_initAdapter( PSH psh, ULONG io_port, UCHAR Irq, USHORT index);
void   DC390W_initDCB( PACB pACB, PDCB pDCB, PSCSICMD cmd );
void   MyDelay( void );
void   EnDisableCE( UCHAR Flag, USHORT scsiIOPort );
void   EEpromOutDI( USHORT Carry, USHORT scsiIOPort );
void   EEpromPrepare( UCHAR EEpromCmd, USHORT scsiIOPort );
void   ReadEEprom( PUCHAR EEpromBuf, USHORT scsiIOPort );
UCHAR  EEpromInDo(USHORT scsiIOPort);
USHORT EEpromGetData(USHORT scsiIOPort);
USHORT CheckEEpromCheckSum( PUCHAR EEpromBuf, USHORT scsiIOPort);

#ifdef MODULE
static int DC390W_release(struct Scsi_Host *host);
static int DC390W_shutdown (struct Scsi_Host *host);
#endif


static ULONG	jmp_table16;
static ULONG	jmp_din16;
static ULONG	jmp_dout16;
static PSHT	pSHT_start = NULL;
static PSH	pSH_start = NULL;
static PSH	pSH_current = NULL;
static PACB	pACB_start= NULL;
static PACB	pACB_current = NULL;
static PDCB	pPrevDCB = NULL;
static USHORT	adapterCnt = 0;
static USHORT	InitialTime = 0;
static USHORT	CurrDCBscntl3 = 0;
static UCHAR	pad_buffer[128];

static PVOID IntVector[]={
       DC390W_CmdCompleted,
       DC390W_Reselected,
       DC390W_Reselected1,
       DC390W_ReselectedT,
       DC390W_Disconnected,
       DC390W_MessageExtnd,
       DC390W_Signal,
       DC390W_UnknownMsg,
       DC390W_MessageOut,
       DC390W_FatalError,
       DC390W_MessageSync,
       DC390W_MessageWide,
       DC390W_RestorePtr,
       DC390W_MsgReject,
       DC390W_Debug,
       DC390W_FatalError
       };

UCHAR  eepromBuf[MAX_ADAPTER_NUM][128];

UCHAR  clock_period[12] = {25, 31, 37, 43, 50, 62, 75, 125, 12, 15, 18, 21};
UCHAR  baddevname[2][28] ={
       "SEAGATE ST3390N         9546",
       "SEAGATE ST3390N ???     0399"};

#define BADDEVCNT	2

/***********************************************************************
 *
 *
 *
 **********************************************************************/
static void
QLinkcmd( PSCSICMD cmd, PDCB pDCB )
{
    ULONG  flags;
    PSCSICMD  pcmd;

    save_flags(flags);
    cli();

    if( !pDCB->QIORBCnt )
    {
	pDCB->pQIORBhead = cmd;
	pDCB->pQIORBtail = cmd;
	pDCB->QIORBCnt++;
	cmd->next = NULL;
    }
    else
    {
	pcmd = pDCB->pQIORBtail;
	pcmd->next = cmd;
	pDCB->pQIORBtail = cmd;
	pDCB->QIORBCnt++;
	cmd->next = NULL;
    }

    restore_flags(flags);
}


static PSCSICMD
Getcmd( PDCB pDCB )
{
    ULONG  flags;
    PSCSICMD  pcmd;

    save_flags(flags);
    cli();

    pcmd = pDCB->pQIORBhead;
    pDCB->pQIORBhead = pcmd->next;
    pcmd->next = NULL;
    pDCB->QIORBCnt--;

    restore_flags(flags);
    return( pcmd );
}


static PSRB
GetSRB( PACB pACB )
{
    ULONG  flags;
    PSRB   pSRB;

    save_flags(flags);
    cli();

    pSRB = pACB->pFreeSRB;
    if( pSRB )
    {
	pACB->pFreeSRB = pSRB->pNextSRB;
	pSRB->pNextSRB = NULL;
    }
    restore_flags(flags);
    return( pSRB );
}


static void
RewaitSRB( PDCB pDCB, PSRB pSRB )
{
    PSRB   psrb1;
    ULONG  flags;
    UCHAR  bval;

    save_flags(flags);
    cli();
    pDCB->GoingSRBCnt--;
    psrb1 = pDCB->pGoingSRB;
    if( pSRB == psrb1 )
    {
	pDCB->pGoingSRB = psrb1->pNextSRB;
    }
    else
    {
	while( pSRB != psrb1->pNextSRB )
	    psrb1 = psrb1->pNextSRB;
	psrb1->pNextSRB = pSRB->pNextSRB;
	if( pSRB == pDCB->pGoingLast )
	    pDCB->pGoingLast = psrb1;
    }
    if( (psrb1 = pDCB->pWaitingSRB) )
    {
	pSRB->pNextSRB = psrb1;
	pDCB->pWaitingSRB = pSRB;
    }
    else
    {
	pSRB->pNextSRB = NULL;
	pDCB->pWaitingSRB = pSRB;
	pDCB->pWaitLast = pSRB;
    }

    bval = pSRB->TagNumber;
    pDCB->TagMask &= (~(1 << bval));	  /* Free TAG number */
    restore_flags(flags);
}


static void
DoWaitingSRB( PACB pACB )
{
    ULONG  flags;
    PDCB   ptr, ptr1;
    PSRB   pSRB;

    save_flags(flags);
    cli();

    if( !(pACB->pActiveDCB) && !(pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV) ) )
    {
	ptr = pACB->pDCBRunRobin;
	if( !ptr )
	{
	    ptr = pACB->pLinkDCB;
	    pACB->pDCBRunRobin = ptr;
	}
	ptr1 = ptr;
	for( ;ptr1; )
	{
	    pACB->pDCBRunRobin = ptr1->pNextDCB;
	    if( !( ptr1->MaxCommand > ptr1->GoingSRBCnt ) ||
		!( pSRB = ptr1->pWaitingSRB ) )
	    {
		if(pACB->pDCBRunRobin == ptr)
		    break;
		ptr1 = ptr1->pNextDCB;
	    }
	    else
	    {
		DC390W_StartSCSI(pACB, ptr1, pSRB);
		ptr1->GoingSRBCnt++;
		if( ptr1->pWaitLast == pSRB )
		{
		    ptr1->pWaitingSRB = NULL;
		    ptr1->pWaitLast = NULL;
		}
		else
		{
		    ptr1->pWaitingSRB = pSRB->pNextSRB;
		}
		pSRB->pNextSRB = NULL;

		if( ptr1->pGoingSRB )
		    ptr1->pGoingLast->pNextSRB = pSRB;
		else
		    ptr1->pGoingSRB = pSRB;
		ptr1->pGoingLast = pSRB;

		break;
	    }
	}
    }
    restore_flags(flags);
    return;
}


static void
SRBwaiting( PDCB pDCB, PSRB pSRB)
{
    if( pDCB->pWaitingSRB )
    {
	pDCB->pWaitLast->pNextSRB = pSRB;
	pDCB->pWaitLast = pSRB;
	pSRB->pNextSRB = NULL;
    }
    else
    {
	pDCB->pWaitingSRB = pSRB;
	pDCB->pWaitLast = pSRB;
    }
}


static void
SendSRB( PSCSICMD pcmd, PACB pACB, PSRB pSRB )
{
    ULONG  flags;
    PDCB   pDCB;

    save_flags(flags);
    cli();

    pDCB = pSRB->pSRBDCB;
    PrepareSG( pACB, pDCB, pSRB );
    if( !(pDCB->MaxCommand > pDCB->GoingSRBCnt) || (pACB->pActiveDCB) ||
	(pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV)) )
    {
	SRBwaiting(pDCB, pSRB);
	goto SND_EXIT;
    }

    if( pDCB->pWaitingSRB )
    {
	SRBwaiting(pDCB, pSRB);
/*	pSRB = GetWaitingSRB(pDCB); */
	pSRB = pDCB->pWaitingSRB;
	pDCB->pWaitingSRB = pSRB->pNextSRB;
	pSRB->pNextSRB = NULL;
    }

    DC390W_StartSCSI(pACB, pDCB, pSRB);
    pDCB->GoingSRBCnt++;
    if( pDCB->pGoingSRB )
    {
	pDCB->pGoingLast->pNextSRB = pSRB;
	pDCB->pGoingLast = pSRB;
    }
    else
    {
	pDCB->pGoingSRB = pSRB;
	pDCB->pGoingLast = pSRB;
    }

SND_EXIT:
    restore_flags(flags);
    return;
}


/***********************************************************************
 * Function : static int DC390W_queue_command (Scsi_Cmnd *cmd,
 *					       void (*done)(Scsi_Cmnd *))
 *
 * Purpose : enqueues a SCSI command
 *
 * Inputs : cmd - SCSI command, done - function called on completion, with
 *	    a pointer to the command descriptor.
 *
 * Returns : 0
 *
 ***********************************************************************/

int
DC390W_queue_command (Scsi_Cmnd *cmd, void (* done)(Scsi_Cmnd *))
{
    USHORT ioport, i;
    Scsi_Cmnd *pcmd;
    struct Scsi_Host *psh;
    PACB   pACB;
    PDCB   pDCB;
    PSRB   pSRB;
    ULONG  flags;
    PUCHAR ptr,ptr1;

    psh = cmd->host;
    pACB = (PACB ) psh->hostdata;
    ioport = pACB->IOPortBase;

#ifdef DC390W_DEBUG0
	printk("Cmd=%x,",cmd->cmnd[0]);
#endif

    if( (pACB->scan_devices == END_SCAN) && (cmd->cmnd[0] != INQUIRY) )
    {
	pACB->scan_devices = 0;
	pPrevDCB->pNextDCB = pACB->pLinkDCB;
    }
    else if( (pACB->scan_devices) && (cmd->cmnd[0] == 8) )
    {
	pACB->scan_devices = 0;
	pPrevDCB->pNextDCB = pACB->pLinkDCB;
    }

    if ( ( cmd->target > pACB->max_id ) || (cmd->lun > pACB->max_lun) )
    {
/*	printk("DC390W: Ignore target %d lun %d\n",
		cmd->target, cmd->lun); */
	cmd->result = (DID_BAD_TARGET << 16);
	done(cmd);
	return( 0 );
    }

    if( (pACB->scan_devices) && !(pACB->DCBmap[cmd->target] & (1 << cmd->lun)) )
    {
	if( pACB->DeviceCnt < MAX_DEVICES )
	{
	    pACB->DCBmap[cmd->target] |= (1 << cmd->lun);
	    pDCB = pACB->pDCB_free;
#ifdef DC390W_DEBUG0
	    printk("pDCB=%8x,ID=%2x,", (UINT) pDCB, cmd->target);
#endif
	    DC390W_initDCB( pACB, pDCB, cmd );
	}
	else	/* ???? */
	{
/*	printk("DC390W: Ignore target %d lun %d\n",
		    cmd->target, cmd->lun); */
	    cmd->result = (DID_BAD_TARGET << 16);
	    done(cmd);
	    return(0);
	}
    }
    else if( !(pACB->scan_devices) && !(pACB->DCBmap[cmd->target] & (1 << cmd->lun)) )
    {
/*	printk("DC390W: Ignore target %d lun %d\n",
		cmd->target, cmd->lun); */
	cmd->result = (DID_BAD_TARGET << 16);
	done(cmd);
	return(0);
    }
    else
    {
	pDCB = pACB->pLinkDCB;
	while( (pDCB->UnitSCSIID != cmd->target) ||
	       (pDCB->UnitSCSILUN != cmd->lun) )
	{
	    pDCB = pDCB->pNextDCB;
	}
#ifdef DC390W_DEBUG0
	printk("pDCB=%8x,ID=%2x,Scan=%1x", (UINT) pDCB, cmd->target,
		pACB->scan_devices);
#endif
    }

    cmd->scsi_done = done;
    cmd->result = 0;

    save_flags(flags);
    cli();

    if( pDCB->QIORBCnt )
    {
	QLinkcmd( cmd, pDCB );
	pcmd = Getcmd( pDCB );
    }
    else
	pcmd = cmd;

    pSRB = GetSRB( pACB );

    if( !pSRB )
    {
	QLinkcmd( pcmd, pDCB );
	restore_flags(flags);
	return(0);
    }

/*  BuildSRB(pSRB); */

    pSRB->pSRBDCB = pDCB;
    pSRB->pcmd = pcmd;
    ptr = (PUCHAR) pSRB->CmdBlock;
    ptr1 = (PUCHAR) pcmd->cmnd;
    (UCHAR) pSRB->__command[0] = pcmd->cmd_len;
    for(i=0; i< pcmd->cmd_len; i++)
    {
	*ptr = *ptr1;
	ptr++;
	ptr1++;
    }
    if( pcmd->use_sg )
    {
	pSRB->SGcount = (UCHAR) pcmd->use_sg;
	pSRB->pSegmentList = (PSGL) pcmd->request_buffer;
    }
    else if( pcmd->request_buffer )
    {
	pSRB->SGcount = 1;
	pSRB->pSegmentList = (PSGL) &pSRB->Segmentx;
	pSRB->Segmentx.address = (PUCHAR) pcmd->request_buffer;
	pSRB->Segmentx.length = pcmd->request_bufflen;
    }
    else
	pSRB->SGcount = 0;

    pSRB->AdaptStatus = 0;
    pSRB->TargetStatus = 0;
    pSRB->MsgCnt = 0;
    if( pDCB->DevType != TYPE_TAPE )
	pSRB->RetryCnt = 1;
    else
	pSRB->RetryCnt = 0;
    pSRB->SRBStatus = 0;
    pSRB->SRBFlag = 0;
    pSRB->ScratchABuf = 0;
    pSRB->SRBState = 0;
    pSRB->RemainSegPtr = 0;
    pSRB->XferredLen = 0;
    SendSRB( pcmd, pACB, pSRB );

    restore_flags(flags);
    return(0);
}


static void
DoNextCmd( PACB pACB, PDCB pDCB )
{
    Scsi_Cmnd *pcmd;
    PSRB   pSRB;
    ULONG  flags;
    PUCHAR ptr,ptr1;
    USHORT i;


    if( pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV) )
	return;
    save_flags(flags);
    cli();

    pcmd = Getcmd( pDCB );
    pSRB = GetSRB( pACB );
    if( !pSRB )
    {
	QLinkcmd( pcmd, pDCB );
	restore_flags(flags);
	return;
    }

    pSRB->pSRBDCB = pDCB;
    pSRB->pcmd = pcmd;
    ptr = (PUCHAR) pSRB->CmdBlock;
    ptr1 = (PUCHAR) pcmd->cmnd;
    (UCHAR) pSRB->__command[0] = pcmd->cmd_len;
    for(i=0; i< pcmd->cmd_len; i++)
    {
	*ptr = *ptr1;
	ptr++;
	ptr1++;
    }
    if( pcmd->use_sg )
    {
	pSRB->SGcount = (UCHAR) pcmd->use_sg;
	pSRB->pSegmentList = (PSGL) pcmd->request_buffer;
    }
    else if( pcmd->request_buffer )
    {
	pSRB->SGcount = 1;
	pSRB->pSegmentList = (PSGL) &pSRB->Segmentx;
	pSRB->Segmentx.address = (PUCHAR) pcmd->request_buffer;
	pSRB->Segmentx.length = pcmd->request_bufflen;
    }
    else
	pSRB->SGcount = 0;

    pSRB->AdaptStatus = 0;
    pSRB->TargetStatus = 0;
    pSRB->MsgCnt = 0;
    if( pDCB->DevType != TYPE_TAPE )
	pSRB->RetryCnt = 1;
    else
	pSRB->RetryCnt = 0;
    pSRB->SRBStatus = 0;
    pSRB->SRBFlag = 0;
    pSRB->ScratchABuf = 0;
    pSRB->SRBState = 0;
    SendSRB( pcmd, pACB, pSRB );

    restore_flags(flags);
    return;
}


/***********************************************************************
 * Function:
 *   DC390W_bios_param
 *
 * Description:
 *   Return the disk geometry for the given SCSI device.
 ***********************************************************************/
#ifdef	VERSION_ELF_1_2_13
int DC390W_bios_param(Disk *disk, int devno, int geom[])
#else
int DC390W_bios_param(Disk *disk, kdev_t devno, int geom[])
#endif
{
    int heads, sectors, cylinders;
    PACB pACB;

    pACB = (PACB) disk->device->host->hostdata;
    heads = 64;
    sectors = 32;
    cylinders = disk->capacity / (heads * sectors);

    if ( (pACB->Gmode2 & GREATER_1G) && (cylinders > 1024) )
    {
      heads = 255;
      sectors = 63;
      cylinders = disk->capacity / (heads * sectors);
    }

    geom[0] = heads;
    geom[1] = sectors;
    geom[2] = cylinders;

    return (0);
}


/***********************************************************************
 * Function : int DC390W_abort (Scsi_Cmnd *cmd)
 *
 * Purpose : Abort an errant SCSI command
 *
 * Inputs : cmd - command to abort
 *
 * Returns : 0 on success, -1 on failure.
 ***********************************************************************/

int
DC390W_abort (Scsi_Cmnd *cmd)
{
    ULONG flags;
    PACB  pACB;
    PDCB  pDCB, pdcb;
    PSRB  pSRB, psrb;
    USHORT count, i;
    PSCSICMD  pcmd, pcmd1;
    int   status;


#ifdef DC390W_DEBUG0
    printk("DC390W : Abort Cmd.");
#endif

    save_flags(flags);
    cli();

    pACB = (PACB) cmd->host->hostdata;
    pDCB = pACB->pLinkDCB;
    pdcb = pDCB;
    while( (pDCB->UnitSCSIID != cmd->target) ||
	   (pDCB->UnitSCSILUN != cmd->lun) )
    {
	pDCB = pDCB->pNextDCB;
	if( pDCB == pdcb )
	    goto  NOT_RUN;
    }

    if( pDCB->QIORBCnt )
    {
	pcmd = pDCB->pQIORBhead;
	if( pcmd == cmd )
	{
	    pDCB->pQIORBhead = pcmd->next;
	    pcmd->next = NULL;
	    pDCB->QIORBCnt--;
	    status = SCSI_ABORT_SUCCESS;
	    goto  ABO_X;
	}
	for( count = pDCB->QIORBCnt, i=0; i<count-1; i++)
	{
	    if( pcmd->next == cmd )
	    {
		pcmd1 = pcmd->next;
		pcmd->next = pcmd1->next;
		pcmd1->next = NULL;
		pDCB->QIORBCnt--;
		status = SCSI_ABORT_SUCCESS;
		goto  ABO_X;
	    }
	    else
	    {
		pcmd = pcmd->next;
	    }
	}
    }

    pSRB = pDCB->pWaitingSRB;
    if( !pSRB )
	goto  ON_GOING;
    if( pSRB->pcmd == cmd )
    {
	pDCB->pWaitingSRB = pSRB->pNextSRB;
	goto  IN_WAIT;
    }
    else
    {
	psrb = pSRB;
	if( !(psrb->pNextSRB) )
	    goto ON_GOING;
	while( psrb->pNextSRB->pcmd != cmd )
	{
	    psrb = psrb->pNextSRB;
	    if( !(psrb->pNextSRB) )
		goto ON_GOING;
	}
	pSRB = psrb->pNextSRB;
	psrb->pNextSRB = pSRB->pNextSRB;
	if( pSRB == pDCB->pWaitLast )
	    pDCB->pWaitLast = psrb;
IN_WAIT:
	pSRB->pNextSRB = pACB->pFreeSRB;
	pACB->pFreeSRB = pSRB;
	cmd->next = NULL;
	status = SCSI_ABORT_SUCCESS;
	goto  ABO_X;
    }

ON_GOING:
    pSRB = pDCB->pGoingSRB;
    for( count = pDCB->GoingSRBCnt, i=0; i<count; i++)
    {
	if( pSRB->pcmd != cmd )
	    pSRB = pSRB->pNextSRB;
	else
	{
	    if( (pACB->pActiveDCB == pDCB) && (pDCB->pActiveSRB == pSRB) )
	    {
		status = SCSI_ABORT_BUSY;
		goto  ABO_X;
	    }
	    else
	    {
		status = SCSI_ABORT_SNOOZE;
		goto  ABO_X;
	    }
	}
    }

NOT_RUN:
    status = SCSI_ABORT_NOT_RUNNING;

ABO_X:
    cmd->result = DID_ABORT << 16;
    cmd->scsi_done(cmd);
    restore_flags(flags);
    return( status );
}


static void
ResetDevParam( PACB pACB )
{
    PDCB   pDCB, pdcb;

    pDCB = pACB->pLinkDCB;
    if( pDCB == NULL )
	return;
    pdcb = pDCB;
    do
    {
	if( pACB->AdaptType == DC390W )
	    pdcb->DCBscntl3 = SYNC_CLK_F2+ASYNC_CLK_F2;
	else
	    pdcb->DCBscntl3 = SYNC_CLK_F4+ASYNC_CLK_F4;
	pdcb->DCBsxfer = 0;
	pdcb = pdcb->pNextDCB;
    }
    while( pdcb != pDCB );
}


static void
RecoverSRB( PACB pACB )
{
    PDCB   pDCB, pdcb;
    PSRB   psrb, psrb2;
    USHORT cnt, i;

    pDCB = pACB->pLinkDCB;
    if( pDCB == NULL )
	return;
    pdcb = pDCB;
    do
    {
	cnt = pdcb->GoingSRBCnt;
	psrb = pdcb->pGoingSRB;
	for (i=0; i<cnt; i++)
	{
	    PrepareSG( pACB, pdcb, psrb );
	    psrb2 = psrb;
	    psrb = psrb->pNextSRB;
/*	    RewaitSRB( pDCB, psrb ); */
	    if( pdcb->pWaitingSRB )
	    {
		psrb2->pNextSRB = pdcb->pWaitingSRB;
		pdcb->pWaitingSRB = psrb2;
	    }
	    else
	    {
		pdcb->pWaitingSRB = psrb2;
		pdcb->pWaitLast = psrb2;
		psrb2->pNextSRB = NULL;
	    }
	}
	pdcb->GoingSRBCnt = 0;
	pdcb->pGoingSRB = NULL;
	pdcb->TagMask = 0;
	pdcb = pdcb->pNextDCB;
    }
    while( pdcb != pDCB );
}


/***********************************************************************
 * Function : int DC390W_reset (Scsi_Cmnd *cmd, ...)
 *
 * Purpose : perform a hard reset on the SCSI bus( and NCR chip).
 *
 * Inputs : cmd - command which caused the SCSI RESET
 *
 * Returns : 0 on success.
 ***********************************************************************/

#ifdef	VERSION_2_0_0
int DC390W_reset(Scsi_Cmnd *cmd, unsigned int resetFlags)
#else
int DC390W_reset (Scsi_Cmnd *cmd)
#endif
{
    USHORT   ioport;
    unsigned long flags;
    PACB  pACB;
    ULONG    wlval;
    UCHAR    bval;
    USHORT   wval;
    USHORT  i;


#ifdef DC390W_DEBUG0
    printk("DC390W : Reset Cmd0,");
#endif

    pACB = (PACB ) cmd->host->hostdata;
    ioport = pACB->IOPortBase;
    save_flags(flags);
    cli();
    bval = inb(ioport+DCNTL);
    bval |= IRQ_DISABLE;
    outb(bval,ioport+DCNTL);  /* disable interrupt */
    DC390W_ResetSCSIBus( pACB );
    for( i=0; i<500; i++ )
	udelay(1000);
    for(;;)
    {
	 bval = inb(ioport+ISTAT);
	 if( bval & SCSI_INT_PENDING )
	 {
	     wval = inw( ioport+SIST0 );
	     if( wval & (SCSI_RESET+SCSI_GERROR) )
		 break;
	 }
	 if(bval & DMA_INT_PENDING)
	 {
	     bval = inb(ioport+DSTAT);
	     if(bval & ABORT_)
	     {
		 wval = inw( ioport+SIST0 );
		 break;
	     }
	 }
    }
    bval = inb(ioport+DCNTL);
    bval &= ~IRQ_DISABLE;
    outb(bval,ioport+DCNTL); /* re-enable interrupt */

    ioport = pACB->IOPortBase;
    bval = inb(ioport+STEST3);
    bval |= CLR_SCSI_FIFO;
    outb(bval,ioport+STEST3);
    bval = CLR_DMA_FIFO;
    outb(bval,ioport+CTEST3);
    ResetDevParam( pACB );
    DoingSRB_Done( pACB );
    pACB->pActiveDCB = NULL;
    wlval = pACB->jmp_reselect;
    outl(wlval,(ioport+DSP));

    pACB->ACBFlag = 0;
    DoWaitingSRB( pACB );
    restore_flags(flags);
    return( SCSI_RESET_SUCCESS );
}


#include "scsiio.c"


/***********************************************************************
 * Function : static void DC390W_initDCB
 *
 * Purpose :  initialize the internal structures for a given DCB
 *
 * Inputs : cmd - pointer to this scsi cmd request block structure
 *
 ***********************************************************************/
void DC390W_initDCB( PACB pACB, PDCB pDCB, PSCSICMD cmd )
{
    PEEprom	prom;
    UCHAR	bval;
    USHORT	index;

    if( pACB->DeviceCnt == 0 )
    {
	pACB->pLinkDCB = pDCB;
	pACB->pDCBRunRobin = pDCB;
	pDCB->pNextDCB = pDCB;
	pPrevDCB = pDCB;
    }
    else
	pPrevDCB->pNextDCB = pDCB;

    pDCB->pDCBACB = pACB;
    pDCB->QIORBCnt = 0;
    pDCB->DCBselect = 0;
    pDCB->DCBsxfer = 0;
    pDCB->DCBsdid = cmd->target;
    pDCB->UnitSCSIID = cmd->target;
    pDCB->UnitSCSILUN = cmd->lun;
    pDCB->pWaitingSRB = NULL;
    pDCB->GoingSRBCnt = 0;
    pDCB->TagMask = 0;

    pDCB->MaxCommand = 1;
    pDCB->AdaptIndex = pACB->AdapterIndex;
    index = pACB->AdapterIndex;

    prom = (PEEprom) &eepromBuf[index][cmd->target << 2];
    pDCB->DevMode = prom->EE_MODE1;
    pDCB->NegoPeriod = clock_period[prom->EE_SPEED];

    if( pACB->AdaptType == DC390W )
	pDCB->DCBscntl3 = SYNC_CLK_F2+ASYNC_CLK_F2;
    else
	pDCB->DCBscntl3 = SYNC_CLK_F4+ASYNC_CLK_F4;

    if( pDCB->DevMode & PARITY_CHK_ )
	pDCB->DCBscntl0 = EN_PARITY_CHK+SATN_IF_PARITY_ERR+FULL_ARBITRATION;
    else
	pDCB->DCBscntl0 = FULL_ARBITRATION;

    pDCB->AdpMode = eepromBuf[index][EE_MODE2];

    if( pDCB->DevMode & EN_DISCONNECT_ )
	bval = 0xC0;
    else
	bval = 0x80;
    bval |= cmd->lun;
    pDCB->IdentifyMsg = bval;

    if( pDCB->DevMode & SYNC_NEGO_ )
    {
	pDCB->SyncMode = SYNC_ENABLE;
	pDCB->SyncOffset = SYNC_NEGO_OFFSET;
    }

    if( pDCB->DevMode & WIDE_NEGO_ )
    {
	if( cmd->lun )
	{
	    if( !(CurrDCBscntl3 & EN_WIDE_SCSI) )
		pDCB->DevMode &= ~WIDE_NEGO_;
	}
	else
	    CurrDCBscntl3 = 0;
    }
    pDCB->DCBFlag = 0;
}


/***********************************************************************
 * Function : static void DC390W_initSRB
 *
 * Purpose :  initialize the internal structures for a given SRB
 *
 * Inputs : psrb - pointer to this scsi request block structure
 *
 ***********************************************************************/
void DC390W_initSRB( PSRB psrb )
{
#ifndef VERSION_ELF_1_2_13
	psrb->PhysSRB = virt_to_phys( psrb );
	psrb->__command[1] = virt_to_phys( psrb->CmdBlock );
	psrb->__msgout0[0] = 1;
	psrb->__msgout0[1] = virt_to_phys( psrb->MsgOutBuf );
	psrb->SegmentPad[0] = 16;
	psrb->SegmentPad[1] = virt_to_phys( pad_buffer );
#else
	psrb->PhysSRB = (ULONG) psrb;
	psrb->__command[1] = (ULONG) psrb->CmdBlock;
	psrb->__msgout0[0] = 1;
	psrb->__msgout0[1] = (ULONG) psrb->MsgOutBuf;
	psrb->SegmentPad[0] = 16;
	psrb->SegmentPad[1] = (ULONG) pad_buffer;
#endif
}


void DC390W_linkSRB( PACB pACB )
{
    USHORT  count, i;
    PSRB    psrb;

    count = pACB->SRBCount;

    for( i=0; i< count; i++)
    {
	if( i != count - 1)
	    pACB->SRB_array[i].pNextSRB = &pACB->SRB_array[i+1];
	else
	    pACB->SRB_array[i].pNextSRB = NULL;
	psrb = (PSRB) &pACB->SRB_array[i];
	DC390W_initSRB( psrb );
    }
}


/***********************************************************************
 * Function : static void DC390W_initACB
 *
 * Purpose :  initialize the internal structures for a given SCSI host
 *
 * Inputs : psh - pointer to this host adapter's structure
 *
 ***********************************************************************/
void DC390W_initACB( PSH psh, USHORT chipType, ULONG io_port, UCHAR Irq, USHORT index )
{
    PACB    pACB;
    USHORT  i;
    UCHAR   adaptType, bval;


    psh->can_queue = MAX_CMD_QUEUE;
    psh->cmd_per_lun = MAX_CMD_PER_LUN;
    psh->this_id = (int) eepromBuf[index][EE_ADAPT_SCSI_ID];
    psh->io_port = io_port;
    psh->n_io_port = 0x80;
    psh->irq = Irq;

    if( chipType == PCI_DEVICE_ID_NCR53C825A )
	adaptType = DC390W;
    else
    {
	outb( 2, io_port+GPREG );
	bval = inb( io_port+GPREG );
	if( bval & 8 )
	    adaptType = DC390U;
	else
	    adaptType = DC390F;
    }

    pACB = (PACB) psh->hostdata;

#ifndef VERSION_ELF_1_2_13
    if( adaptType == DC390U )
    {
	psh->max_id = 8;
	pACB->max_id = 7;
    }
    else
    {
	psh->max_id = 16;
	pACB->max_id = 15;
    }

#ifdef	CONFIG_SCSI_MULTI_LUN
    if( eepromBuf[index][EE_MODE2] & LUN_CHECK )
	psh->max_lun = 8;
    else
#endif
	psh->max_lun = 1;

#else
	pACB->max_id = 7;
#endif
    if( pACB->max_id == eepromBuf[index][EE_ADAPT_SCSI_ID] )
	pACB->max_id--;

#ifdef	CONFIG_SCSI_MULTI_LUN
    if( eepromBuf[index][EE_MODE2] & LUN_CHECK )
	pACB->max_lun = 7;
    else
#endif
	pACB->max_lun = 0;

    pACB->pScsiHost = psh;
    pACB->IOPortBase = (USHORT) io_port;
    pACB->pLinkDCB = NULL;
    pACB->pDCBRunRobin = NULL;
    pACB->pActiveDCB = NULL;
    pACB->pFreeSRB = pACB->SRB_array;
    pACB->SRBCount = MAX_SRB_CNT;
    pACB->AdapterIndex = index;
    pACB->status = 0;
    pACB->AdaptSCSIID = eepromBuf[index][EE_ADAPT_SCSI_ID];
    pACB->AdaptSCSILUN = 0;
    pACB->DeviceCnt = 0;
    pACB->IRQLevel = Irq;
    pACB->AdaptType = adaptType;
    pACB->TagMaxNum = eepromBuf[index][EE_TAG_CMD_NUM] << 2;
    pACB->ACBFlag = 0;
    pACB->scan_devices = 1;
    pACB->Gmode2 = eepromBuf[index][EE_MODE2];
    if( eepromBuf[index][EE_MODE2] & LUN_CHECK )
	pACB->LUNchk = 1;
    pACB->pDCB_free = &pACB->DCB_array[0];
    DC390W_linkSRB( pACB );
    for(i=0; i<MAX_SCSI_ID; i++)
	pACB->DCBmap[i] = 0;
}


/***********************************************************************
 * Function : static int DC390W_initAdapter
 *
 * Purpose :  initialize the SCSI chip ctrl registers
 *
 * Inputs : psh - pointer to this host adapter's structure
 *
 ***********************************************************************/
int DC390W_initAdapter( PSH psh, ULONG io_port, UCHAR Irq, USHORT index )
{
    USHORT ioport, wval;
    UCHAR  bval;
    PACB   pACB, pacb;
    USHORT used_irq = 0;

    pacb = pACB_start;
    if( pacb != NULL )
    {
	for ( ; (pacb != (PACB) -1) ; )
	{
	    if( pacb->IRQLevel == Irq )
	    {
		used_irq = 1;
		break;
	    }
	    else
		pacb = pacb->pNextACB;
	}
    }

    if( !used_irq )
    {
#ifdef	VERSION_ELF_1_2_13
	if( request_irq(Irq, DC390W_Interrupt, SA_INTERRUPT, "tmscsiw"))
#else
	if( request_irq(Irq, DC390W_Interrupt, SA_INTERRUPT | SA_SHIRQ, "tmscsiw", NULL))
#endif
	{
	    printk("DC390W : register IRQ error!\n");
	    return( -1 );
	}
    }
    request_region(io_port,psh->n_io_port,"tmscsiw");

    ioport = (USHORT) io_port;
    outb(IRQ_DISABLE, ioport+DCNTL);
    outb(ABORT_OP, ioport+ISTAT);
    udelay(100000);
    outb(0, ioport+ISTAT);
    bval = inb(ioport+DSTAT);
    bval = inb(ioport+ISTAT);
    wval = inw(ioport+SIST0);

    pACB = (PACB) psh->hostdata;
    bval = pACB->AdaptSCSIID;
    bval |= ENABLE_RESEL;
    outb(bval,ioport+SCID);

    if(pACB->AdaptType == DC390W)
	bval = SYNC_CLK_F2+ASYNC_CLK_F2;
    else
    {						/* @1.09 */
	bval = inb(ioport+CTEST3);
	if( (bval & CHIP_REV_MASK) < 0x30 )	/* 53C875 Rev. F or later ? */
	    goto  REVF;
	bval = inb(ioport+STEST1);
	if( (bval & 0x0C) == 0x0C )		/* double clock already enable ? */
	    goto  REVF;
	outb(8,ioport+STEST1);			/* enable clock doubler */
	udelay(20);
	outb(HALT_SCSI_CLK,ioport+STEST3);	/* halt clock */
	outb(0x0C,ioport+STEST1);		/* select double SCSI clock */
	outb(0,ioport+STEST3);			/* re-enable clock */
REVF:
	bval = SYNC_CLK_F4+ASYNC_CLK_F4;
    }
    outb(bval,ioport+SCNTL3);

    bval = SYNC_PERIOD_F4+ASYNCHRONOUS; 	/* set to async */
    outb(bval,ioport+SXFER);

    bval = WRT_EN_INVALIDATE;		/* Enable write and invalidate */
    outb(bval,ioport+CTEST3);

    bval = EN_DMA_FIFO_536+BURST_LEN_MSB;	/* select 536 bytes DMA FIFO, burst len bit2=1 */
    outb(bval,ioport+CTEST5);

    bval = BURST_LEN8+EN_READ_LINE+EN_READ_MULTIPLE+BURST_OPCODE_FETCH+AUTO_START;   /* set DMA parameter */
    outb(bval,ioport+DMODE);

    bval = EN_ABORTED+EN_SCRIPT_INT+EN_ILLEGAL_INST;	/* enable DMA interrupt */
    outb(bval,ioport+DIEN);

    bval = EN_CACHE_LINE_SIZE+EN_PRE_FETCH+TOTEM_POLE_IRQ+COMPATIBLE_700;
    outb(bval,ioport+DCNTL);

    bval = EN_PHASE_MISMATCH+EN_SCSI_GERROR+EN_UNEXPECT_DISC+EN_SCSI_RESET+EN_PARITY_ERROR;
    outb(bval,ioport+SIEN0);

    bval = EN_SEL_TIMEOUT+EN_GENERAL_TIMEOUT;
    outb(bval,ioport+SIEN1);

    bval = SEL_TO_204ms;	/* 250ms selection timeout */
    outb(bval,ioport+STIME0);

    wval = 1 << (eepromBuf[index][EE_ADAPT_SCSI_ID]);	/* @1.11 */
    outw(wval,ioport+RESPID0);

    bval = DIS_SINGLE_INIT;
    if( eepromBuf[index][EE_MODE2] & ACTIVE_NEGATION )
	 bval |= ACTIVE_NEGATION_;
    outb(bval,ioport+STEST3);

    return(0);
}


/***********************************************************************
 * Function : static int DC390W_init (struct Scsi_Host *host)
 *
 * Purpose :  initialize the internal structures for a given SCSI host
 *
 * Inputs : host - pointer to this host adapter's structure/
 *
 * Preconditions : when this function is called, the chip_type
 *	field of the pACB structure MUST have been set.
 ***********************************************************************/

static int
DC390W_init (PSHT psht, USHORT chipType, ULONG io_port, UCHAR Irq, USHORT index)
{
    PSH   psh;
    PACB  pACB;

    if( ! CheckEEpromCheckSum( &eepromBuf[index][0], (USHORT) io_port) )
    {
	psh = scsi_register( psht, sizeof(DC390W_ACB) );
	if( !psh )
	    return( -1 );
	if( !pSH_start )
	{
	    pSH_start = psh;
	    pSH_current = psh;
	}
	else
	{
	    pSH_current->next = psh;
	    pSH_current = psh;
	}

#ifdef DC390W_DEBUG0
	printk("DC390W : pSH = %8x,", (UINT) psh);
#endif

	DC390W_initACB( psh, chipType, io_port, Irq, index );
	if( !DC390W_initAdapter( psh, io_port, Irq, index  ) )
	{
	    pACB = (PACB) psh->hostdata;
	    if( !pACB_start )
	    {
		pACB_start = pACB;
		pACB_current = pACB;
		pACB->pNextACB = (PACB) -1;
	    }
	    else
	    {
		pACB_current->pNextACB = pACB;
		pACB_current = pACB;
		pACB->pNextACB = (PACB)  -1;
	    }

#ifdef DC390W_DEBUG0
	printk("DC390W : pACB = %8x, pDCB_array = %8x, pSRB_array = %8x\n",
	      (UINT) pACB, (UINT) pACB->DCB_array, (UINT) pACB->SRB_array);
	printk("DC390W : ACB size= %4x, DCB size= %4x, SRB size= %4x\n",
	      sizeof(DC390W_ACB), sizeof(DC390W_DCB), sizeof(DC390W_SRB) );
#endif

	}
	else
	{
	    pSH_start = NULL;
	    scsi_unregister( psh );
	    return( -1 );
	}
	DC390W_download_script( psh );
	return( 0 );
    }
    else
    {
	printk("DC390W_init: EEPROM reading error!\n");
	return( -1 );
    }
}


void  MyDelay( void )
{
	UCHAR	i,j;

	j = inb(0x61) & 0x10;

	for(;;)
	{
	    i = inb(0x61) & 0x10;
	    if( j ^ i)
		break;
	}
}


void  EnDisableCE( UCHAR Flag, USHORT scsiIOPort )
{

    UCHAR bval;
    USHORT port;

    port = (scsiIOPort & 0xff00) + GPREG;
    if(Flag == ENABLE_CE)
	bval = 0x10;
    else
	bval = 0x00;
    outb(bval,port);
    udelay(8);	/* Delay();*/
}


void  EEpromOutDI( USHORT Carry, USHORT scsiIOPort )
{
    UCHAR  bval;
    USHORT port;

    port = (scsiIOPort & 0xff00) + GPREG;
    bval = 0x10;
    if(Carry)
	bval |= 0x02;	/* SK=0, DI */
    outb(bval,port);
    udelay(8);	/* Delay();*/
    bval |= 0x04;	/* SK=1, DI */
    outb(bval,port);
    udelay(8);	/* Delay();*/
    bval &= 0xfb;	/* SK=0, DI */
    outb(bval,port);
    udelay(8);	/* Delay();*/
}


void  EEpromPrepare( UCHAR EEpromCmd, USHORT scsiIOPort )
{
    UCHAR i,j;
    USHORT carryFlag;

    carryFlag = 1;
    j = 0x80;
    for(i=0;i<9;i++)
    {
	EEpromOutDI(carryFlag,scsiIOPort);
	carryFlag = (EEpromCmd & j) ? 1 : 0;
	j >>= 1;
    }
}


UCHAR  EEpromInDo(USHORT scsiIOPort)
{
    UCHAR  bval;
    USHORT port;

    port = (scsiIOPort & 0xff00) + GPREG;
    bval = 0x14;	/* SK=1 */
    outb(bval,port);
    udelay(8);	/* Delay();*/
    bval = 0x10;	/* SK=0 */
    outb(bval,port);
    udelay(8);	/* Delay();*/
    bval = inb(port);
    if(bval & 0x01)
	return( 1 );
    else
	return( 0 );
}


USHORT	EEpromGetData(USHORT scsiIOPort)
{
    UCHAR i;
    UCHAR carryFlag;
    USHORT wval;

    wval = 0;
    for(i=0;i<16;i++)
    {
	wval <<= 1;
	carryFlag = EEpromInDo(scsiIOPort);
	wval |= carryFlag;
    }
    return( wval );
}


void  ReadEEprom( PUCHAR EEpromBuf, USHORT scsiIOPort )
{
    UCHAR cmd;

    cmd = EEPROM_READ;
loop_rd:
    EnDisableCE(ENABLE_CE, scsiIOPort);
    EEpromPrepare(cmd, scsiIOPort);
    *((PUSHORT)EEpromBuf) = EEpromGetData(scsiIOPort);
    EEpromBuf++;
    EEpromBuf++;
    cmd++;
    EnDisableCE(DISABLE_CE, scsiIOPort);
    if(cmd & 0x3f)
       goto loop_rd;
}


USHORT	CheckEEpromCheckSum( PUCHAR EEpromBuf, USHORT scsiIOPort)
{
    USHORT wval,port, *ptr;
    UCHAR  i,bval;

    port = (scsiIOPort & 0xff00) + GPCNTL;
    bval = 0x09;			/* configure IO Pin */
    outb(bval,port);
    ReadEEprom(EEpromBuf,scsiIOPort);	/* read eeprom data */
    wval = 0;
    ptr = (PUSHORT) EEpromBuf;
    for(i=0; i<128 ;i+=2, ptr++)
	wval += *ptr;
    return( (wval == 0x1234) ? 0 : -1);
}



static void
DC390W_download_script (struct Scsi_Host *host)
{
    ULONG   wlval, wlval1, length, alignm;
    USHORT  j, k, m;
    USHORT  ioport;
    UCHAR   bval;
    PACB    pACB;
    PSRB    pSRB;
    void    *pSrc, *pSrc1;
    ULONG   *pStart;
    ULONG   Ent_reselected;
    ULONG   Ent_reselecttag;
    ULONG   Ent_select0;
    ULONG   Ent_select1;
    ULONG   Ent_check_phase;
    ULONG   Ent_status1_phase;
    ULONG   Ent_command_phase;
    ULONG   Ent_jump_table0;
    ULONG   Ent_din_phaseB;
    ULONG   Ent_dout_phaseB;
    ULONG   Ent_din_pad_0;
    ULONG   Ent_dout_pad_0;
    ULONG   Ent_jump_tablew;
    ULONG   Ent_din_pad_1;
    ULONG   Ent_dout_pad_1;
    ULONG   Ent_mout_phase;
    ULONG   Ent_status_phase;
    ULONG   Ent_min_phase;
    ULONG   Ent_jump_msgok;
    ULONG   Ent_msg__1;
    ULONG   Ent_msg___3;
    ULONG   Ent_msg___2;
    ULONG   Ent_set_atn;
    ULONG   Ent_msg__a;
    ULONG   Ent_msg__23;
    ULONG   Ent_msg__3;
    ULONG   Ent_msg__4;
    ULONG   Ent_clr_atn;
    ULONG   Ent_din_phaseW;
    ULONG   Ent_dout_phaseW;
    ULONG   Ent_din_pad_addrB;
    ULONG   Ent_dout_pad_addrB;
    ULONG   Ent_din_pad_addrW;
    ULONG   Ent_dout_pad_addrW;


    pACB = (PACB) host->hostdata;
    ioport = pACB->IOPortBase;
    bval = SCRATCHAB_AS_BASE;	 /* set scratchB contains 4K RAM base address */
    outb(bval,ioport+CTEST2);

    wlval = inl((ioport+SCRATCHB)); /* get starting address of 4K RAM */
/*  wlval += 0x800; */		 /* point to Upper 2K RAM */
    DesPhysAddr[0] = wlval;	 /* destination address */

#ifdef DC390W_DEBUG0
	printk("DesAddr=%8x,",(UINT) wlval);
#endif
    bval = 0;			 /* set Scratch_A and Scratch_B to normal mode */
    outb(bval,ioport+CTEST2);

   /*-------------------------------------------------------------------
    * patch the label in jump instruction: using offset relative
    * to start_script
    *------------------------------------------------------------------*/

    Ent_reselected	 = (ULONG) reselected - (ULONG) start_script;
    Ent_reselecttag	 = (ULONG) reselecttag - (ULONG) start_script;
    Ent_select0 	 = (ULONG) select0 - (ULONG) start_script;
    Ent_select1 	 = (ULONG) select1 - (ULONG) start_script;
    Ent_check_phase	 = (ULONG) check_phase - (ULONG) start_script;
    Ent_status1_phase	 = (ULONG) status1_phase - (ULONG) start_script;
    Ent_command_phase	 = (ULONG) command_phase - (ULONG) start_script;
    Ent_din_phaseB	 = (ULONG) din_phaseB - (ULONG) start_script;
    Ent_dout_phaseB	 = (ULONG) dout_phaseB - (ULONG) start_script;
    Ent_din_phaseW	 = (ULONG) din_phaseW - (ULONG) start_script;
    Ent_dout_phaseW	 = (ULONG) dout_phaseW - (ULONG) start_script;
    Ent_jump_table0	 = (ULONG) jump_table0 - (ULONG) start_script;
    Ent_din_pad_0	 = (ULONG) din_pad_0 - (ULONG) start_script;
    Ent_din_pad_addrB	 = (ULONG) din_pad_addrB - (ULONG) start_script;
    Ent_dout_pad_0	 = (ULONG) dout_pad_0 - (ULONG) start_script;
    Ent_dout_pad_addrB	 = (ULONG) dout_pad_addrB - (ULONG) start_script;
    Ent_jump_tablew	 = (ULONG) jump_tablew - (ULONG) start_script;
    Ent_din_pad_1	 = (ULONG) din_pad_1 - (ULONG) start_script;
    Ent_din_pad_addrW	 = (ULONG) din_pad_addrW - (ULONG) start_script;
    Ent_dout_pad_1	 = (ULONG) dout_pad_1 - (ULONG) start_script;
    Ent_dout_pad_addrW	 = (ULONG) dout_pad_addrW - (ULONG) start_script;
    Ent_mout_phase	 = (ULONG) mout_phase - (ULONG) start_script;
    Ent_status_phase	 = (ULONG) status_phase - (ULONG) start_script;
    Ent_min_phase	 = (ULONG) min_phase - (ULONG) start_script;
    Ent_jump_msgok	 = (ULONG) jump_msgok - (ULONG) start_script;
    Ent_msg__1		 = (ULONG) msg__1 - (ULONG) start_script;
    Ent_msg___3 	 = (ULONG) msg___3 - (ULONG) start_script;
    Ent_msg___2 	 = (ULONG) msg___2 - (ULONG) start_script;
    Ent_set_atn 	 = (ULONG) set_atn - (ULONG) start_script;
    Ent_msg__a		 = (ULONG) msg__a - (ULONG) start_script;
    Ent_msg__23 	 = (ULONG) msg__23 - (ULONG) start_script;
    Ent_msg__3		 = (ULONG) msg__3 - (ULONG) start_script;
    Ent_msg__4		 = (ULONG) msg__4 - (ULONG) start_script;
    Ent_clr_atn 	 = (ULONG) clr_atn - (ULONG) start_script;

    jmp_select0[0]	  = Ent_select0 + wlval;
    jmp_reselected[0]	  = Ent_reselected + wlval;
    jmp_check_phase[0]	  = Ent_check_phase + wlval;
    jmp_check_phase1[0]   = Ent_check_phase + wlval;
    jmp_check_phase2[0]   = Ent_check_phase + wlval;
    jmp_check_phase3[0]   = Ent_check_phase + wlval;
    jmp_check_phase4[0]   = Ent_check_phase + wlval;
    jmp_check_phase5[0]   = Ent_check_phase + wlval;
    jmp_check_phase6[0]   = Ent_check_phase + wlval;
    jmp_status1_phase[0]  = Ent_status1_phase + wlval;
    jmp_status1_phase1[0] = Ent_status1_phase + wlval;
    jmp_status1_phase2[0] = Ent_status1_phase + wlval;
    jmp_status1_phase3[0] = Ent_status1_phase + wlval;
    jmp_command_phase[0]  = Ent_command_phase + wlval;
    for(j=0,k=1,m=0; j< (MAX_SG_LIST_BUF+1); j++)
    {
	jmp_dio_phaseB[k] = Ent_din_phaseB + m + wlval;
	jmp_dio_phaseW[k] = Ent_din_phaseW + m + wlval;
	k += 2;
	jmp_dio_phaseB[k] = Ent_dout_phaseB + m + wlval;
	jmp_dio_phaseW[k] = Ent_dout_phaseW + m + wlval;
	k += 2;
	m += 8;
    }
    jmp_din_pad_0[0]	  = Ent_din_pad_0 + wlval;
    jmp_dout_pad_0[0]	  = Ent_dout_pad_0 + wlval;
    jmp_din_pad_addrB[0]  = Ent_din_pad_addrB + wlval;
    jmp_dout_pad_addrB[0] = Ent_dout_pad_addrB + wlval;
    jmp_din_pad_addrW[0]  = Ent_din_pad_addrW + wlval;
    jmp_dout_pad_addrW[0] = Ent_dout_pad_addrW + wlval;
    jmp_din_pad_1[0]	  = Ent_din_pad_1 + wlval;
    jmp_dout_pad_1[0]	  = Ent_dout_pad_1 + wlval;
    jmp_status_phase[0]   = Ent_status_phase + wlval;
    jmp_min_phase[0]	  = Ent_min_phase + wlval;
    jmp_mout_phase[0]	  = Ent_mout_phase + wlval;
    jmp_jump_msgok[0]	  = Ent_jump_msgok + wlval;
    jmp_msg__1[0]	  = Ent_msg__1 + wlval;
    jmp_msg___3[0]	  = Ent_msg___3 + wlval;
    jmp_msg___2[0]	  = Ent_msg___2 + wlval;
    jmp_msg__a[0]	  = Ent_msg__a + wlval;
    jmp_msg__a1[0]	  = Ent_msg__a + wlval;
    jmp_msg__a2[0]	  = Ent_msg__a + wlval;
    jmp_msg__23[0]	  = Ent_msg__23 + wlval;
    jmp_msg__3[0]	  = Ent_msg__3 + wlval;
    jmp_msg__4[0]	  = Ent_msg__4 + wlval;

   /*--------------------------------------------------------------------
   // patch the element in ACB struct: using Physical address
   //-------------------------------------------------------------------*/

#ifndef VERSION_ELF_1_2_13
    wlval1 = virt_to_phys( pACB->msgin123 );
#else
    wlval1 = (ULONG) pACB->msgin123;
#endif
    ACB_msgin123_1[0] = wlval1;
    ACB_msgin123_2[0] = wlval1;
    ACB_msgin123_3[0] = wlval1;
    ACB_msgin123_4[0] = wlval1;
    ACB_msgin123_5[0] = wlval1;
    ACB_msgin123_6[0] = wlval1;
    ACB_msgin123_7[0] = wlval1;

#ifndef VERSION_ELF_1_2_13
    ACB_status[0] = virt_to_phys( &pACB->status );
#else
    ACB_status[0] = (ULONG) &pACB->status;
#endif
   /*--------------------------------------------------------------------
   // patch the element in SRB struct: using offset in struct
   //-------------------------------------------------------------------*/

    pSRB = (PSRB) pACB->SRB_array;
    select1[0] = (select1[0] & 0xffff0000) + ((ULONG) &pSRB->__select - (ULONG) &pSRB->CmdBlock);
    SRB_msgout0[0]  = (ULONG) &pSRB->__msgout0 - (ULONG) &pSRB->CmdBlock;
    SRB_msgout01[0] = (ULONG) &pSRB->__msgout0 - (ULONG) &pSRB->CmdBlock;
    SRB_command[0] = (ULONG) &pSRB->__command - (ULONG) &pSRB->CmdBlock;
    SRB_SegmentPad[0]  = (ULONG) &pSRB->SegmentPad - (ULONG) &pSRB->CmdBlock;
    SRB_SegmentPad1[0] = (ULONG) &pSRB->SegmentPad - (ULONG) &pSRB->CmdBlock;
    SRB_SegmentPad2[0] = (ULONG) &pSRB->SegmentPad - (ULONG) &pSRB->CmdBlock;
    SRB_SegmentPad3[0] = (ULONG) &pSRB->SegmentPad - (ULONG) &pSRB->CmdBlock;
    wlval = (ULONG) &pSRB->Segment0 - (ULONG) &pSRB->CmdBlock;
    for(j=0,k=1; j<(MAX_SG_LIST_BUF+1); j++)
    {
	 din_phaseB[k]	= wlval;
	 dout_phaseB[k] = wlval;
	 din_phaseW[k]	= wlval;
	 dout_phaseW[k] = wlval;
	 k += 2;
	 wlval += 8;
    }


    bval = inb(ioport+DCNTL);
    bval |= IRQ_DISABLE;
    outb(bval,ioport+DCNTL);  /* disable interrupt */

/*  pSrc = scsi_init_malloc( 2048, GFP_ATOMIC); */
    pSrc = scsi_init_malloc( 4096, GFP_ATOMIC); /* 1.11 */
#ifdef DC390W_DEBUG0
	printk("SrcAlloc=%8x,",(UINT) pSrc);
#endif
    alignm = 4 - (((ULONG) pSrc) & 3);
    pSrc1 = (void *)(((ULONG) pSrc) + alignm);
    length = (ULONG) end_script - (ULONG) start_script;
    memcpy( pSrc1, (void *) start_script, length);
    pStart = (ULONG *) ((ULONG) start_mov - (ULONG) start_script);
    pStart =(ULONG *) (((ULONG) pStart) + ((ULONG) pSrc1));

#ifdef DC390W_DEBUG0
	printk("SrcAddr=%8x,\n",(UINT) pSrc1);
#endif
#ifndef VERSION_ELF_1_2_13
    (ULONG *)pStart[1] = virt_to_phys( pSrc1 );
#else
    (ULONG *)pStart[1] = (ULONG) pSrc1;
#endif

/* wlval = virt_to_phys( start_script ); */ /* physical address of start_script */
/* SrcPhysAddr[0] = wlval; */	/* sources address */

/* start to download SCRIPT instruction to the RAM of NCR53c825A,875 */

/* wlval = virt_to_phys( start_mov ); */

#ifndef VERSION_ELF_1_2_13
   wlval = virt_to_phys( pStart );
#else
   wlval = (ULONG) pStart;
#endif

   outl(wlval,ioport+DSP);

   bval = inb(ioport+ISTAT);
   while(!(bval & DMA_INT_PENDING))	/* check load start_script is finished? */
       bval = inb(ioport+ISTAT);

   bval = inb(ioport+DSTAT); /* clear interrupt */

   bval = inb(ioport+DCNTL);
   bval &= ~IRQ_DISABLE;
   outb(bval,ioport+DCNTL); /* re-enable interrupt */

   scsi_init_free((char *) pSrc, 4096);

   wlval = DesPhysAddr[0];	/* starting addr of RAM */
   wlval -=  (ULONG) start_script;

   pACB->jmp_reselect	 = wlval  + (ULONG) start_script;
   pACB->jmp_select	 = wlval  + (ULONG) select1;
   pACB->jmp_table8	 = wlval  + (ULONG) jump_table0;
   pACB->jmp_set_atn	 = wlval  + (ULONG) set_atn;
   pACB->jmp_clear_ack	 = wlval  + (ULONG) msg__a;
   pACB->jmp_next	 = wlval  + (ULONG) check_phase;
   pACB->jmp_din8	 = wlval  + (ULONG) din_phaseB+8;
   pACB->jmp_dout8	 = wlval  + (ULONG) dout_phaseB+8;
   pACB->jmp_clear_atn	 = wlval  + (ULONG) clr_atn;
   pACB->jmp_reselecttag = wlval  + (ULONG) reselecttag;

   wlval = pACB->jmp_reselect;
   outl(wlval,(ioport+DSP));
   return;
}


/***********************************************************************
 * Function : int DC390W_detect(Scsi_Host_Template *psht)
 *
 * Purpose : detects and initializes NCR53c825A,875 SCSI chips
 *	     that were autoprobed, overridden on the LILO command line,
 *	     or specified at compile time.
 *
 * Inputs : psht - template for this SCSI adapter
 *
 * Returns : number of host adapters detected
 *
 ***********************************************************************/

int
DC390W_detect(Scsi_Host_Template *psht)
{
    UCHAR   pci_bus, pci_device_fn, irq;
#ifndef VERSION_ELF_1_2_13
    UINT    io_port, ram_base;
#else
    ULONG   io_port, ram_base;
#endif
    USHORT  i;
    int     error = 0;
    USHORT  adaptCnt = 0;	/* Number of boards detected */
    USHORT  pci_index = 0;	/* Device index to PCI BIOS calls */
    USHORT  pci_index2 = 0;	/* Device index to PCI BIOS calls */
    USHORT  chipType = 0;


#ifndef VERSION_ELF_1_2_13
    psht->proc_dir = &proc_scsi_tmscsiw;
#endif

    InitialTime = 1;
    pSHT_start = psht;
    jmp_table16 = (ULONG) jump_tablew - (ULONG) jump_table0;
    jmp_din16 = (ULONG) din_phaseW - (ULONG) din_phaseB;
    jmp_dout16 = (ULONG) dout_phaseW - (ULONG) dout_phaseB;
    pACB_start = NULL;

    if ( pcibios_present() )
    {
	for (i = 0; i < MAX_ADAPTER_NUM; ++i)
	{
	    if( !pcibios_find_device( PCI_VENDOR_ID_NCR,
				PCI_DEVICE_ID_NCR53C825A,
				pci_index, &pci_bus, &pci_device_fn) )
	    {
		chipType = PCI_DEVICE_ID_NCR53C825A;
		pci_index++;
	    }
	    else if( !pcibios_find_device( PCI_VENDOR_ID_NCR,
				PCI_DEVICE_ID_NCR53C875,
				pci_index2, &pci_bus, &pci_device_fn) )
	    {
		chipType = PCI_DEVICE_ID_NCR53C875;
		pci_index2++;
	    }

	    if( chipType )
	    {
		error = pcibios_read_config_dword(pci_bus, pci_device_fn,
					PCI_BASE_ADDRESS_0, &io_port);
		error |= pcibios_read_config_dword(pci_bus, pci_device_fn,
					PCI_BASE_ADDRESS_2, &ram_base);
		error |= pcibios_read_config_byte(pci_bus, pci_device_fn,
						  PCI_INTERRUPT_LINE, &irq);
		if( error )
		{
		    printk("DC390W_detect: reading configuration registers error!\n");
		    InitialTime = 0;
		    return( 0 );
		}

		(USHORT) io_port = (USHORT) io_port & 0xFFFE;
#ifdef DC390W_DEBUG0
		printk("DC390W : IO_PORT=%4x,RAM_BASE=%8x,IRQ=%x,CHIPID=%x,\n",
		      (UINT) io_port, (UINT) ram_base, irq, (UCHAR)chipType);
#endif

		if( !DC390W_init(psht, chipType, io_port, irq, i) )
		    adaptCnt++;
		chipType = 0;
	    }
	    else
		break;
	}
    }
    InitialTime = 0;
    adapterCnt = adaptCnt;
    return( adaptCnt );
}

#ifndef VERSION_ELF_1_2_13

/********************************************************************
 * Function: tmscsiw_set_info()
 *
 * Purpose: Set adapter info (!)
 *
 * Not yet implemented
 *
 *******************************************************************/

int tmscsiw_set_info(char *buffer, int length, struct Scsi_Host *shpnt)
{
  return(-ENOSYS);  /* Currently this is a no-op */
}

/********************************************************************
 * Function: tmscsiw_proc_info(char* buffer, char **start,
 *			     off_t offset, int length, int hostno, int inout)
 *
 * Purpose: return SCSI Adapter/Device Info
 *
 * Input: buffer: Pointer to a buffer where to write info
 *	  start :
 *	  offset:
 *	  hostno: Host adapter index
 *	  inout : Read (=0) or set(!=0) info
 *
 * Output: buffer: contains info
 *	   length; length of info in buffer
 *
 * return value: length
 *
 ********************************************************************/

/* KG: proc_info taken from driver aha152x.c */

#undef SPRINTF
#define SPRINTF(args...) pos += sprintf(pos, ## args)

#define YESNO(YN)\
if (YN) SPRINTF(" Yes ");\
else SPRINTF(" No  ")

int tmscsiw_proc_info(char *buffer, char **start,
		      off_t offset, int length, int hostno, int inout)
{
  int dev, spd, spd1;
  char *pos = buffer;
  PSH shpnt;
  PACB acbpnt;
  PDCB dcbpnt;
  unsigned long flags;
/*  Scsi_Cmnd *ptr; */

  acbpnt = pACB_start;

  while(acbpnt != (PACB)-1)
     {
	shpnt = acbpnt->pScsiHost;
	if (shpnt->host_no == hostno) break;
	acbpnt = acbpnt->pNextACB;
     }

  if (acbpnt == (PACB)-1) return(-ESRCH);
  if (!shpnt) return(-ESRCH);

  if(inout) /* Has data been written to the file ? */
    return(tmscsiw_set_info(buffer, length, shpnt));

  SPRINTF("Tekram DC390W/U/F (T) PCI SCSI Host Adadpter, ");
  SPRINTF("Driver Version 1.12, 1997/02/17\n");

  save_flags(flags);
  cli();

  SPRINTF("SCSI Host Nr %i, ", hostno);
  SPRINTF("DC390WUF Adapter Nr %i\n", acbpnt->AdapterIndex);
  SPRINTF("IOPortBase 0x%04x, ", acbpnt -> IOPortBase);
  SPRINTF("IRQLevel 0x%02x\n",acbpnt -> IRQLevel);

  SPRINTF("Adapter Type: ");
  switch(acbpnt->AdaptType)
     {
      case DC390W: SPRINTF("DC390W, Fast Wide SCSI \n"); break;
      case DC390U: SPRINTF("DC390U, Ultra SCSI\n"); break;
      case DC390F: SPRINTF("DC390F, Ultra Wide SCSI\n"); break;
      default: SPRINTF("Unknown !\n");
     }

  SPRINTF("MaxID %i, MaxLUN %i, ", acbpnt->max_id, acbpnt->max_lun);
  SPRINTF("AdapterID %i, AdapterLUN %i\n", acbpnt->AdaptSCSIID, acbpnt->AdaptSCSILUN);

  SPRINTF("TagMaxNum %i, Status %i\n", acbpnt->TagMaxNum, acbpnt->status);

  SPRINTF("Nr of attached devices: %i\n", acbpnt->DeviceCnt);

  SPRINTF("Un ID LUN Prty Sync DsCn SndS TagQ Wide NegoPeriod SyncSpeed SyncOffs\n");
  dcbpnt = acbpnt->pLinkDCB;

  for (dev = 0; dev < acbpnt->DeviceCnt; dev++)
     {
      SPRINTF("%02i %02i  %02i ", dev, dcbpnt->UnitSCSIID, dcbpnt->UnitSCSILUN);
      YESNO(dcbpnt->DevMode & PARITY_CHK_);
      YESNO(dcbpnt->DCBsxfer & OFFSET_MASK);
      YESNO(dcbpnt->DevMode & EN_DISCONNECT_);
      YESNO(dcbpnt->DevMode & SEND_START_);
      YESNO(dcbpnt->MaxCommand > 1);
      YESNO(dcbpnt->DCBscntl3 & EN_WIDE_SCSI);
      SPRINTF("  %03i ns ", (dcbpnt->NegoPeriod) << 2);
      if (dcbpnt->DCBsxfer & OFFSET_MASK)
      {
	 spd = 1000/(dcbpnt->SyncPeriod <<2);
	 spd1 = 1000%(dcbpnt->SyncPeriod <<2);
	 spd1 = (spd1 * 10)/(dcbpnt->SyncPeriod <<2);
	 SPRINTF("   %2i.%1i M      %02i\n", spd, spd1, dcbpnt->DCBsxfer & OFFSET_MASK);
      }
      else SPRINTF("\n");
      /* Add more info ...*/
      dcbpnt = dcbpnt->pNextDCB;
     }

  restore_flags(flags);
  *start = buffer + offset;

  if (pos - buffer < offset)
    return 0;
  else if (pos - buffer - offset < length)
    return pos - buffer - offset;
  else
    return length;
}
#endif /* VERSION_ELF_1_2_13 */

#ifdef MODULE

/***********************************************************************
 * Function : static int DC390W_shutdown (struct Scsi_Host *host)
 *
 * Purpose : does a clean (we hope) shutdown of the NCR SCSI chip.
 *	     Use prior to dumping core, unloading the NCR driver, etc.
 *
 * Returns : 0 on success
 ***********************************************************************/
static int
DC390W_shutdown (struct Scsi_Host *host)
{
    USHORT   ioport;
    unsigned long flags;
    PACB pACB = (PACB) host->hostdata;

    ioport = (unsigned int) pACB->IOPortBase;

    save_flags (flags);
    cli();

/*  pACB->soft_reset(host); */
/*
 * For now, we take the simplest solution : reset the SCSI bus. Eventually,
 * - If a command is connected, kill it with an ABORT message
 * - If commands are disconnected, connect to each target/LUN and
 *	do a ABORT, followed by a SOFT reset, followed by a hard
 *	reset.
 */

#ifdef DC390W_DEBUG0
    printk("DC390W: shutdown,");
#endif
    outb(ASSERT_RST, ioport+SCNTL1);
    udelay(25); /* Minimum amount of time to assert RST */
    outb(0, ioport+SCNTL1);
    restore_flags (flags);
    return( 0 );
}


int DC390W_release(struct Scsi_Host *host)
{
    int irq_count;
    struct Scsi_Host *tmp;

    DC390W_shutdown (host);

    if (host->irq != IRQ_NONE)
    {
	for (irq_count = 0, tmp = pSH_start; tmp; tmp = tmp->next)
	{
	    if ( tmp->irq == host->irq )
		++irq_count;
	}
	if (irq_count == 1)
	 {
#ifdef DC390W_DEBUG0
	    printk("DC390W: Free IRQ %i.",host->irq);
#endif
#ifndef VERSION_ELF_1_2_13
	    free_irq(host->irq,NULL);
#else
	    free_irq(host->irq);
#endif
	 }
    }

    release_region(host->io_port,host->n_io_port);

   return( 1 );
}

Scsi_Host_Template driver_template = DC390WUF;
#include "scsi_module.c"
#endif /* def MODULE */

