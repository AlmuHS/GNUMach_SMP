/***********************************************************************
;*	File Name : SCRIPTS.H					       *
;*	Description:SCRIPT language for NCR53c825A,875 SCRIPT processor*
;*								       *
;***********************************************************************

;==========================================================
; NCR 53C810,53C815,53C820,53C825,53C825A,53C860,53C875
; Script language definition for assembly programming
;==========================================================

;==========================================================
;	DMA Command
;==========================================================
*/
#define DCMD_BLOCK_MOVE      0
#define DCMD_IO 	    0x040000000  /*;BIT30 */
#define DCMD_RD_WRT	    0x040000000  /*;BIT30 */
#define DCMD_XFER_CTRL	    0x080000000  /*;BIT31 */
#define DCMD_MEM_MOVE	    0x0C0000000  /*;(BIT31+BIT30) */
#define DCMD_LOAD_STORE     0x0E0000000  /*;(BIT31+BIT30+BIT29) */
/*;==========================================================*/
#define INDIRECT_ADDR	     0x20000000  /*;BIT29 */
#define TABLE_INDIRECT	     0x10000000  /*;BIT28 */
#define BLOCK_MOVE	     0x08000000  /*;BIT27 */
#define CHAIN_MOVE	     0
/*; SCSI phase definition */
#define DATA_OUT_	     0x00000000  /*;data out phase */
#define DATA_IN_	     0x01000000  /*;BIT24	    ; data in phase */
#define COMMAND_	     0x02000000  /*;BIT25	    ; command phase */
#define STATUS_ 	     0x03000000  /*;(BIT25+BIT24)   ; status phase */
#define RESERVED_OUT	     0x04000000  /*;BIT26 */
#define RESERVED_IN	     0x05000000  /*;(BIT26+BIT24) */
#define MSG_OUT_	     0x06000000  /*;(BIT26+BIT25)   ; message in phase */
#define MSG_IN_ 	     0x07000000  /*;(BIT26+BIT25+BIT24);message out phase */
/*;----------------------------------------------------------*/
#define DCMD_SELECT	     0x40000000  /*;DCMD_IO+0 */
#define DCMD_SELECT_ATN      0x41000000  /*;(DCMD_IO+BIT24) */
#define DCMD_WAIT_DISC	     0x48000000  /*;(DCMD_IO+BIT27) */
#define DCMD_WAIT_RESEL      0x50000000  /*;(DCMD_IO+BIT28) */
#define DCMD_SET_CARRY	     0x58000400  /*;(DCMD_IO+BIT28+BIT27+BIT10) */
#define DCMD_SET_ACK	     0x58000040  /*;(DCMD_IO+BIT28+BIT27+BIT6) */
#define DCMD_SET_ATN	     0x58000008  /*;(DCMD_IO+BIT28+BIT27+BIT3) */
#define DCMD_CLR_CARRY	     0x60000400  /*;(DCMD_IO+BIT29+BIT10) */
#define DCMD_CLR_ACK	     0x60000040  /*;(DCMD_IO+BIT29+BIT6) */
#define DCMD_CLR_ATN	     0x60000008  /*;(DCMD_IO+BIT29+BIT3) */
#define RELATIVE_ADDR	     0x04000000  /*;BIT26 */
#define IO_TABLE_INDIR	     0x02000000  /*;BIT25 */
/*;----------------------------------------------------------*/
#define MOVE_FROM_SFBR	     0x68000000  /*;(DCMD_RD_WRT+BIT29+BIT27) */
#define MOVE_TO_SFBR	     0x70000000  /*;(DCMD_RD_WRT+BIT29+BIT28) */
#define RD_MODIFY_WRT	     0x78000000  /*;(DCMD_RD_WRT+BIT29+BIT28+BIT27) */
#define OP_MOVE_DATA	     0
#define OP_SHIFT_LEFT_C      0x01000000  /*;BIT24 */
#define OP_OR		     0x02000000  /*;BIT25 */
#define OP_XOR		     0x03000000  /*;(BIT25+BIT24) */
#define OP_AND		     0x04000000  /*;BIT26 */
#define OP_SHIFT_RIGHT_C     0x05000000  /*;(BIT26+BIT24) */
#define OP_ADD_DATA	     0x06000000  /*;(BIT26+BIT25) */
#define OP_ADD_DATA_C	     0x07000000  /*;(BIT26+BIT25+BIT24) */
#define USE_SFBR	     0x00800000  /*;BIT23 */
/*;----------------------------------------------------------*/
#define DCMD_JUMP	     0x80000000  /*;DCMD_XFER_CTRL+0 */
#define DCMD_CALL	     0x88000000  /*;(DCMD_XFER_CTRL+BIT27) */
#define DCMD_RETURN	     0x90000000  /*;(DCMD_XFER_CTRL+BIT28) */
#define DCMD_INT	     0x98000000  /*;(DCMD_XFER_CTRL+BIT28+BIT27) */
#define RELATIVE_	     0x00800000  /*;BIT23 */
#define IF_CARRY	     0x00200000  /*;BIT21 */
#define INT_ON_FLY_	     0x00100000  /*;BIT20 */
#define IF_TRUE 	     0x00080000  /*;BIT19 */
#define IF_NOT		     0
#define DATA_CMP	     0x00040000  /*;BIT18 */
#define PHASE_CMP	     0x00020000  /*;BIT17 */
#define WAIT_PHASE_VALID     0x00010000  /*;BIT16 */
/*;----------------------------------------------------------*/
#define DSA_RELATIVE	     0x10000000  /*;BIT28 */
#define FLUSH_PREFETCH	     0x02000000  /*;BIT25 */
#define DCMD_LOAD	    0x0E1000000  /*;(DCMD_LOAD_STORE+BIT24) */
#define DCMD_STORE	    0x0E0000000  /*;DCMD_LOAD_STORE */
/*
;==========================================================
; SCSI message EQUATES
;==========================================================
*/
#define CMD_COMPLETE	     0
#define EXT_MSG 	     1
#define SAVE_PTR	     2
#define RESTORE_PTR	     3
#define DISCONNECTMSG	     4
#define INITIATOR_ERR	     5
#define ABORTMSG	     6
#define MSG_REJECT	     7
#define NOPMSG		     8
#define MSG_PARITY	     9
#define LINK_CMD_CPL	     0x0a
#define LINK_CMD_FLAG	     0x0b
#define RESET_DEVICE	     0x0c
#define IDENTIFYMSG	     0x80
#define SIMPLE_TAG	     0x20
#define IGNORE_WIDE_RES      0x23
/*
;==========================================================
; Operation assumption
; 1. If phase mismatch during Xfer PAD ==> do nothing
;    Else compute FIXUP needed
; 2. After phase mismatch ==> Set to Xfer PAD
; 3. At disconnection ==> Modify return address
; 4. 1st restore ptr after reselection is ignored
; 5. If Xfer PAD is done ==> Error
;==========================================================
*/
/*	static	start_script
	static	reselected
	static	reselecttag
	static	select0
	static	select1
	static	check_phase
	static	status1_phase
	static	command_phase
	static	jump_table0
	static	jump_tableB
	static	din_phase
	static	din_phaseB
	static	din_pad_0
	static	din_pad_addrB
	static	dout_phase
	static	dout_phaseB
	static	dout_pad_0
	static	dout_pad_addrB
	static	jump_tablew
	static	jump_tableW
	static	din_phase1
	static	din_phaseW
	static	din_pad_1
	static	din_pad_addrW
	static	dout_phase1
	static	dout_phaseW
	static	dout_pad_1
	static	dout_pad_addrW
	static	mout_phase
	static	status_phase
	static	min_phase
	static	set_atn
	static	clr_atn
	static	end_script
	static	start_mov
	static	SrcPhysAddr
	static	DesPhysAddr
*/
ULONG  start_script[]={
/*
;==========================================================
; Wait for reselection
;==========================================================
*/
	DCMD_WAIT_RESEL
	};
