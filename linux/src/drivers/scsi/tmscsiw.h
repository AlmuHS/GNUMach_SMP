/***********************************************************************
;*	File Name : TMSCSIW.H					       *
;*		    TEKRAM DC-390 PCI Wide SCSI Bus Master Host Adapter*
;*		    Device Driver				       *
;***********************************************************************/

#ifndef TMSCSIW_H
#define TMSCSIW_H

#define IRQ_NONE 255

typedef unsigned char	UCHAR;
typedef unsigned short	USHORT;
typedef unsigned long	ULONG;
typedef unsigned int	UINT;

typedef UCHAR		*PUCHAR;
typedef USHORT		*PUSHORT;
typedef ULONG		*PULONG;
typedef Scsi_Host_Template  *PSHT;
typedef struct Scsi_Host    *PSH;
typedef Scsi_Device	*PSCSIDEV;
typedef Scsi_Cmnd	*PSCSICMD;
typedef void		*PVOID;
typedef struct scatterlist  *PSGL, SGL;


typedef  struct  _AddWkSpace
{
USHORT		WKIOCtrlFlag;		/* ;b7-Done, b6-wrtVerify, b0-in process */
USHORT		XferredBlkCnt;
USHORT		WKToXferBlkCnt;
USHORT		WKcSGListDone;
USHORT		WKMaxSGIndex;
ULONG		WKToXferLen;
USHORT		NxtSGoffset;
} AddWkSpace;

/*;-----------------------------------------------------------------------*/
typedef  struct  _SyncMsg
{
UCHAR		ExtendMsg;
UCHAR		ExtMsgLen;
UCHAR		SyncXferReq;
UCHAR		Period;
UCHAR		ReqOffset;
} SyncMsg;
/*;-----------------------------------------------------------------------*/
typedef  struct  _Capacity
{
ULONG		BlockCount;
ULONG		BlockLength;
} Capacity;
/*;-----------------------------------------------------------------------*/
typedef  struct  _SGentry
{
ULONG		SGXferDataPtr;
ULONG		SGXferDataLen;
} SGentry;

typedef  struct  _SGentry1
{
ULONG		SGXLen;
ULONG		SGXPtr;
} SGentry1, *PSGE;


#define MAX_ADAPTER_NUM 	4
#define MAX_DEVICES		10
#define MAX_SG_LIST_BUF 	32
#define MAX_CMD_QUEUE		20
#define MAX_CMD_PER_LUN 	8
#define MAX_SCSI_ID		16
#define MAX_SRB_CNT		MAX_CMD_QUEUE+4
#define END_SCAN		2

/*
;-----------------------------------------------------------------------
; SCSI Request Block
;-----------------------------------------------------------------------
*/
struct	_SRB
{
UCHAR		CmdBlock[12];
ULONG		Segment0[MAX_SG_LIST_BUF+1][2];
ULONG		SegmentPad[2];

ULONG		__select;
ULONG		__command[2];	/* ;len,ptr */
ULONG		__msgout0[2];	/* ;len,ptr */

ULONG		PhysSRB;
ULONG		ReturnAddr;
ULONG		RemainSegPtr;
PSCSICMD	pcmd;
struct _SRB	*pNextSRB;
struct _DCB	*pSRBDCB;
PSGL		pSegmentList;
ULONG		XferredLen;

ULONG		SGPhysAddr;	/*;a segment starting address */
ULONG		XferredLen1;

SGL		Segmentx;	/* make a one entry of S/G list table */

PUCHAR		pMsgPtr;
USHORT		SRBState;
USHORT		Revxx2; 	/* ??? */

UCHAR		MsgInBuf[6];
UCHAR		MsgOutBuf[6];
UCHAR		SenseDataBuf[0x12];
UCHAR		AdaptStatus;
UCHAR		TargetStatus;

UCHAR		MsgCnt;
UCHAR		EndMessage;
UCHAR		TagNumber;
UCHAR		InternalReq;	/*; 1-ADD internal request, 0-DMD request */

UCHAR		SGcount;
UCHAR		SGIndex;
UCHAR		IORBFlag;	/*;81h-Reset, 2-retry */
UCHAR		SRBStatus;

UCHAR		RetryCnt;
UCHAR		SRBFlag;	/*; b0-AutoReqSense,b6-Read,b7-write */
				/*; b4-settimeout,b5-Residual valid */
UCHAR		ScratchABuf;
UCHAR		Reserved3[1];	/*;for dword alignment */
};

typedef  struct  _SRB	 DC390W_SRB, *PSRB;

/*
;-----------------------------------------------------------------------
; Device Control Block
;-----------------------------------------------------------------------
*/
struct	_DCB
{
UCHAR		DCBselect;
UCHAR		DCBsxfer;
UCHAR		DCBsdid;
UCHAR		DCBscntl3;

UCHAR		DCBscntl0;
UCHAR		IdentifyMsg;
UCHAR		DevMode;
UCHAR		AdpMode;

struct _DCB	*pNextDCB;
struct _ACB	*pDCBACB;

PSCSICMD	pQIORBhead;
PSCSICMD	pQIORBtail;
PSCSICMD	AboIORBhead;
PSCSICMD	AboIORBtail;
USHORT		QIORBCnt;
USHORT		AboIORBcnt;

PSRB		pWaitingSRB;
PSRB		pWaitLast;
PSRB		pGoingSRB;
PSRB		pGoingLast;
PSRB		pActiveSRB;
USHORT		GoingSRBCnt;
USHORT		WaitSRBCnt;	/* ??? */

ULONG		TagMask;

USHORT		MaxCommand;
USHORT		AdaptIndex;	/*; UnitInfo struc start */
USHORT		UnitIndex;	/*; nth Unit on this card */
UCHAR		UnitSCSIID;	/*; SCSI Target ID  (SCSI Only) */
UCHAR		UnitSCSILUN;	/*; SCSI Log.  Unit (SCSI Only) */

UCHAR		InqDataBuf[8];
UCHAR		CapacityBuf[8];
UCHAR		SyncMode;	/*; 0:async mode */
UCHAR		NegoPeriod;	/*;for nego. */
UCHAR		SyncPeriod;	/*;for reg. */
UCHAR		SyncOffset;	/*;for reg. and nego.(low nibble) */
UCHAR		UnitCtrlFlag;
UCHAR		DCBFlag;
UCHAR		DevType;
UCHAR		Reserved2[1];	/*;for dword alignment */
};

typedef  struct  _DCB	 DC390W_DCB, *PDCB;
/*
;-----------------------------------------------------------------------
; Adapter Control Block
;-----------------------------------------------------------------------
*/
struct	_ACB
{
ULONG		PhysACB;
ULONG		DevVendorID;
PSH		pScsiHost;
struct _ACB	*pNextACB;
USHORT		IOPortBase;
USHORT		Revxx1; 	/* ??? */

PDCB		pLinkDCB;
PDCB		pDCBRunRobin;
PDCB		pActiveDCB;
PDCB		pDCB_free;
PSRB		pFreeSRB;
USHORT		SRBCount;
USHORT		AdapterIndex;	/*; nth Adapter this driver */
USHORT		max_id;
USHORT		max_lun;

ULONG		jmp_reselect;
ULONG		jmp_select;
ULONG		jmp_table8;
ULONG		jmp_set_atn;
ULONG		jmp_clear_ack;
ULONG		jmp_next;
ULONG		jmp_din8;
ULONG		jmp_dout8;
ULONG		jmp_clear_atn;
ULONG		jmp_reselecttag;

UCHAR		msgin123[4];
UCHAR		status;
UCHAR		AdaptSCSIID;	/*; Adapter SCSI Target ID */
UCHAR		AdaptSCSILUN;	/*; Adapter SCSI LUN */
UCHAR		DeviceCnt;
UCHAR		IRQLevel;
UCHAR		AdaptType;	/*;1:390W, 2:390U, 3:390F */
UCHAR		TagMaxNum;
UCHAR		ACBFlag;
UCHAR		Gmode2;
UCHAR		LUNchk;
UCHAR		scan_devices;
UCHAR		Reserved1[1];	/*;for dword alignment */
UCHAR		DCBmap[MAX_SCSI_ID];
DC390W_DCB	DCB_array[MAX_DEVICES]; 	/* +74h,  Len=3E8 */
DC390W_SRB	SRB_array[MAX_SRB_CNT]; 	/* +45Ch, Len=	*/
};

typedef  struct  _ACB	 DC390W_ACB, *PACB;

/*;-----------------------------------------------------------------------*/

#define PCI_DEVICE_ID_NCR53C825A	0x0003
#define PCI_DEVICE_ID_NCR53C875 	0x000f

#define CHIP810_ID	0x00011000
#define CHIP820_ID	0x00021000
#define CHIP825_ID	0x00031000
#define CHIP815_ID	0x00041000
#define CHIP875_ID	0x000f1000

#define DC390W		1	/*;825A - 16 BIT*/
#define DC390U		2	/*;875	-  8 BIT*/
#define DC390F		3	/*;875	- 16 BIT*/

#define BIT31	0x80000000
#define BIT30	0x40000000
#define BIT29	0x20000000
#define BIT28	0x10000000
#define BIT27	0x08000000
#define BIT26	0x04000000
#define BIT25	0x02000000
#define BIT24	0x01000000
#define BIT23	0x00800000
#define BIT22	0x00400000
#define BIT21	0x00200000
#define BIT20	0x00100000
#define BIT19	0x00080000
#define BIT18	0x00040000
#define BIT17	0x00020000
#define BIT16	0x00010000
#define BIT15	0x00008000
#define BIT14	0x00004000
#define BIT13	0x00002000
#define BIT12	0x00001000
#define BIT11	0x00000800
#define BIT10	0x00000400
#define BIT9	0x00000200
#define BIT8	0x00000100
#define BIT7	0x00000080
#define BIT6	0x00000040
#define BIT5	0x00000020
#define BIT4	0x00000010
#define BIT3	0x00000008
#define BIT2	0x00000004
#define BIT1	0x00000002
#define BIT0	0x00000001

/*;---WKIOCtrlFlag */
#define IN_PROCESSING	BIT0
#define BAD_WRITE_V_CMD BIT1
#define VERIFY_CMD	BIT2
#define RESIDUAL_SG	BIT3

/*;---UnitCtrlFlag */
#define UNIT_ALLOCATED	BIT0
#define UNIT_INFO_CHANGED BIT1
#define FORMATING_MEDIA BIT2
#define UNIT_RETRY	BIT3

/*;---UnitFlags */
#define DASD_SUPPORT	BIT0
#define SCSI_SUPPORT	BIT1
#define ASPI_SUPPORT	BIT2

/*;----SRBState machine definition */
#define SRB_FREE	0
#define SRB_WAIT	BIT0
#define SRB_READY	BIT1
#define SRB_MSGOUT	BIT2	/*;arbitration+msg_out 1st byte*/
#define SRB_MSGIN	BIT3
#define SRB_MSGIN_MULTI BIT4
#define SRB_COMMAND	BIT5
#define SRB_START_	BIT6	/*;arbitration+msg_out+command_out*/
#define SRB_DISCONNECT	BIT7
#define SRB_DATA_XFER	BIT8
#define SRB_XFERPAD	BIT9
#define SRB_STATUS	BIT10
#define SRB_COMPLETED	BIT11