ULONG  jmp_select0[]={
	0	/* offset select0 */
	};
ULONG  reselected[]={
	RD_MODIFY_WRT+OP_OR+0x200+0x340000, /* (2 shl 8) or (__scratcha shl 16) */
	0,

	DCMD_INT+WAIT_PHASE_VALID+IF_NOT+PHASE_CMP+MSG_IN_,
	__RESELECTED,

	BLOCK_MOVE+MSG_IN_+1	/* ;move in ID byte */
	};
ULONG  ACB_msgin123_1[]={
	0,	/* offset ACB.msgin123,*/

	DCMD_INT+IF_TRUE,
	__RESELECTED1
	};
ULONG  reselecttag[]={
	DCMD_CLR_ACK,
	0,

	BLOCK_MOVE+MSG_IN_+2	/* ;move 2 msg bytes */
	};
ULONG  ACB_msgin123_2[]={
	0,	/* offset ACB.msgin123,*/

	DCMD_INT+IF_TRUE,
	__RESELECTEDT
	};
/*
;==========================================================
; Select
; Case 1 : Only identify message is to be sent
; Case 2 : Synchronous negotiation is requested
;==========================================================
*/
ULONG  select0[]={
	DCMD_INT+IF_TRUE,
	__SIGNAL
	};
ULONG  select1[]={		   /* ; Select with ATN */

	DCMD_SELECT_ATN+IO_TABLE_INDIR	/* +offset SRB.__select ;4200h or 0100H */
	};