#define DO_WIDE_NEGO	BIT12
#define DO_SYNC_NEGO	BIT13
#define SRB_UNEXPECT_RESEL BIT14

/*;---ACBFlag */
#define RESET_DEV	BIT0
#define RESET_DETECT	BIT1
#define RESET_DONE	BIT2

/*;---DCBFlag */
#define ABORT_DEV_	BIT0

/*;---SRBstatus */
#define SRB_OK		BIT0
#define ABORTION	BIT1
#define OVER_RUN	BIT2
#define UNDER_RUN	BIT3
#define SRB_ERROR	BIT4

/*;---SRBFlag */
#define DATAOUT 	BIT7
#define DATAIN		BIT6
#define RESIDUAL_VALID	BIT5
#define ENABLE_TIMER	BIT4
#define RESET_DEV0	BIT2
#define ABORT_DEV	BIT1
#define AUTO_REQSENSE	BIT0

/*;---Adapter status */
#define H_STATUS_GOOD	 0
#define H_SEL_TIMEOUT	 0x11
#define H_OVER_UNDER_RUN 0x12
#define H_UNEXP_BUS_FREE 0x13
#define H_TARGET_PHASE_F 0x14
#define H_INVALID_CCB_OP 0x16
#define H_LINK_CCB_BAD	 0x17
#define H_BAD_TARGET_DIR 0x18
#define H_DUPLICATE_CCB  0x19
#define H_BAD_CCB_OR_SG  0x1A
#define H_ABORT 	 0x0FF

/*; SCSI Status byte codes*/
#define SCSI_STAT_GOOD		0x0	/*;  Good status */
#define SCSI_STAT_CHECKCOND	0x02	/*;  SCSI Check Condition */
#define SCSI_STAT_CONDMET	0x04	/*;  Condition Met */
#define SCSI_STAT_BUSY		0x08	/*;  Target busy status */
#define SCSI_STAT_INTER 	0x10	/*;  Intermediate status */
#define SCSI_STAT_INTERCONDMET	0x14	/*;  Intermediate condition met */
#define SCSI_STAT_RESCONFLICT	0x18	/*;  Reservation conflict */
#define SCSI_STAT_CMDTERM	0x22	/*;  Command Terminated */
#define SCSI_STAT_QUEUEFULL	0x28	/*;  Queue Full */

#define SCSI_STAT_UNEXP_BUS_F	0xFD	/*;  Unexpect Bus Free */
#define SCSI_STAT_BUS_RST_DETECT 0xFE	/*;  Scsi Bus Reset detected */
#define SCSI_STAT_SEL_TIMEOUT	0xFF	/*;  Selection Time out */

/*;---Sync_Mode */
#define SYNC_DISABLE	0
#define SYNC_ENABLE	BIT0
#define SYNC_NEGO_DONE	BIT1
#define WIDE_ENABLE	BIT2
#define WIDE_NEGO_DONE	BIT3
#define EN_TAG_QUEUING	BIT4

#define SYNC_NEGO_OFFSET 16

/*;---SCSI bus phase*/
#define SCSI_DATA_OUT	0
#define SCSI_DATA_IN	1
#define SCSI_COMMAND	2
#define SCSI_STATUS_	3
#define SCSI_NOP0	4
#define SCSI_NOP1	5
#define SCSI_MSG_OUT	6
#define SCSI_MSG_IN	7

/*;----SCSI MSG BYTE*/
#define MSG_COMPLETE		0x00
#define MSG_EXTENDED		0x01
#define MSG_SAVE_PTR		0x02
#define MSG_RESTORE_PTR 	0x03
#define MSG_DISCONNECT		0x04
#define MSG_INITIATOR_ERROR	0x05
#define MSG_ABORT		0x06
#define MSG_REJECT_		0x07
#define MSG_NOP 		0x08
#define MSG_PARITY_ERROR	0x09
#define MSG_LINK_CMD_COMPL	0x0A
#define MSG_LINK_CMD_COMPL_FLG	0x0B
#define MSG_BUS_RESET		0x0C
#define MSG_ABORT_TAG		0x0D
#define MSG_SIMPLE_QTAG 	0x20
#define MSG_HEAD_QTAG		0x21
#define MSG_ORDER_QTAG		0x22
#define MSG_IDENTIFY		0x80
#define MSG_HOST_ID		0x0C0

/*;----SCSI STATUS BYTE*/
#define STATUS_GOOD		0x00
#define CHECK_CONDITION_	0x02
#define STATUS_BUSY		0x08
#define STATUS_INTERMEDIATE	0x10
#define RESERVE_CONFLICT	0x18

/* cmd->result */
#define STATUS_MASK_		0xFF
#define MSG_MASK		0xFF00
#define RETURN_MASK		0xFF0000

/*
**  Inquiry Data format
*/

typedef struct	_SCSIInqData { /* INQ */

	UCHAR	 DevType;		/* Periph Qualifier & Periph Dev Type*/
	UCHAR	 RMB_TypeMod;		/* rem media bit & Dev Type Modifier */
	UCHAR	 Vers;			/* ISO, ECMA, & ANSI versions	     */
	UCHAR	 RDF;			/* AEN, TRMIOP, & response data format*/
	UCHAR	 AddLen;		/* length of additional data	     */
	UCHAR	 Res1;			/* reserved			     */
	UCHAR	 Res2;			/* reserved			     */
	UCHAR	 Flags; 		/* RelADr,Wbus32,Wbus16,Sync,etc.    */
	UCHAR	 VendorID[8];		/* Vendor Identification	     */
	UCHAR	 ProductID[16]; 	/* Product Identification	     */
	UCHAR	 ProductRev[4]; 	/* Product Revision		     */


} SCSI_INQDATA, *PSCSI_INQDATA;


/*  Inquiry byte 0 masks */


#define SCSI_DEVTYPE	    0x1F      /* Peripheral Device Type 	    */
#define SCSI_PERIPHQUAL     0xE0      /* Peripheral Qualifier		    */


/*  Inquiry byte 1 mask */

#define SCSI_REMOVABLE_MEDIA  0x80    /* Removable Media bit (1=removable)  */


/*  Peripheral Device Type definitions */

#define SCSI_DASD		 0x00	   /* Direct-access Device	   */
#define SCSI_SEQACESS		 0x01	   /* Sequential-access device	   */
#define SCSI_PRINTER		 0x02	   /* Printer device		   */
#define SCSI_PROCESSOR		 0x03	   /* Processor device		   */
#define SCSI_WRITEONCE		 0x04	   /* Write-once device 	   */
#define SCSI_CDROM		 0x05	   /* CD-ROM device		   */
#define SCSI_SCANNER		 0x06	   /* Scanner device		   */
#define SCSI_OPTICAL		 0x07	   /* Optical memory device	   */
#define SCSI_MEDCHGR		 0x08	   /* Medium changer device	   */
#define SCSI_COMM		 0x09	   /* Communications device	   */
#define SCSI_NODEV		 0x1F	   /* Unknown or no device type    */

/*
** Inquiry flag definitions (Inq data byte 7)
*/

#define SCSI_INQ_RELADR       0x80    /* device supports relative addressing*/
#define SCSI_INQ_WBUS32       0x40    /* device supports 32 bit data xfers  */
#define SCSI_INQ_WBUS16       0x20    /* device supports 16 bit data xfers  */
#define SCSI_INQ_SYNC	      0x10    /* device supports synchronous xfer   */
#define SCSI_INQ_LINKED       0x08    /* device supports linked commands    */
#define SCSI_INQ_CMDQUEUE     0x02    /* device supports command queueing   */
#define SCSI_INQ_SFTRE	      0x01    /* device supports soft resets */


/*
;==========================================================
; EEPROM byte offset
;==========================================================
*/
typedef  struct  _EEprom
{
UCHAR	EE_MODE1;
UCHAR	EE_SPEED;
UCHAR	xx1;
UCHAR	xx2;
} EEprom, *PEEprom;

#define EE_ADAPT_SCSI_ID 64
#define EE_MODE2	65
#define EE_DELAY	66
#define EE_TAG_CMD_NUM	67

/*; EE_MODE1 bits definition*/
#define PARITY_CHK_	BIT0
#define SYNC_NEGO_	BIT1
#define EN_DISCONNECT_	BIT2
#define SEND_START_	BIT3
#define TAG_QUEUING_	BIT4
#define WIDE_NEGO_	BIT5

/*; EE_MODE2 bits definition*/
#define MORE2_DRV	BIT0
#define GREATER_1G	BIT1
#define RST_SCSI_BUS	BIT2
#define ACTIVE_NEGATION BIT3
#define NO_SEEK 	BIT4
#define LUN_CHECK	BIT5

#define ENABLE_CE	1
#define DISABLE_CE	0
#define EEPROM_READ	0x80