ULONG  jmp_reselected[]={
	0,	/* offset reselected, */

	DCMD_JUMP+WAIT_PHASE_VALID+IF_NOT+PHASE_CMP+MSG_OUT_
	};
ULONG  jmp_check_phase[]={
	0,	/* offset check_phase, */

	TABLE_INDIRECT+BLOCK_MOVE+MSG_OUT_
	};
ULONG  SRB_msgout0[]={
	0	/* offset SRB.__msgout0 */
	};
ULONG  check_phase[]={
	DCMD_RETURN+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,

	DCMD_RETURN+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0
	};
ULONG  status1_phase[]={
	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+STATUS_
	};
ULONG  jmp_status_phase[]={
	0,	/* offset status_phase,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+COMMAND_
	};
ULONG  jmp_command_phase[]={
	0,	/* offset command_phase, */

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+MSG_IN_
	};
ULONG  jmp_min_phase[]={
	0,	/* offset min_phase,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+MSG_OUT_
	};
ULONG  jmp_mout_phase[]={
	0,	/* offset mout_phase,*/

	DCMD_INT+IF_TRUE,
	__FATALERROR
	};
/*
;==========================================================
; Command phase
;==========================================================
*/
ULONG  command_phase[]={
	DCMD_CLR_ATN,
	0,
	TABLE_INDIRECT+BLOCK_MOVE+COMMAND_
	};