/*
;==========================================================
;	NCR 53C825 Registers Structure
;==========================================================
*/
typedef  struct  _OperatingReg
{
UCHAR	SCSICtrl0;	/*; (+00)*/
UCHAR	SCSICtrl1;	/*; (+01)*/
UCHAR	SCSICtrl2;	/*; (+02)*/
UCHAR	SCSICtrl3;	/*; (+03)*/
UCHAR	SCSIChipID;	/*; (+04)*/
UCHAR	SCSIXfer;	/*; (+05)*/
UCHAR	SCSIDestID;	/*; (+06)*/
UCHAR	GeneralPReg;	/*; (+07)*/
UCHAR	SCSI1stByteR;	/*; (+08)*/
UCHAR	SCSIOPCtrlL;	/*; (+09)*/
UCHAR	SCSISelID;	/*; (+0A)*/
UCHAR	SCSIBusCtrlLin; /*; (+0B)*/
UCHAR	DMAStatus;	/*; (+0C)*/
UCHAR	SCSIStatus0;	/*; (+0D)*/
UCHAR	SCSIStatus1;	/*; (+0E)*/
UCHAR	SCSIStatus2;	/*; (+0F)*/
ULONG	DataStrucAddr;	/*; (+10)*/
UCHAR	InterruptStatus;/*; (+14)*/
UCHAR	Reser1; 	/*; (+15)*/
UCHAR	Reser2; 	/*; (+16)*/
UCHAR	Reser3; 	/*; (+17)*/
UCHAR	ChipTest0;	/*; (+18)*/
UCHAR	ChipTest1;	/*; (+19)*/
UCHAR	ChipTest2;	/*; (+1A)*/
UCHAR	ChipTest3;	/*; (+1B)*/
ULONG	TempStack;	/*; (+1C)*/
UCHAR	DMAFIFO;	/*; (+20)*/
UCHAR	ChipTest4;	/*; (+21)*/
UCHAR	ChipTest5;	/*; (+22)*/
UCHAR	ChipTest6;	/*; (+23)*/
UCHAR	DMAByteCnt[3];	/*; (+24)*/
UCHAR	DMACommand;	/*; (+27)*/
ULONG	DMANextAddr;	/*; (+28)*/
ULONG	DMAScriptsPtr;	/*; (+2C)*/
ULONG	DMAScriptPtrSav;/*; (+30)*/
ULONG	ScratchA;	/*; (+34)*/
UCHAR	DMAMode;	/*; (+38)*/
UCHAR	DMAIntEnable;	/*; (+39)*/
UCHAR	DMAWatchDog;	/*; (+3A)*/
UCHAR	DMACtrl;	/*; (+3B)*/
ULONG	ADDer;		/*; (+3C)*/
UCHAR	SCSIIntEnable0; /*; (+40)*/
UCHAR	SCSIIntEnable1; /*; (+41)*/
UCHAR	SCSIIntStatus0; /*; (+42)*/
UCHAR	SCSIIntStatus1; /*; (+43)*/
UCHAR	SCSILongParity; /*; (+44)*/
UCHAR	WideResiData;	/*; (+45)*/
UCHAR	MemAccessCtrl;	/*; (+46)*/
UCHAR	GeneralPCtrl;	/*; (+47)*/
UCHAR	SCSITimer0;	/*; (+48)*/
UCHAR	SCSITimer1;	/*; (+49)*/
UCHAR	ResponseID0;	/*; (+4A)*/
UCHAR	ResponseID1;	/*; (+4B)*/
UCHAR	SCSITest0;	/*; (+4C)*/
UCHAR	SCSITest1;	/*; (+4D)*/
UCHAR	SCSITest2;	/*; (+4E)*/
UCHAR	SCSITest3;	/*; (+4F)*/
USHORT	SCSIIPDataL;	/*; (+50)*/
USHORT	Reser4; 	/*; (+52)*/
USHORT	SCSIOPDataL;	/*; (+54)*/
USHORT	Reser5; 	/*; (+56)*/
USHORT	SCSIBusDataLin; /*; (+58)*/
USHORT	Reser6; 	/*; (+5A)*/
ULONG	ScratchB;	/*; (+5C)*/
ULONG	ScratchC;	/*; (+60)*/
ULONG	ScratchD;	/*; (+64)*/
ULONG	ScratchE;	/*; (+68)*/
ULONG	ScratchF;	/*; (+6C)*/
ULONG	ScratchG;	/*; (+70)*/
ULONG	ScratchH;	/*; (+74)*/
ULONG	ScratchI;	/*; (+78)*/
ULONG	ScratchJ;	/*; (+7C)*/
} OperatingReg;
/*
;==========================================================
;	NCR 53C825 Registers bit Definition
;==========================================================
*/
/*; SCSICtrl0	      (+00)*/
#define FULL_ARBITRATION   (BIT7+BIT6)
#define START_ARBIT	   BIT5
#define SEL_W_SATN	   BIT4
#define EN_PARITY_CHK	   BIT3
#define SATN_IF_PARITY_ERR BIT1
#define TARGET_MODE	   BIT0

/*; SCSICtrl1	      (+01)*/
#define ASSERT_RST	BIT3
#define ASSERT_EVEN_P	BIT2
#define START_SCSI_XFER BIT0

/*; SCSICtrl2	      (+02)*/
#define DISC_UNEXPECTED BIT7
#define CHAIN_MODE	BIT6
#define WIDE_SCSI_SEND	BIT3
#define WIDE_SCSI_RECV	BIT0

/*; SCSICtrl3	      (+03)*/
#define EN_FAST_20	BIT7
#define SYNC_CLK_F4	(BIT6+BIT4)
#define SYNC_CLK_F3	BIT6
#define SYNC_CLK_F2	(BIT5+BIT4)
#define SYNC_CLK_F1_5	BIT5
#define SYNC_CLK_F1	BIT4
#define EN_WIDE_SCSI	BIT3
#define ASYNC_CLK_F4	(BIT2+BIT0)
#define ASYNC_CLK_F3	BIT2
#define ASYNC_CLK_F2	(BIT1+BIT0)
#define ASYNC_CLK_F1_5	BIT1
#define ASYNC_CLK_F1	BIT0

/*; SCSIChipID	      (+04)*/
#define ENABLE_RESEL	BIT6
#define ENABLE_SEL	BIT5
#define CHIP_ID_MASK	0FH

/*; SCSIXfer	      (+05)*/
#define PERIOD_MASK	(BIT7+BIT6+BIT5)
#define SYNC_PERIOD_F11 (BIT7+BIT6+BIT5)
#define SYNC_PERIOD_F10 (BIT7+BIT6)
#define SYNC_PERIOD_F9	(BIT7+BIT5)
#define SYNC_PERIOD_F8	BIT7
#define SYNC_PERIOD_F7	(BIT6+BIT5)
#define SYNC_PERIOD_F6	BIT6
#define SYNC_PERIOD_F5	BIT5
#define SYNC_PERIOD_F4	0
#define OFFSET_MASK	(BIT4+BIT3+BIT2+BIT1+BIT0)
#define SYNC_OFFSET_16	BIT4
#define SYNC_OFFSET_8	BIT3
#define SYNC_OFFSET_7	(BIT2+BIT1+BIT0)
#define SYNC_OFFSET_6	(BIT2+BIT1)
#define SYNC_OFFSET_5	(BIT2+BIT0)
#define SYNC_OFFSET_4	BIT2
#define SYNC_OFFSET_3	(BIT1+BIT0)
#define SYNC_OFFSET_2	BIT1
#define SYNC_OFFSET_1	BIT0
#define SYNC_OFFSET_0	0
#define ASYNCHRONOUS	SYNC_OFFSET_0
/*
; SCSIDestID	    (+06)

; GeneralPReg	    (+07)

; SCSI1stByteR	    (+08)
*/
/*; SCSIOPCtrlL       (+09)*/
#define ASSERT_REQ	BIT7
#define ASSERT_ACK	BIT6
#define ASSERT_BSY	BIT5
#define ASSERT_SEL	BIT4
#define ASSERT_ATN	BIT3
#define ASSERT_MSG	BIT2
#define ASSERT_C_D	BIT1
#define ASSERT_I_O	BIT0

/*; SCSISelID	      (+0A)	  Read*/
#define SCSI_ID_VALID	BIT7

/*; SCSIBusCtrlLin    (+0B)	  Read*/
#define REQ_SIGNAL	BIT7
#define ACK_SIGNAL	BIT6
#define BSY_SIGNAL	BIT5
#define SEL_SIGNAL	BIT4
#define ATN_SIGNAL	BIT3
#define MSG_SIGNAL	BIT2
#define C_D_SIGNAL	BIT1
#define I_O_SIGNAL	BIT0

/*; DMAStatus	      (+0C)	  Read*/
#define DMA_FIFO_EMPTY	BIT7
#define MASTER_PARITY_ERR BIT6
#define BUS_FAULT	BIT5
#define ABORT_		BIT4
#define SINGLE_STEP_INT BIT3
#define SCRIPTS_INT	BIT2
#define ILLEGAL_INSTRUC BIT0

/*; SCSIStatus0       (+0D)	  Read*/
#define SIDL_LSB_FULL	BIT7
#define SODR_LSB_FULL	BIT6
#define SODL_LSB_FULL	BIT5
#define IN_ARBITRATION	BIT4
#define LOST_ARBITRATION BIT3
#define WIN_ARBITRATION BIT2
#define RST_SIGNAL	BIT1
#define PARITY_SIGNAL	BIT0

/*; SCSIStatus1       (+0E)	  Read*/
#define SCSI_FIFO_MASK	(BIT7+BIT6+BIT5+BIT4)

/*; SCSIStatus2       (+0F)	  Read*/
#define SIDL_MSB_FULL	BIT7
#define SODR_MSB_FULL	BIT6
#define SODL_MSB_FULL	BIT5
#define SCSI_FIFO_MSB	BIT4

/*; DataStrucAddr     (+10)*/

/*; InterruptStatus   (+14)*/
#define ABORT_OP	BIT7
#define SOFTWARE_RST	BIT6
#define SIGNAL_PROC	BIT5
#define SEMAPHORE	BIT4
#define CONNECTED	BIT3
#define INT_ON_FLY	BIT2
#define SCSI_INT_PENDING BIT1
#define DMA_INT_PENDING BIT0

/*; ChipTest0	      (+18)*/
/*; ChipTest1	      (+19)*/
#define FIFO_BYTE_EMPTY (BIT7+BIT6+BIT5+BIT4)
#define FIFO_BYTE_FULL	(BIT3+BIT2+BIT1+BIT0)

/*; ChipTest2	      (+1A)*/
#define READ_DIR	BIT7
#define WRITE_DIR	0
#define SIGNAL_PROC_	BIT6
#define CFG_AS_IO_MAP	BIT5
#define CFG_AS_MEM_MAP	BIT4
#define SCRATCHAB_AS_BASE BIT3
#define TRUE_EOP	BIT2
#define INTERNAL_DREQ	BIT1
#define INTERNAL_DACK	BIT0

/*; ChipTest3	      (+1B)*/
#define CHIP_REV_MASK	(BIT7+BIT6+BIT5+BIT4)
#define FLUSH_DMA_FIFO	BIT3
#define CLR_DMA_FIFO	BIT2
#define FETCH_PIN_MODE	BIT1
#define WRT_EN_INVALIDATE BIT0

/*; TempStack	      (+1C)*/

/*; DMAFIFO	      (+20)*/
/*; 2 upper bits in ChipTest5*/

/*; ChipTest4	      (+21)*/
#define BURST_DISABLE	BIT7

/*; ChipTest5	      (+22) */
#define EN_DMA_FIFO_536 BIT5
#define BURST_LEN_MSB	BIT2
#define DMAFIFO_MS2B	(BIT1+BIT0)
/*
; ChipTest6	    (+23)
; DMAByteCnt	    (+24)
; DMACommand	    (+27)
; DMANextAddr	    (+28)
; DMAScriptsPtr     (+2C)
; DMAScriptPtrSav   (+30)
*/
/*; ScratchA	      (+34)*/
#define COMPLETED_OK_	BIT0
#define RE_SELECTED_	BIT1
#define OVER_RUN_	BIT2

/*; DMAMode	      (+38)*/
#define BURST_LEN16	(BIT7+BIT6)
#define BURST_LEN8	BIT7
#define BURST_LEN4	BIT6
#define BURST_LEN2	0
#define SRC_IO_MAP	BIT5
#define SRC_MEM_MAP	0
#define DEST_IO_MAP	BIT4
#define DEST_MEM_MAP	0
#define EN_READ_LINE	BIT3
#define EN_READ_MULTIPLE BIT2
#define BURST_OPCODE_FETCH BIT1
#define MANUAL_START	BIT0
#define AUTO_START	0

/*; DMAIntEnable      (+39)*/
#define EN_MDPE 	BIT6
#define EN_BUS_FAULT	BIT5
#define EN_ABORTED	BIT4
#define EN_SINGLE_STEP	BIT3
#define EN_SCRIPT_INT	BIT2
#define EN_ILLEGAL_INST BIT0


/*; DMAWatchDog       (+3A)*/

/*; DMACtrl	      (+3B)*/
#define EN_CACHE_LINE_SIZE BIT7
#define PRE_FETCH_FLUSH BIT6
#define EN_PRE_FETCH	BIT5
#define SINGLE_STEP_MIDE BIT4
#define TOTEM_POLE_IRQ	BIT3
#define OPEN_DRAIN_IRQ	0
#define START_DMA	BIT2
#define IRQ_DISABLE	BIT1
#define COMPATIBLE_700	BIT0