ULONG  SRB_command[]={
	0,	/* offset SRB.__command,*/

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_check_phase1[]={
	0	/* offset check_phase */
	};
/*
;==========================================================
; Data phase jump table for 8 bit operation
;==========================================================
*/
ULONG  jmp_dio_phaseB[]={
	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   0,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  0,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   8,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  8,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   16,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  16,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   24,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  24,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   32,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  32,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   40,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  40,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   48,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  48,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   56,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  56,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   64,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  64,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   72,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  72,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   80,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  80,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   88,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  88,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   96,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  96,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   104,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  104,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   112,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  112,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   120,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  120,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   0,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  0,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   8,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  8,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   16,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  16,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   24,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  24,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   32,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  32,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   40,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  40,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   48,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  48,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   56,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  56,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   64,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  64,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   72,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  72,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   80,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  80,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   88,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  88,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   96,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  96,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   104,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  104,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   112,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  112,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   120,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseB+  120,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseB+   128,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0   /* offset dout_phaseB+  128 */
	};
ULONG  jump_table0[]={
	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_
	};
ULONG  jmp_din_pad_0[]={
	0,	/* offset din_pad_0,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_
	};
ULONG  jmp_dout_pad_0[]={
	0	/* offset dout_pad_0 */
	};

#define jump_tableB	jump_table0
/*
;==========================================================
; Data in phase
;==========================================================
*/
ULONG  din_phaseB[]={
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment0,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment1,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment2,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment3,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment4,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment5,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment6,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment7,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment8,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment9,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment10,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment11,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment12,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment13,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment14,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment15,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment0,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment1,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment2,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment3,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment4,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment5,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment6,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment7,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment8,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment9,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment10,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment11,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment12,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment13,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment14,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment15,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_,
	0, /* offset SRB.Segment16,*/

	RD_MODIFY_WRT+OP_OR+0x100+0x340000, /*;(1 shl 8) or (__scratcha shl 16)*/
	0,

	DCMD_JUMP+WAIT_PHASE_VALID+IF_NOT+PHASE_CMP+DATA_IN_
	};
ULONG  jmp_status1_phase[]={
	0	/* offset status1_phase */
	};

#define din_phase	din_phaseB

ULONG  din_pad_0[]={
	RD_MODIFY_WRT+OP_OR+0x340000+0x400,  /*;(4 shl 8) or (__scratcha shl 16)*/
	0
	};
ULONG  din_pad_addrB[]={
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_IN_
	};
ULONG  SRB_SegmentPad[]={
	0,	/* offset SRB.SegmentPad,*/

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_din_pad_addrB[]={
	0	/* offset din_pad_addrB */
	};
/*
;==========================================================
; Data out phase
;==========================================================
*/
ULONG  dout_phaseB[]={
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment0,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment1,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment2,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment3,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment4,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment5,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment6,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment7,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment8,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment9,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment10,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment11,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment12,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment13,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment14,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment15,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment0,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment1,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment2,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment3,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment4,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment5,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment6,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment7,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment8,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment9,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment10,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment11,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment12,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment13,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment14,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment15,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment16,*/

	RD_MODIFY_WRT+OP_OR+0x100+0x340000, /*;(1 shl 8) or (__scratcha shl 16)*/
	0,

	DCMD_JUMP+WAIT_PHASE_VALID+IF_NOT+PHASE_CMP+DATA_OUT_
	};
ULONG  jmp_status1_phase1[]={
	0	/* offset status1_phase */
	};

#define dout_phase	dout_phaseB

ULONG  dout_pad_0[]={
	RD_MODIFY_WRT+OP_OR+0x340000+0x400, /*;(4 shl 8) or (__scratcha shl 16)*/
	0
	};
ULONG  dout_pad_addrB[]={
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_
	};
ULONG  SRB_SegmentPad1[]={
	0,	/* offset SRB.SegmentPad,*/

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_dout_pad_addrB[]={
	0	/* offset dout_pad_addrB */
	};
/*
;==========================================================
; Data phase jump table for WIDE SCSI operation
;==========================================================
*/
ULONG  jmp_dio_phaseW[]={

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+   0,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+  0,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+   8,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+  8,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  16,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 16,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  24,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 24,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  32,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 32,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  40,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 40,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  48,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 48,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  56,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 56,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  64,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 64,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  72,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 72,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  80,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 80,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  88,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 88,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  96,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 96,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  104,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 104,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  112,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 112,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  120,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 120,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+   0,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+  0,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+   8,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+  8,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  16,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 16,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  24,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 24,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  32,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 32,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  40,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 40,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  48,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 48,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  56,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 56,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  64,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 64,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  72,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 72,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  80,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 80,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  88,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 88,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  96,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 96,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  104,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 104,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  112,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 112,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  120,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0,  /* offset dout_phaseW+ 120,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_,
	0,  /* offset din_phaseW+  128,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_,
	0   /* offset dout_phaseW+ 128 */
	};
ULONG  jump_tablew[]={
	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_IN_
	};
ULONG  jmp_din_pad_1[]={
	0,	/* offset din_pad_1,*/

	DCMD_JUMP+WAIT_PHASE_VALID+IF_TRUE+PHASE_CMP+DATA_OUT_
	};
ULONG  jmp_dout_pad_1[]={
	0	/* offset dout_pad_1 */
	};

#define jump_tableW	jump_tablew
/*
;==========================================================
; Data in phase
;==========================================================
*/
ULONG  din_phaseW[]={
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment0,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment1,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment2,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment3,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment4,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment5,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment6,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment7,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment8,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment9,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment10,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment11,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment12,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment13,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment14,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment15,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment0,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment1,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment2,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment3,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment4,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment5,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment6,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment7,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment8,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment9,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment10,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment11,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment12,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment13,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment14,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment15,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_,
	0, /* offset SRB.Segment16,*/

	RD_MODIFY_WRT+OP_OR+0x340000+0x100, /*;(1 shl 8) or (__scratcha shl 16)*/
	0,

	DCMD_JUMP+WAIT_PHASE_VALID+IF_NOT+PHASE_CMP+DATA_IN_
	};
ULONG  jmp_status1_phase2[]={
	0	/* offset status1_phase */
	};

#define din_phase1	din_phaseW

ULONG  din_pad_1[]={
	RD_MODIFY_WRT+OP_OR+0x340000+0x400, /*;(4 shl 8) or (__scratcha shl 16)*/
	0
	};
ULONG  din_pad_addrW[]={
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_IN_
	};
ULONG  SRB_SegmentPad2[]={
	0,	/* offset SRB.SegmentPad,*/

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_din_pad_addrW[]={
	0	/* offset din_pad_addrW */
	};
/*
;==========================================================
; Data out phase
;==========================================================
*/
ULONG  dout_phaseW[]={
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment0,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment1,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment2,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment3,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment4,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment5,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment6,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment7,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment8,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment9,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment10,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment11,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment12,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment13,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment14,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment15,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment0,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment1,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment2,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment3,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment4,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment5,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment6,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment7,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment8,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment9,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment10,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment11,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment12,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment13,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment14,*/
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+CHAIN_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment15,*/
/*;	18000000h or DATA_OUT_ */
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_,
	0, /* offset SRB.Segment16,*/

	RD_MODIFY_WRT+OP_OR+0x340000+0x100, /*;(1 shl 8) or (__scratcha shl 16)*/
	0,

	DCMD_JUMP+WAIT_PHASE_VALID+IF_NOT+PHASE_CMP+DATA_OUT_
	};
ULONG  jmp_status1_phase3[]={
	0	/* offset status1_phase */
	};

#define dout_phase1	dout_phaseW

ULONG  dout_pad_1[]={
	RD_MODIFY_WRT+OP_OR+0x340000+0x400, /*;(4 shl 8) or (__scratcha shl 16)*/
	0
	};
ULONG  dout_pad_addrW[]={
	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+DATA_OUT_
	};
ULONG  SRB_SegmentPad3[]={
	0,	/* offset SRB.SegmentPad,*/

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_dout_pad_addrW[]={
	0	/* offset dout_pad_addrW */
	};
/*
;==========================================================
; message out phase
;==========================================================
*/
ULONG  mout_phase[]={
	DCMD_SET_ATN,
	0,

	DCMD_BLOCK_MOVE+TABLE_INDIRECT+BLOCK_MOVE+MSG_OUT_
	};
ULONG  SRB_msgout01[]={
	0,	/* offset SRB.__msgout0,*/

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_check_phase2[]={
	0	/* offset check_phase */
	};
/*
;==========================================================
; Status phase process
;==========================================================
*/
ULONG  status_phase[]={
	DCMD_BLOCK_MOVE+BLOCK_MOVE+STATUS_+1
	};
ULONG  ACB_status[]={
	0	/* offset ACB.status */
	};
/*
;==========================================================
; message in phase
;==========================================================
*/
ULONG  min_phase[]={
	DCMD_BLOCK_MOVE+BLOCK_MOVE+MSG_IN_+1
	};
ULONG  ACB_msgin123_3[]={
	0,	/* offset ACB.msgin123,*/

	DCMD_JUMP+IF_NOT+DATA_CMP+CMD_COMPLETE
	};
ULONG  jmp_jump_msgok[]={
	0	/* offset jump_msgok */
	};
/*
;==========================================================
; command complete message
;==========================================================
*/
ULONG  msg__0[]={
	RD_MODIFY_WRT+OP_AND+0x20000+0x7F00, /*;(7FH shl 8) or (__scntl2 shl 16)*/
	0,

	DCMD_CLR_ACK,
	0,

	DCMD_WAIT_DISC,
	0,

	DCMD_INT+IF_TRUE,
	__COMPLETE
	};
/*
;==========================================================
; Other message
;==========================================================
*/
ULONG  jump_msgok[]={
	DCMD_JUMP+IF_TRUE+DATA_CMP+SAVE_PTR
	};
ULONG  jmp_msg__a[]={
	0,	/* offset msg__a,*/

	DCMD_JUMP+IF_TRUE+DATA_CMP+RESTORE_PTR
	};
ULONG  jmp_msg__3[]={
	0,	/* offset msg__3,*/

	DCMD_JUMP+IF_TRUE+DATA_CMP+DISCONNECTMSG
	};
ULONG  jmp_msg__4[]={
	0,	/* offset msg__4,*/

	DCMD_JUMP+IF_TRUE+DATA_CMP+EXT_MSG
	};
ULONG  jmp_msg__1[]={
	0,	/* offset msg__1,*/

	DCMD_INT+IF_TRUE+DATA_CMP+MSG_REJECT,
	__MSGREJECT,

	DCMD_JUMP+IF_TRUE+DATA_CMP+LINK_CMD_CPL
	};
ULONG  jmp_msg__a1[]={
	0,	/* offset msg__a,*/

	DCMD_JUMP+IF_TRUE+DATA_CMP+LINK_CMD_FLAG
	};
ULONG  jmp_msg__a2[]={
	0,	/* offset msg__a,*/

	DCMD_JUMP+IF_TRUE+DATA_CMP+IGNORE_WIDE_RES
	};
ULONG  jmp_msg__23[]={
	0,	/* offset msg__23,*/

	DCMD_INT+IF_TRUE,
	__MSGUNKNOWN
	};
/*
;==========================================================
; Extended message
;==========================================================
*/
ULONG  msg__1[]={
	DCMD_CLR_ACK,
	0,

	DCMD_BLOCK_MOVE+BLOCK_MOVE+MSG_IN_+1   /*  ;ext msg len */
	};
ULONG  ACB_msgin123_4[]={
	0,	/* offset ACB.msgin123,*/

	DCMD_JUMP+IF_TRUE+DATA_CMP+3
	};
ULONG  jmp_msg___3[]={
	0,	/* offset msg___3,*/

	DCMD_JUMP+IF_TRUE+DATA_CMP+2
	};
ULONG  jmp_msg___2[]={
	0,	/* offset msg___2,*/

	DCMD_INT+IF_TRUE,
	__MSGEXTEND
	};

ULONG  msg___3[]={
	DCMD_CLR_ACK,
	0,

	DCMD_BLOCK_MOVE+BLOCK_MOVE+MSG_IN_+3
	};
ULONG  ACB_msgin123_5[]={
	0,	/* offset ACB.msgin123,*/

	DCMD_INT+IF_TRUE,
	__MSGSYNC
	};

ULONG  msg___2[]={
	DCMD_CLR_ACK,
	0,

	DCMD_BLOCK_MOVE+BLOCK_MOVE+MSG_IN_+2
	};
ULONG  ACB_msgin123_6[]={
	0,	/* offset ACB.msgin123,*/

	DCMD_INT+IF_TRUE,
	__MSGWIDE
	};
/*
;############################################################
; for synchronous negotiation
; 1. Active  ==> INT3, restart at data__1_2
; 2. Passive ==> INT3, prepare message out, restart at data__1_1
; 3. Disable ==> INT3, prepare message out, restart at data__1_1
;############################################################
*/
ULONG  set_atn[]={
	DCMD_SET_ATN,
	0
	};
ULONG  msg__a[]={
	DCMD_CLR_ACK,
	0,

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_check_phase3[]={
	0	/* offset check_phase */
	};

ULONG  msg__23[]={	   /*	 ; ignore wide residue */
	DCMD_CLR_ACK,
	0,

	DCMD_BLOCK_MOVE+BLOCK_MOVE+MSG_IN_+1
	};
ULONG  ACB_msgin123_7[]={
	0,	/* offset ACB.msgin123,*/

	DCMD_CLR_ACK,
	0,

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_check_phase4[]={
	0	/* offset check_phase */
	};

ULONG  msg__3[]={
	DCMD_CLR_ACK,
	0,

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_check_phase5[]={
	0	/* offset check_phase */
	};

ULONG  msg__4[]={	   /*	 ; disconnect */
	RD_MODIFY_WRT+OP_AND+0x20000+0x7F00, /*;(7FH shl 8) or (__scntl2 shl 16)*/
	0,

	DCMD_CLR_ACK,
	0,

	DCMD_WAIT_DISC,
	0,

	DCMD_INT+IF_TRUE,
	__DISCONNECTED
	};

ULONG  clr_atn[]={
	DCMD_CLR_ATN,
	0,

	DCMD_JUMP+IF_TRUE
	};
ULONG  jmp_check_phase6[]={
	0	/* offset check_phase */
	};
/*
;==========================================================
; Used for script operation
;==========================================================
*/
ULONG  start_mov[]={
/*;	  DCMD_MEM_MOVE+(OFFSET DGROUP:end_script - OFFSET DGROUP:start_script)   ;Memory move SCRIPTS instruction*/
	DCMD_MEM_MOVE+0x1000	  /*;Memory move SCRIPTS instruction ( 4K )*/
	};
ULONG  SrcPhysAddr[]={
	0		/*; source */
	};
ULONG  DesPhysAddr[]={
	0,		/*; destination */

	DCMD_INT+IF_TRUE, /*; script interrupt, */
	0,

	DCMD_INT+IF_NOT,  /*; script interrupt */
	0
	};
ULONG  end_script[]={0};
/***********************************************************************/