/*; ADDer	      (+3C)*/

/*; SCSIIntEnable0    (+40)*/
#define EN_PHASE_MISMATCH BIT7
#define EN_ARB_SEL_DONE BIT6
#define EN_BE_SELECTED	BIT5
#define EN_BE_RESELECTED BIT4
#define EN_SCSI_GERROR	BIT3
#define EN_UNEXPECT_DISC BIT2
#define EN_SCSI_RESET	BIT1
#define EN_PARITY_ERROR BIT0

/*; SCSIIntEnable1    (+41)*/
#define EN_SEL_TIMEOUT	BIT2
#define EN_GENERAL_TIMEOUT BIT1
#define EN_HANDSHAKE_TIMEOUT  BIT0

/*; SCSIIntStatus0    (+42)*/
#define PHASE_MISMATCH	BIT7
#define ARB_SEL_DONE	BIT6
#define BE_SELECTED	BIT5
#define BE_RESELECTED	BIT4
#define SCSI_GERROR	BIT3
#define UNEXPECT_DISC	BIT2
#define SCSI_RESET	BIT1
#define PARITY_ERROR	BIT0

/*; SCSIIntStatus1    (+43)*/
#define SEL_TIMEOUT	BIT2
#define GENERAL_TIMEOUT BIT1
#define HANDSHAKE_TIMEOUT BIT0
/*
; SCSILongParity    (+44)
; WideResiData	    (+45)
; MemAccessCtrl     (+46)
; GeneralPCtrl	    (+47)
*/
/*; SCSITimer0	      (+48)*/
#define HTH_TO_DISABLE	0
#define SEL_TO_204ms	(BIT3+BIT2)
/*
; SCSITimer1	    (+49)
; ResponseID0	    (+4A)
; ResponseID1	    (+4B)
; SCSITest0	    (+4C)
; SCSITest1	    (+4D)
; SCSITest2	    (+4E)
*/
/*; SCSITest3	      (+4F)*/
#define ACTIVE_NEGATION_ BIT7
#define FIFO_TEST_READ	BIT6
#define HALT_SCSI_CLK	BIT5
#define DIS_SINGLE_INIT BIT4
#define TIMER_TEST_MODE BIT2
#define CLR_SCSI_FIFO	BIT1
#define FIFO_TEST_WRITE BIT0
/*
; SCSIIPDataL	    (+50)
; SCSIOPDataL	    (+54)
; SCSIBusDataLin    (+58)
; ScratchB	    (+5C)
; ScratchC	    (+60)
; ScratchD	    (+64)
; ScratchE	    (+68)
; ScratchF	    (+6C)
; ScratchG	    (+70)
; ScratchH	    (+74)
; ScratchI	    (+78)
; ScratchJ	    (+7C)
*/

/***********************************************************************
 * NCR53C825  I/O Address Map
 * base address is stored at offset 0x10 in the PCI configuration space
 *-----------------------------------------------------------------------
 *	SCSI Core Registers Offset
 ***********************************************************************/
#define  SCNTL0    0x00   /* Scsi Control 0		    R/W */
#define  SCNTL1    0x01   /* Scsi Control 1		    R/W */
#define  SCNTL2    0x02   /* Scsi Control 2		    R/W */
#define  SCNTL3    0x03   /* Scsi Control 3		    R/W */
#define  SCID	   0x04   /* Scsi Chip ID		    R/W */
#define  SXFER	   0x05   /* Scsi Transfer		    R/W */
#define  SDID	   0x06   /* Scsi Destination ID	    R/W */
#define  GPREG	   0x07   /* General Purpose Bits	    R/W */
#define  SFBR	   0x08   /* Scsi First Byte Received	    R/W */
#define  SOCL	   0x09   /* Scsi Output Control Latch	    R/W */
#define  SSID	   0x0A   /* Scsi Selector ID		    R	*/
#define  SBCL	   0x0B   /* Scsi Bus Control Lines	    R/W */
#define  DSTAT	   0x0C   /* DMA Status 		    R	*/
#define  SSTAT0    0x0D   /* Scsi Status 0		    R	*/
#define  SSTAT1    0x0E   /* Scsi Status 1		    R	*/
#define  SSTAT2    0x0F   /* Scsi Status 2		    R	*/
#define  DSA	   0x10   /* Data Structure Address	    R/W */
#define  ISTAT	   0x14   /* Interrupt Status		    R/W */
#define  RES15	   0x15   /* Reserved				*/
#define  CTEST0    0x18   /* Reserved				*/
#define  CTEST1    0x19   /* Chip Test 1		    R/W */
#define  CTEST2    0x1A   /* Chip Test 2		    R	*/
#define  CTEST3    0x1B   /* Chip Test 3		    R	*/
#define  TEMP	   0x1C   /* Temperary Stack		    R/W */
#define  DFIFO	   0x20   /* DMA FIFO			    R/W */
#define  CTEST4    0x21   /* Chip Test 4		    R/W */
#define  CTEST5    0x22   /* Chip Test 5		    R/W */
#define  CTEST6    0x23   /* Chip Test 6		    R/W */
#define  DBC	   0x24   /* DMA Byte Counter		    R/W */
#define  DCMD	   0x27   /* DMA Command		    R/W */
#define  DNAD	   0x28   /* DMA Next Address For Data	    R/W */
#define  DSP	   0x2C   /* DMA SCIPTS Pointer 	    R/W */
#define  DSPS	   0x30   /* DMA SCIPTS Pointer Saves	    R/W */
#define  SCRATCHA  0x34   /* General Purpose Scratch Pad A  R/W */
#define  DMODE	   0x38   /* DMA Mode			    R/W */
#define  DIEN	   0x39   /* DMA Interrupt Enable	    R/W */
#define  DWT	   0x3A   /* DMA Watchdog Timer 	    R/W */
#define  DCNTL	   0x3B   /* DMA Control		    R/W */
#define  ADDER	   0x3C   /* Sum output of Internal adder   R	*/
#define  SIEN0	   0x40   /* Scsi Interrupt Enable 0	    R/W */
#define  SIEN1	   0x41   /* Scsi Interrupt Enable 1	    R/W */
#define  SIST0	   0x42   /* Scsi Interrupt Status 0	    R	*/
#define  SIST1	   0x43   /* Scsi Interrupt Status 1	    R	*/
#define  SLPAR	   0x44   /* Scsi Longitudinal Parity	    R/W */
#define  SWIDE	   0x45   /* Reserved				*/
#define  MACNTL    0x46   /* Memory Access Control	    R/W */
#define  GPCNTL    0x47   /* General Purpose Control	    R/W */
#define  STIME0    0x48   /* Scsi Timer 0		    R/W */
#define  STIME1    0x49   /* Scsi Timer 1		    R/W */
#define  RESPID0   0x4A   /* Response ID 0		    R/W */
#define  RESPID1   0x4B   /* Response ID 1		    R/W */
#define  STEST0    0x4C   /* Scsi Test 0		    R	*/
#define  STEST1    0x4D   /* Scsi Test 1		    R	*/
#define  STEST2    0x4E   /* Scsi Test 2		    R/W */
#define  STEST3    0x4F   /* Scsi Test 3		    R/W */
#define  SIDL	   0x50   /* Scsi Input Data Latch	    R	*/
#define  RES51	   0x51   /* Reserved				*/
#define  SODL	   0x54   /* Scsi Output Data Latch	    R/W */
#define  RES55	   0x55   /* Reserved				*/
#define  SBDL	   0x58   /* Scsi Bus Data Lines	    R	*/
#define  RES59	   0x59   /* Reserved				*/
#define  SCRATCHB  0x5C   /* General Purpose Scratch Pad B  R/W */


/*
;==========================================================
; structure for 53C825A register ( I/O )
;==========================================================
*/
#define __scntl0	0
#define __scntl1	1
#define __scntl2	2
#define __scntl3	3
#define __scid		4
#define __sxfer 	5
#define __sdid		6
#define __gpreg 	7
#define __sfbr		8
#define __socl		9
#define __ssid		0x0A
#define __sbcl		0x0B
#define __dstat 	0x0C
#define __sstat0	0x0D
#define __sstat1	0x0E
#define __sstat2	0x0F
#define __dsa		0x10
#define __istat 	0x14
/*#define		0x15*/
#define __ctest0	0x18
#define __ctest1	0x19
#define __ctest2	0x1A
#define __ctest3	0x1B
#define __temp		0x1C
#define __dfifo 	0x20
#define __ctest4	0x21
#define __ctest5	0x22
#define __ctest6	0x23
#define __dbc		0x24
#define __dcmd		0x27
#define __dnad		0x28
#define __dsp		0x2C
#define __dsps		0x30
#define __scratcha	0x34
#define __dmode 	0x38
#define __dien		0x39
#define __dwt		0x3A
#define __dcntl 	0x3B
#define __adder 	0x3C
#define __sien0 	0x40
#define __sien1 	0x41
#define __sist0 	0x42
#define __sist1 	0x43
#define __slpar 	0x44
#define __swide 	0x45
#define __mactrl	0x46
#define __gpctrl	0x47
#define __stime0	0x48
#define __stime1	0x49
#define __respid0	0x4A
#define __respid1	0x4B
#define __stest0	0x4C
#define __stest1	0x4D
#define __stest2	0x4E
#define __stest3	0x4F
#define __sidl		0x50   /*; For wide SCSI, this register*/
/*#define		0x51	 ; contains 2 bytes*/
#define __sodl		0x54
/*#define		0x55*/
#define __sbdl		0x58
/*#define		0x59*/
#define __scratchb	0x5C
/*
;==========================================================
; Script interrupt code(Vector)
;==========================================================
*/
#define __COMPLETE	0	/*; command complete*/
#define __RESELECTED	1	/*; reselected, without MESSAGE*/
#define __RESELECTED1	2	/*; reselected with idenntify bytes*/
#define __RESELECTEDT	3	/*; reselected with TAGS*/
#define __DISCONNECTED	4	/*; disconnected*/
#define __MSGEXTEND	5	/*; Extended msgin*/
#define __SIGNAL	6	/*; signaled, need to clear SIG*/
#define __MSGUNKNOWN	7	/*; Unknown message*/
#define __MSGOUT	8	/*; Message out phase detected*/
#define __FATALERROR	9	/*; Fatal error*/
#define __MSGSYNC	10	/*; Sync nego input*/
#define __MSGWIDE	11	/*; Wide nego input*/
#define __RESTOREPTR	12	/*; restore pointer received*/
#define __MSGREJECT	13	/*; Reject message received*/
#define __DEBUG 	14	/*; For debug*/


#define DC390W_read8(address)				\
	inb(DC390W_ioport + (address)))

#define DC390W_read16(address)				\
	inw(DC390W_ioport + (address)))

#define DC390W_read32(address)				\
	inl(DC390W_ioport + (address)))

#define DC390W_write8(address,value)			\
	outb((value), DC390W_ioport + (address)))

#define DC390W_write16(address,value)			\
	outw((value), DC390W_ioport + (address)))

#define DC390W_write32(address,value)			\
	outl((value), DC390W_ioport + (address)))

#endif /* TMSCSIW_H */
