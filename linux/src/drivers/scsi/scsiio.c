/***********************************************************************
 *	FILE NAME : SCSIIO.C					       *
 *	     BY   : C.L. Huang					       *
 *	Description: Device Driver for Tekram DC-390W/U/F (T) PCI SCSI *
 *		     Bus Master Host Adapter			       *
 ***********************************************************************/


static void
PrepareSG( PACB pACB, PDCB pDCB, PSRB pSRB )
{
    ULONG  retAddr,wlval;
    USHORT wval,i;
    PSGL   psgl;
    PSGE   psge;


    retAddr = pACB->jmp_table8;
    if(pDCB->DCBscntl3 & EN_WIDE_SCSI)
	retAddr += jmp_table16;
    wval = (USHORT)(pSRB->SGcount);
    wval <<= 4; 	/* 16 bytes per entry, datain=8, dataout=8 */
			/* (4 bytes for count, 4 bytes for addr) */
    retAddr -= (ULONG)wval;
    pSRB->ReturnAddr = retAddr;    /* return address for SCRIPT */
    if(wval)
    {
	wval >>= 1;
	wlval = (ULONG) pSRB->SegmentPad;
	wlval -= (ULONG)wval;
	wval >>= 3;
	psge = (PSGE) wlval;
	psgl = pSRB->pSegmentList;
	for(i=0; i<wval; i++)
	{
#ifndef VERSION_ELF_1_2_13
	    psge->SGXPtr = virt_to_phys( psgl->address );
#else
	    psge->SGXPtr = (ULONG) psgl->address;
#endif
	    psge->SGXLen = psgl->length;
	    psge++;
	    psgl++;
	}
    }
}


static void
DC390W_StartSCSI( PACB pACB, PDCB pDCB, PSRB pSRB )
{
    USHORT ioport;
    UCHAR  bval;

    pSRB->TagNumber = 31;
    ioport = pACB->IOPortBase;
    bval = SIGNAL_PROC;
    outb(bval,ioport+ISTAT);
    pACB->pActiveDCB = pDCB;
    pDCB->pActiveSRB = pSRB;
    return;
}


#ifndef  VERSION_ELF_1_2_13
static void
DC390W_Interrupt( int irq, void *dev_id, struct pt_regs *regs)
#else
static void
DC390W_Interrupt( int irq, struct pt_regs *regs)
#endif
{
    PACB pACB;
    PDCB pDCB;
    ULONG  wlval;
    USHORT ioport = 0;
    USHORT wval, i;
    void  (*stateV)( PACB );
    UCHAR  istat = 0;
    UCHAR  bval;

    pACB = pACB_start;
    if( pACB == NULL )
	return;
    for( i=0; i < adapterCnt; i++ )
    {
	if( pACB->IRQLevel == (UCHAR) irq )
	{
	     ioport = pACB->IOPortBase;
	     istat = inb( ioport+ISTAT );
	     if( istat & (ABORT_OP+SCSI_INT_PENDING+DMA_INT_PENDING) )
		 break;
	     else
		pACB = pACB->pNextACB;
	}
	else
	{
	    pACB = pACB->pNextACB;
	}
    }

    if( pACB == (PACB )-1 )
    {
	printk("DC390W_intr: Spurious interrupt detected!\n");
	return;
    }


#ifdef DC390W_DEBUG1
	printk("Istate=%2x,",istat);
#endif
    /* if Abort operation occurred, reset abort bit before reading DMA status
       to prevent further aborted interrupt.  */

    if(istat &	ABORT_OP)
    {
	istat &= ~ABORT_OP;
	outb(istat,ioport+ISTAT);
    }

    pDCB = pACB->pActiveDCB;
    bval = inb(ioport+CTEST2);	 /* Clear Signal Bit */

    /*	If Scsi Interrupt, then clear Interrupt Status by reading
	Scsi interrupt status register 0. */

    wlval = 0;
    if(istat & SCSI_INT_PENDING)
    {
	wlval = (ULONG)inw( ioport+SIST0 );
	wlval <<= 8;
    }

    /*	If DMA Interrupt, then read the DMA status register to see what happen */

    if(istat & DMA_INT_PENDING)
    {
	bval = inb(ioport+DSTAT);
	wlval |= (ULONG) bval;
    }

#ifdef DC390W_DEBUG1
	printk("IDstate=%8x,",(UINT) wlval);
#endif
    if(wlval & ( (SEL_TIMEOUT << 16)+
		 ((SCSI_GERROR+UNEXPECT_DISC+SCSI_RESET) << 8)+
		 ILLEGAL_INSTRUC+ABORT_) )
    {
	 ExceptionHandler( wlval, pACB, pDCB );
    }
    else if( wlval & SCRIPTS_INT )
    {
	 wval = inw( ioport+DSPS );
	 stateV = (void *) IntVector[wval];
	 stateV( pACB );
    }
    else if( wlval & ( PARITY_ERROR << 8) )
	ParityError( pACB, pDCB );
    else if( wlval & ( PHASE_MISMATCH << 8) )
	PhaseMismatch( pACB );
    return;
}


static void
ExceptionHandler(ULONG wlval, PACB pACB, PDCB pDCB)
{
    PSRB  pSRB;
    UCHAR bval;
    USHORT ioport;

/* disconnect/scsi reset/illegal instruction */

    ioport = pACB->IOPortBase;
    if(wlval & ( (SCSI_RESET+SCSI_GERROR) << 8) )
	DC390W_ScsiRstDetect( pACB );
    else if(wlval & ABORT_)
    {
#ifdef DC390W_DEBUG0
	printk("AboRst,");
#endif
	if( !InitialTime )
	    DC390W_ResetSCSIBus2( pACB );
    }
    else if(wlval & (SEL_TIMEOUT << 16) )
    {
	pACB->status = SCSI_STAT_SEL_TIMEOUT;
#ifdef DC390W_DEBUG1
	printk("Selto,");
#endif
	DC390W_CmdCompleted( pACB );
    }
    else if(wlval & (UNEXPECT_DISC << 8) )
    {
	bval = inb(ioport+STEST3);
	bval |= CLR_SCSI_FIFO;
	outb(bval,ioport+STEST3);
	bval = CLR_DMA_FIFO;
	outb(bval,ioport+CTEST3);
	pSRB = pDCB->pActiveSRB;
	if( pSRB->SRBState & DO_SYNC_NEGO )
	{
	    pDCB->DevMode &= ~SYNC_NEGO_;
	    pACB->status = SCSI_STAT_CHECKCOND;
	    DC390W_CmdCompleted( pACB );
	}
	else if( pSRB->SRBState & DO_WIDE_NEGO )
	{
	    pDCB->DevMode &= ~WIDE_NEGO_;
	    pACB->status = SCSI_STAT_CHECKCOND;
	    DC390W_CmdCompleted( pACB );
	}
	else
	{
	    pACB->status = SCSI_STAT_UNEXP_BUS_F;
	    DC390W_CmdCompleted( pACB );
	}
#ifdef DC390W_DEBUG0
	printk("Uxpbf,");
#endif
    }
    else
    {
#ifdef DC390W_DEBUG0
	printk("Except,");
#endif
	DC390W_ResetSCSIBus( pACB );
    }
}


static void
ParityError( PACB pACB, PDCB pDCB )
{
    ULONG   ioport;
    UCHAR   bval,msg;
    ULONG   wlval;
    PSRB    pSRB;

    ioport = pACB->IOPortBase;
    bval = inb(ioport+SCRATCHA);
    if(bval & RE_SELECTED_)
    {
#ifdef DC390W_DEBUG0
	printk("ParityErr,");
#endif
	DC390W_ResetSCSIBus( pACB );
	return;
    }
    else
    {
	pSRB = pDCB->pActiveSRB;
	bval = inb(ioport+STEST3);
	bval |= CLR_SCSI_FIFO;
	outb(bval,ioport+STEST3);
	bval = CLR_DMA_FIFO;
	outb(bval,ioport+CTEST3);

	bval = inb(ioport+DCMD);
	bval &= 0x07;	    /* get phase bits */
	if(bval == 0x07)    /* message in phase */
	{
	    msg = MSG_PARITY_ERROR;
	    wlval = pACB->jmp_clear_ack;
	}
	else
	{
	    msg = MSG_INITIATOR_ERROR;
	    wlval = pACB->jmp_next;
	}
	pSRB->__msgout0[0] = 1;
	pSRB->MsgOutBuf[0] = msg;
	outl(wlval,(ioport+DSP));
	return;
    }
}


static void
DC390W_Signal( PACB pACB )
{
    PDCB pDCB;
    PSRB pSRB;
    USHORT ioport;
    ULONG wlval, flags;
    UCHAR bval,msgcnt,tagnum;

    save_flags(flags);
    cli();
    ioport = pACB->IOPortBase;
    pDCB = pACB->pActiveDCB;
    pSRB = pDCB->pActiveSRB;
#ifdef DC390W_DEBUG0
    printk("Signal,Cmd=%2x", pSRB->CmdBlock[0]);
#endif
    wlval = pSRB->PhysSRB;
    outl(wlval,(ioport+DSA));
    wlval = pSRB->ReturnAddr;
    outl(wlval,(ioport+TEMP));
    msgcnt = 1;
    bval = pDCB->IdentifyMsg;
    pSRB->MsgOutBuf[0] = bval;
    if( (pSRB->CmdBlock[0] != INQUIRY) &&
	(pSRB->CmdBlock[0] != REQUEST_SENSE) )
    {
	if(pDCB->MaxCommand > 1)
	{
	    wlval = 1;
	    tagnum = 0;
	    while( wlval & pDCB->TagMask )
	    {
		wlval = wlval << 1;
		tagnum++;
	    }
	    pDCB->TagMask |= wlval;
	    pSRB->TagNumber = tagnum;
	    pSRB->MsgOutBuf[1] = MSG_SIMPLE_QTAG;
	    pSRB->MsgOutBuf[2] = tagnum;
	    msgcnt = 3;
	}
    }
    else
    {
	pSRB->MsgOutBuf[0] &= 0xBF;	   /* Diable Disconnected */
	if(pSRB->CmdBlock[0] == INQUIRY)
	{
	    if(bval & 0x07)
		goto type_6_3;
	}
	if(pDCB->DevMode & WIDE_NEGO_)
	{
	    msgcnt = 5;
	    *((PULONG) &(pSRB->MsgOutBuf[1])) = 0x01030201;
	    pSRB->SRBState |= DO_WIDE_NEGO;
	}
	else if(pDCB->DevMode & SYNC_NEGO_)
	{
	    msgcnt = 6;
	    *((PULONG) &(pSRB->MsgOutBuf[1])) = 0x00010301;
	    pSRB->MsgOutBuf[4] = pDCB->NegoPeriod;
	    pSRB->MsgOutBuf[5] = SYNC_NEGO_OFFSET;
	    pSRB->SRBState |= DO_SYNC_NEGO;
	}
    }
type_6_3:
    pSRB->__msgout0[0] = (ULONG) msgcnt;
    wlval = 0;
    outl(wlval,(ioport+SCRATCHA));
    bval = pDCB->DCBscntl0;
    outb(bval,ioport+SCNTL0);
    pSRB->__select = *((PULONG) &(pDCB->DCBselect));
#ifdef DC390W_DEBUG0
    printk("__sel=%8x,", (UINT)(pSRB->__select));
#endif
    wlval = pACB->jmp_select;
    outl(wlval,(ioport+DSP));
    restore_flags(flags);
    return;
}


static void
DC390W_MessageWide( PACB pACB )
{
    PDCB pDCB;
    PSRB pSRB;
    PUCHAR msgoutPtr;
    USHORT ioport;
    ULONG wlval;
    UCHAR bval,msgcnt;


#ifdef DC390W_DEBUG0
    printk("MsgWide,");
#endif
    ioport = pACB->IOPortBase;
    pDCB = pACB->pActiveDCB;
    pSRB = pDCB->pActiveSRB;
    msgcnt = 0;
    pDCB->DCBscntl3 &= ~EN_WIDE_SCSI;
    msgoutPtr = pSRB->MsgOutBuf;
    if( pSRB->SRBState & DO_WIDE_NEGO )
    {
	pSRB->SRBState &= ~DO_WIDE_NEGO;
	if( pACB->msgin123[0] == 3 )
	{
	    bval = pACB->msgin123[1];
	    if(bval == 1)
	    {
		pDCB->DCBscntl3 |= EN_WIDE_SCSI;
		goto x5;
	    }
	    if(bval < 1)
		goto x5;
	}
    }

/*type_11_1:*/
    msgcnt = 1;
    *msgoutPtr = MSG_REJECT_;
    msgoutPtr++;
x5:
    bval = pDCB->DCBscntl3;
    outb(bval,ioport+SCNTL3);
    AdjustTemp(pACB,pDCB,pSRB);
    SetXferRate(pACB,pDCB);
    if( pDCB->DevMode & SYNC_NEGO_ )
    {
	*((PULONG)msgoutPtr) = 0x00010301;
	*(msgoutPtr + 3) = pDCB->NegoPeriod;
	*(msgoutPtr + 4) = SYNC_NEGO_OFFSET;
	msgcnt += 5;
	pSRB->SRBState |= DO_SYNC_NEGO;
    }

    pSRB->__msgout0[0] = (ULONG) msgcnt;
    wlval = pACB->jmp_clear_ack;
    if(msgcnt)
	wlval = pACB->jmp_set_atn;
    outl(wlval,(ioport+DSP));
    return;
}


static void
DC390W_MessageSync( PACB pACB )
{
    PDCB pDCB;
    PSRB pSRB;
    USHORT ioport;
    ULONG wlval;
    USHORT wval,wval1;
    UCHAR bval,bval1;

#ifdef DC390W_DEBUG0
    printk("MsgSync,");
#endif
    ioport = pACB->IOPortBase;
    pDCB = pACB->pActiveDCB;
    pSRB = pDCB->pActiveSRB;
    if( !(pSRB->SRBState & DO_SYNC_NEGO) )
	 goto MessageExtnd;
    pSRB->SRBState &= ~DO_SYNC_NEGO;
    if(pACB->msgin123[0] != 1)
    {
MessageExtnd:
	pSRB->__msgout0[0] = 1;
	pSRB->MsgOutBuf[0] = MSG_REJECT_;
	wlval = pACB->jmp_set_atn;
	outl(wlval,(ioport+DSP));
	return;
    }
    bval = pACB->msgin123[2];	/* offset */
asyncx:
    pDCB->DCBsxfer = bval;
    if(bval == 0)	 /* if offset or period == 0, async */
    {
	if( pACB->AdaptType == DC390W )
	    bval = SYNC_CLK_F2+ASYNC_CLK_F2;
	else
	    bval = SYNC_CLK_F4+ASYNC_CLK_F4;
	pDCB->DCBscntl3 = bval;
    }
    else
    {
	bval = pACB->msgin123[1];
	if(bval == 0)
	    goto asyncx;
	pDCB->SyncPeriod = bval;
	wval = (USHORT)bval;
	wval <<= 3;
	bval = pDCB->DCBscntl3;
	bval &= 0x0f;
	if(wval < 200)		/* < 100 ns ==> Fast-20 */
	{
	    bval |= 0x90;	/* Fast-20 and div 1 */
	    bval1 = 25; 	/* 12.5 ns */
	}
	else if(wval < 400)
	{
	    bval |= 0x30;	/* 1 cycle = 25ns */
	    bval1 = 50;
	}
	else			/* Non Fast */
	{
	    bval |= 0x50;	/* 1 cycle = 50ns */
	    bval1 = 100;
	}
	if( pACB->AdaptType == DC390W )
	    bval -= 0x20;  /* turn down to 40Mhz scsi clock */
			   /* assume 390W will not receive fast-20 */
	wval1 = wval;
	wval /= bval1;
	if(wval * bval1 < wval1)
	    wval++;
			    /* XFERP	 TP2 TP1 TP0  */
	wval -= 4;	    /*	 4	  0   0   0   */
			    /*	 5	  0   0   1   */
	wval <<= 5;
	pDCB->DCBsxfer |= (UCHAR)wval;
	pDCB->DCBscntl3 = bval;
    }
/*sync_2:*/
    SetXferRate( pACB,pDCB );
    wlval = pACB->jmp_clear_ack;
/*sync_3:*/
    bval = pDCB->DCBscntl3;
    outb(bval,ioport+SCNTL3);
    bval = pDCB->DCBsxfer;
    outb(bval,ioport+SXFER);
    outl(wlval,(ioport+DSP));
    return;
}


static void
DC390W_MsgReject( PACB pACB )
{
    PDCB pDCB;
    PSRB pSRB;
    ULONG wlval;
    USHORT ioport;
    UCHAR bval;

#ifdef DC390W_DEBUG0
    printk("Msgrjt,");
#endif
    pDCB = pACB->pActiveDCB;
    pSRB = pDCB->pActiveSRB;
    wlval = pACB->jmp_clear_ack;
    if(pSRB->SRBState & DO_WIDE_NEGO)
    {
	pSRB->SRBState &= ~DO_WIDE_NEGO;
	pDCB->DCBscntl3 &= ~EN_WIDE_SCSI;
	AdjustTemp( pACB, pDCB, pSRB );
	SetXferRate( pACB, pDCB );
	if( pDCB->DevMode & SYNC_NEGO_ )
	{
	    *((PULONG) &(pSRB->MsgOutBuf[0])) = 0x00010301;
	    pSRB->MsgOutBuf[3] = pDCB->NegoPeriod;
	    pSRB->MsgOutBuf[4] = SYNC_NEGO_OFFSET;
	    pSRB->__msgout0[0] = 5;
	    pSRB->SRBState |= DO_SYNC_NEGO;
	    wlval = pACB->jmp_set_atn;
	}
    }
    else
    {
	if(pSRB->SRBState & DO_SYNC_NEGO)
	{
	    pSRB->SRBState &= ~DO_SYNC_NEGO;
	    pDCB->DCBsxfer = 0; /* reject sync msg, set aync */
	    if( pACB->AdaptType == DC390W )
		bval = SYNC_CLK_F2+ASYNC_CLK_F2;
	    else
		bval = SYNC_CLK_F4+ASYNC_CLK_F4;
	    pDCB->DCBscntl3 = bval;
	    SetXferRate(pACB,pDCB);
	    wlval = pACB->jmp_clear_ack;
	}
    }
    ioport = pACB->IOPortBase;
    bval = pDCB->DCBscntl3;
    outb(bval,ioport+SCNTL3);
    bval = pDCB->DCBsxfer;
    outb(bval,ioport+SXFER);
    outl(wlval,(ioport+DSP));
    return;
}


static void
AdjustTemp( PACB pACB, PDCB pDCB, PSRB pSRB )
{
    USHORT ioport;
    ULONG wlval;

    wlval = pSRB->ReturnAddr;
    if(wlval <= pACB->jmp_table8)
    {
	if(pDCB->DCBscntl3 & EN_WIDE_SCSI)
	    wlval +=  jmp_table16;
    }
    else
    {
	if((pDCB->DCBscntl3 & EN_WIDE_SCSI) == 0)
	    wlval -= jmp_table16;
    }
    pSRB->ReturnAddr = wlval;
    ioport = pACB->IOPortBase;
    outl(wlval,(ioport+TEMP));
    return;
}


static void
SetXferRate( PACB pACB, PDCB pDCB )
{
    UCHAR  bval;
    USHORT cnt, i;
    PDCB   ptr;

    if( !(pDCB->IdentifyMsg & 0x07) )
    {
	if( pACB->scan_devices )
	{
	    CurrDCBscntl3 = pDCB->DCBscntl3;
	}
	else
	{
	    ptr = pACB->pLinkDCB;
	    cnt = pACB->DeviceCnt;
	    bval = pDCB->UnitSCSIID;
	    for(i=0; i<cnt; i++)
	    {
		if( ptr->UnitSCSIID == bval )
		{
		    ptr->DCBsxfer = pDCB->DCBsxfer;
		    ptr->DCBscntl3 = pDCB->DCBscntl3;
		}
		ptr = ptr->pNextDCB;
	    }
	}
    }
    return;
}


static void
DC390W_UnknownMsg( PACB pACB )
{
    PSRB pSRB;
    ULONG wlval;
    USHORT ioport;

    pSRB = pACB->pActiveDCB->pActiveSRB;
    pSRB->__msgout0[0] = 1;
    pSRB->MsgOutBuf[0] = MSG_REJECT_;
    wlval = pACB->jmp_set_atn;
    ioport = pACB->IOPortBase;
    outl(wlval,(ioport+DSP));
    return;
}


static void
DC390W_MessageExtnd( PACB pACB )
{
    DC390W_UnknownMsg( pACB );
}


static void
DC390W_Disconnected( PACB pACB )
{
    PDCB pDCB;
    PSRB pSRB;
    ULONG  wlval, flags;
    USHORT ioport;
    UCHAR bval;

#ifdef DC390W_DEBUG0
    printk("Discnet,");
#endif
    save_flags(flags);
    cli();
    pDCB = pACB->pActiveDCB;
    if (! pDCB)
     {
#ifdef DC390W_DEBUG0
	printk("ACB:%08lx->ActiveDCB:%08lx !,", (ULONG)pACB, (ULONG)pDCB);
#endif
	restore_flags(flags); return;
     }

    pSRB = pDCB->pActiveSRB;

    ioport = pACB->IOPortBase;
    bval = inb(ioport+SCRATCHA);
    pSRB->ScratchABuf = bval;
    pSRB->SRBState |= SRB_DISCONNECT;	/* 1.02 */
    wlval = pACB->jmp_reselect;
    outl(wlval,(ioport+DSP));
    pACB->pActiveDCB = 0;
    DoWaitingSRB( pACB );
    restore_flags(flags);
    return;
}


static void
DC390W_Reselected( PACB pACB )
{
#ifdef DC390W_DEBUG0
    printk("Rsel,");
#endif
    pACB->msgin123[0] = 0x80;	 /* set identify byte 80h */
    DC390W_Reselected1(pACB);
    return;
}


static void
DC390W_Reselected1( PACB pACB )
{
    PDCB   pDCB;
    PSRB   pSRB;
    USHORT ioport, wval;
    ULONG  wlval, flags;
    UCHAR  bval;


#ifdef DC390W_DEBUG0
    printk("Rsel1,");
#endif
    ioport = pACB->IOPortBase;
    pDCB = pACB->pActiveDCB;
    if(pDCB)
    {
	pSRB = pDCB->pActiveSRB;
	RewaitSRB( pDCB, pSRB );
    }

    wval = (USHORT) (pACB->msgin123[0]);
    wval = (wval & 7) << 8;		/* get LUN */
    wval |= (USHORT) (inb(ioport+SSID) & 0x0f); /* get ID */
    pDCB = pACB->pLinkDCB;
    while( *((PUSHORT) &pDCB->UnitSCSIID) != wval )
	pDCB = pDCB->pNextDCB;
    pACB->pActiveDCB = pDCB;
    bval = pDCB->DCBscntl3;
    outb(bval,ioport+SCNTL3);
    bval = pDCB->DCBsxfer;
    outb(bval,ioport+SXFER);
    bval = pDCB->DCBscntl0;
    outb(bval,ioport+SCNTL0);
    if(pDCB->MaxCommand > 1)
    {
	wlval = pACB->jmp_reselecttag;
	outl(wlval,(ioport+DSP));
    }
    else
    {
	pSRB = pDCB->pActiveSRB;
	if( !pSRB || !(pSRB->SRBState & SRB_DISCONNECT) )
	{
	    save_flags(flags);
	    cli();
	    pSRB = pACB->pFreeSRB;
	    pACB->pFreeSRB = pSRB->pNextSRB;
	    restore_flags(flags);
	    pSRB->SRBState = SRB_UNEXPECT_RESEL;
	    pDCB->pActiveSRB = pSRB;
	    pSRB->MsgOutBuf[0] = MSG_ABORT;
	    pSRB->__msgout0[0] = 1;
	}
	pSRB->SRBState &= ~SRB_DISCONNECT;
	wlval = pSRB->PhysSRB;
	outl(wlval,(ioport+DSA));
	wlval = pSRB->ReturnAddr;
	outl(wlval,(ioport+TEMP));
	bval = pSRB->ScratchABuf;
	outb(bval,ioport+SCRATCHA);
	if( pSRB->SRBState & SRB_UNEXPECT_RESEL )
	    wlval = pACB->jmp_set_atn;
	else
	    wlval = pACB->jmp_clear_ack;
	outl(wlval,(ioport+DSP));
    }
    return;
}


static void
DC390W_ReselectedT( PACB pACB )
{
    PDCB pDCB;
    PSRB pSRB, psrb1;
    USHORT ioport;
    ULONG wlval, flags;
    UCHAR bval;

#ifdef DC390W_DEBUG0
    printk("RselT,");
#endif
    ioport = pACB->IOPortBase;
    bval = pACB->msgin123[1];
    pDCB = pACB->pActiveDCB;
    pSRB = pDCB->pGoingSRB;
    psrb1 = pDCB->pGoingLast;
    if( !pSRB )
	goto  UXP_RSL;
    for(;;)
    {
	if(pSRB->TagNumber != bval)
	{
	    if( pSRB != psrb1 )
		pSRB = pSRB->pNextSRB;
	    else
		goto  UXP_RSL;
	}
	else
	    break;
    }
    if( !(pSRB->SRBState & SRB_DISCONNECT) )
    {
UXP_RSL:
	    save_flags(flags);
	    cli();
	    pSRB = pACB->pFreeSRB;
	    pACB->pFreeSRB = pSRB->pNextSRB;
	    restore_flags(flags);
	    pSRB->SRBState = SRB_UNEXPECT_RESEL;
	    pDCB->pActiveSRB = pSRB;
	    pSRB->MsgOutBuf[0] = MSG_ABORT_TAG;
	    pSRB->__msgout0[0] = 1;
    }
    else
    {
	pSRB->SRBState &= ~SRB_DISCONNECT;
	pDCB->pActiveSRB = pSRB;
    }
    wlval = pSRB->PhysSRB;
    outl(wlval,(ioport+DSA));
    wlval = pSRB->ReturnAddr;
    outl(wlval,(ioport+TEMP));
    bval = pSRB->ScratchABuf;
    outb(bval,ioport+SCRATCHA);
    if( pSRB->SRBState & SRB_UNEXPECT_RESEL )
	wlval = pACB->jmp_set_atn;
    else
	wlval = pACB->jmp_clear_ack;
    outl(wlval,(ioport+DSP));
    return;
}


static void
DC390W_RestorePtr( PACB pACB )
{
   PSRB pSRB;
   USHORT ioport;
   ULONG wlval;

   pSRB = pACB->pActiveDCB->pActiveSRB;
   wlval = pSRB->ReturnAddr;
   ioport = pACB->IOPortBase;
   outl(wlval,(ioport+TEMP));
   wlval = inl(ioport+DSP);
   outl(wlval,(ioport+DSP));
   return;
}


static void
PhaseMismatch( PACB pACB )
{
    USHORT  ioport;
    ULONG   wlval,swlval;
    USHORT  wval;
    UCHAR   bval,phase;
    PDCB    pDCB;

#ifdef DC390W_DEBUG0
    printk("Mismatch,");
#endif
    ioport = pACB->IOPortBase;
    bval = inb(ioport+SCRATCHA);
    if(bval & OVER_RUN_)	/* xfer PAD */
    {
	bval = inb(ioport+STEST3);
	bval |= CLR_SCSI_FIFO;
	outb(bval,ioport+STEST3);
	bval = CLR_DMA_FIFO;
	outb(bval,ioport+CTEST3);
	wlval = pACB->jmp_next; 	 /* check phase */
	outl(wlval,(ioport+DSP));
	return;
    }
    pDCB = pACB->pActiveDCB;
    wlval = inl(ioport+DBC);
    phase = (UCHAR)((wlval & 0x07000000) >> 24);
    wlval &= 0xffffff;	/* bytes not xferred */
    if( phase == SCSI_DATA_IN )
    {
	swlval = pACB->jmp_din8;
	if( pDCB->DCBscntl3 & EN_WIDE_SCSI )
	     swlval += jmp_din16;
	DataIOcommon(pACB,swlval,wlval);
    }
    else if( phase == SCSI_DATA_OUT )
    {
	wval = (USHORT)inb(ioport+CTEST5);
	wval <<= 8;
	bval = inb(ioport+DFIFO);
	wval |= (USHORT) bval;
	wval -= ((USHORT)(wlval & 0xffff));
	wval &= 0x3ff;
	wlval += (ULONG)wval;  /* # of bytes remains in FIFO */
	bval = inb(ioport+SSTAT0);
	if(bval & SODR_LSB_FULL)
	   wlval++;	       /* data left in Scsi Output Data Buffer */
	if(bval & SODL_LSB_FULL)
	   wlval++;	       /* data left in Scsi Output Data Latch */
	swlval = pACB->jmp_dout8;
	if(pDCB->DCBscntl3 & EN_WIDE_SCSI)
	{
	   swlval += jmp_dout16;
	   bval = inb(ioport+SSTAT2);
	   if(bval & SODR_MSB_FULL)
	       wlval++;
	   if(bval & SODL_MSB_FULL)
	       wlval++;
	}
	bval = inb(ioport+STEST3);
	bval |= CLR_SCSI_FIFO;
	outb(bval,ioport+STEST3);
	bval = CLR_DMA_FIFO;
	outb(bval,ioport+CTEST3);
	DataIOcommon(pACB,swlval,wlval);
    }
    else
    {
	bval = inb(ioport+STEST3);
	bval |= CLR_SCSI_FIFO;
	outb(bval,ioport+STEST3);
	bval = CLR_DMA_FIFO;
	outb(bval,ioport+CTEST3);
	if(phase == SCSI_MSG_OUT)
	    wlval = pACB->jmp_clear_atn;
	else
	    wlval = pACB->jmp_next;	/* check phase */
	outl(wlval,(ioport+DSP));
    }
    return;
}


static void
DataIOcommon( PACB pACB, ULONG	Swlval, ULONG  Cwlval )
{
    /* Swlval - script address */
    /* Cwlval - bytes not xferred */
    PDCB pDCB;
    PSRB pSRB;
    PSGE Segptr;
    USHORT ioport;
    ULONG  wlval,swlval,dataXferCnt;
    UCHAR  bval,bvald;

    ioport = pACB->IOPortBase;
    wlval = inl((ioport+DSP));
    pDCB = pACB->pActiveDCB;
    pSRB = pDCB->pActiveSRB;
    wlval -= Swlval;
    bval = inb(ioport+SBCL);
    bval &= 0x07;
    if(bval == SCSI_MSG_IN)
    {
	bval = pDCB->DCBscntl3;
	bval &= ~EN_WIDE_SCSI;
	outb(bval,ioport+SCNTL3);
	bval = inb(ioport+SBDL);
	bvald = pDCB->DCBscntl3;    /* enable WIDE SCSI */
	outb(bvald,ioport+SCNTL3);
	if(bval == MSG_DISCONNECT || bval == MSG_SAVE_PTR)
	{
	    Segptr = (PSGE)((ULONG) &(pSRB->Segment0[0][0]) + wlval);
	    dataXferCnt = Segptr->SGXLen - Cwlval;
	    Segptr->SGXLen = Cwlval;		/* modified count */
	    Segptr->SGXPtr += dataXferCnt;	/* modified address */
	    swlval = pACB->jmp_table8;
	    if(pDCB->DCBscntl3 & EN_WIDE_SCSI)
		swlval += jmp_table16;
	    wlval <<= 1;
	    swlval += wlval;
	    swlval = swlval - ((MAX_SG_LIST_BUF+1) * 16);
	    pSRB->ReturnAddr = swlval;
	}
    }
    else if( Cwlval )	/* Remaining not xferred -- UNDER_RUN */
    {
	Segptr = (PSGE)((ULONG) &(pSRB->Segment0[0][0]) + wlval);
	dataXferCnt = Segptr->SGXLen - Cwlval;
	Segptr->SGXLen = Cwlval;	    /* modified count */
	Segptr->SGXPtr += dataXferCnt;	    /* modified address */
	swlval = pACB->jmp_table8;
	if(pDCB->DCBscntl3 & EN_WIDE_SCSI)
	    swlval += jmp_table16;
	wlval <<= 1;
	swlval += wlval;
	swlval = swlval - ((MAX_SG_LIST_BUF+1) * 16);
	pSRB->RemainSegPtr = swlval;
    }
/* pm__1: */
    wlval = pSRB->ReturnAddr;
    outl(wlval,(ioport+TEMP));
    wlval = pACB->jmp_next;
    outl(wlval,(ioport+DSP));
    return;
}


static void
DC390W_CmdCompleted( PACB pACB )
{
    PDCB pDCB;
    PSRB pSRB;
    USHORT ioport;
    ULONG  wlval, flags;
    UCHAR  bval;

#ifdef DC390W_DEBUG0
    printk("Cmplete,");
#endif
    save_flags(flags);
    cli();
    pDCB = pACB->pActiveDCB;
    pSRB = pDCB->pActiveSRB;
    pDCB->pActiveSRB = NULL;
    ioport = pACB->IOPortBase;

    bval  = inb(ioport+SCRATCHA);
    pSRB->ScratchABuf = bval;		/* save status */
    bval = pSRB->TagNumber;
    if(pDCB->MaxCommand > 1)
       pDCB->TagMask &= (~(1 << bval));   /* free tag mask */
    pACB->pActiveDCB = NULL;		/* no active device */
    wlval = pACB->jmp_reselect; 	/* enable reselection */
    outl(wlval,(ioport+DSP));
    SRBdone( pACB, pDCB, pSRB);
    restore_flags(flags);
    return;
}


static void
SRBdone( PACB pACB, PDCB pDCB, PSRB pSRB )
{
    PSRB   psrb;
    UCHAR  bval, bval1, i, j, status;
    PSCSICMD pcmd;
    PSCSI_INQDATA  ptr;
    USHORT  disable_tag;
    ULONG  flags;
    PSGE   ptr1;
    PSGL   ptr2;
    ULONG  wlval,swlval;

    pcmd = pSRB->pcmd;
    status = pACB->status;
    if(pSRB->SRBFlag & AUTO_REQSENSE)
    {
	pSRB->SRBFlag &= ~AUTO_REQSENSE;
	pSRB->AdaptStatus = 0;
	pSRB->TargetStatus = SCSI_STAT_CHECKCOND;
	if(status == SCSI_STAT_CHECKCOND)
	{
	    pcmd->result = DID_BAD_TARGET << 16;
	    goto ckc_e;
	}
	if(pSRB->RetryCnt == 0)
	{
	    *((PULONG) &(pSRB->CmdBlock[0])) = pSRB->Segment0[0][0];
	    pSRB->XferredLen = pSRB->Segment0[2][1];
	    if( (pSRB->XferredLen) &&
		(pSRB->XferredLen >= pcmd->underflow) )
	    {
		pcmd->result |= (DID_OK << 16);
	    }
	    else
		pcmd->result = (DRIVER_SENSE << 24) | (DRIVER_OK << 16) |
				SCSI_STAT_CHECKCOND;
	    goto ckc_e;
	}
	else
	{
	    pSRB->RetryCnt--;
	    pSRB->TargetStatus = 0;
	    *((PULONG) &(pSRB->CmdBlock[0])) = pSRB->Segment0[0][0];
	    *((PULONG) &(pSRB->CmdBlock[4])) = pSRB->Segment0[0][1];
	    *((PULONG) &(pSRB->CmdBlock[8])) = pSRB->Segment0[1][0];
	    pSRB->__command[0]		     = pSRB->Segment0[1][1] & 0xff;
	    pSRB->SGcount		     = (UCHAR) (pSRB->Segment0[1][1] >> 8);
	    *((PULONG) &(pSRB->pSegmentList))= pSRB->Segment0[2][0];
	    if( pSRB->CmdBlock[0] == TEST_UNIT_READY )
	    {
		pcmd->result = (DRIVER_SENSE << 24) | (DRIVER_OK << 16) |
				SCSI_STAT_CHECKCOND;
		goto ckc_e;
	    }
	    pcmd->result |= (DRIVER_SENSE << 24);
	    PrepareSG(pACB,pDCB,pSRB);
	    pSRB->XferredLen = 0;
	    DC390W_StartSCSI( pACB, pDCB, pSRB );
	    return;
	}
    }
    if( status )
    {
	if( status == SCSI_STAT_CHECKCOND)
	{
	    if( !(pSRB->ScratchABuf & SRB_OK) && (pSRB->SGcount) && (pSRB->RemainSegPtr) )
	    {
		wlval = pSRB->RemainSegPtr;
		swlval = pACB->jmp_table8;
		if(pDCB->DCBscntl3 & EN_WIDE_SCSI)
		    swlval += jmp_table16;
		swlval -= wlval;
		swlval >>= 4;
		bval = (UCHAR) swlval;
		wlval = 0;
		ptr1 = (PSGE) &pSRB->Segment0[MAX_SG_LIST_BUF+1][0];
		for( i=0; i< bval; i++)
		{
		    wlval += ptr1->SGXLen;
		    ptr1--;
		}

		bval = pSRB->SGcount;
		swlval = 0;
		ptr2 = pSRB->pSegmentList;
		for( i=0; i< bval; i++)
		{
		    swlval += ptr2->length;
		    ptr2++;
		}
		pSRB->XferredLen = swlval - wlval;
		pSRB->RemainSegPtr = 0;
#ifdef	DC390W_DEBUG0
		printk("XferredLen=%8x,NotXferLen=%8x,",(UINT) pSRB->XferredLen,(UINT) wlval);
#endif
	    }
	    RequestSense( pACB, pDCB, pSRB );
	    return;
	}
	else if( status == SCSI_STAT_QUEUEFULL )
	{
	    bval = (UCHAR) pDCB->GoingSRBCnt;
	    bval--;
	    pDCB->MaxCommand = bval;
	    RewaitSRB( pDCB, pSRB );
	    pSRB->AdaptStatus = 0;
	    pSRB->TargetStatus = 0;
	    return;
	}
	else if(status == SCSI_STAT_SEL_TIMEOUT)
	{
	    pSRB->AdaptStatus = H_SEL_TIMEOUT;
	    pSRB->TargetStatus = 0;
	    pcmd->result = DID_BAD_TARGET << 16;
	}
	else if(status == SCSI_STAT_UNEXP_BUS_F)
	{
	    pSRB->AdaptStatus = H_UNEXP_BUS_FREE;
	    pSRB->TargetStatus = 0;
	    pcmd->result |= DID_NO_CONNECT << 16;
	}
	else if(status == SCSI_STAT_BUS_RST_DETECT )
	{
	    pSRB->AdaptStatus = H_ABORT;
	    pSRB->TargetStatus = 0;
	    pcmd->result = DID_RESET << 16;
	}
	else
	{
	    pSRB->AdaptStatus = 0;
	    pSRB->TargetStatus = status;
	    if( pSRB->RetryCnt )
	    {
		pSRB->RetryCnt--;
		pSRB->TargetStatus = 0;
		PrepareSG(pACB,pDCB,pSRB);
		pSRB->XferredLen = 0;
		DC390W_StartSCSI( pACB, pDCB, pSRB );
		return;
	    }
	    else
	    {
		pcmd->result |= (DID_ERROR << 16) | (ULONG) (pACB->msgin123[0] << 8) |
			       (ULONG) status;
	    }
	}
    }
    else
    {
	status = pSRB->ScratchABuf;
	if(status & OVER_RUN_)
	{
	    pSRB->AdaptStatus = H_OVER_UNDER_RUN;
	    pSRB->TargetStatus = 0;
	    pcmd->result |= (DID_OK << 16) | (pACB->msgin123[0] << 8);
	}
	else		       /* No error */
	{
	    pSRB->AdaptStatus = 0;
	    pSRB->TargetStatus = 0;
	    pcmd->result |= (DID_OK << 16);
	}
    }
ckc_e:

    if( pACB->scan_devices )
    {
	if( pSRB->CmdBlock[0] == TEST_UNIT_READY )
	{
	    if(pcmd->result != (DID_OK << 16))
	    {
		if( pcmd->result & SCSI_STAT_CHECKCOND )
		{
		    goto RTN_OK;
		}
		else
		{
		    pACB->DCBmap[pcmd->target] &= ~(1 << pcmd->lun);
		    pPrevDCB->pNextDCB = pACB->pLinkDCB;
		    if( (pcmd->target == pACB->max_id) &&
		       ((pcmd->lun == 0) || (pcmd->lun == pACB->max_lun)) )
		    {
			pACB->scan_devices = 0;
		    }
		}
	    }
	    else
	    {
RTN_OK:
		pPrevDCB->pNextDCB = pDCB;
		pDCB->pNextDCB = pACB->pLinkDCB;
		if( (pcmd->target == pACB->max_id) && (pcmd->lun == pACB->max_lun) )
		    pACB->scan_devices = END_SCAN;
	    }
	}
	else if( pSRB->CmdBlock[0] == INQUIRY )
	{
	    if( (pcmd->target == pACB->max_id) &&
		(pcmd->lun == pACB->max_lun) )
	    {
		pACB->scan_devices = 0;
	    }
	    ptr = (PSCSI_INQDATA) (pcmd->request_buffer);
	    if( pcmd->use_sg )
		ptr = (PSCSI_INQDATA) (((PSGL) ptr)->address);
	    bval1 = ptr->DevType & SCSI_DEVTYPE;
	    if(bval1 == SCSI_NODEV)
	    {
		pACB->DCBmap[pcmd->target] &= ~(1 << pcmd->lun);
		pPrevDCB->pNextDCB = pACB->pLinkDCB;
	    }
	    else
	    {
		pACB->DeviceCnt++;
		pPrevDCB = pDCB;
		pACB->pDCB_free = (PDCB) ((ULONG) (pACB->pDCB_free) + sizeof( DC390W_DCB ));
		pDCB->DevType = bval1;
		if(bval1 == TYPE_DISK || bval1 == TYPE_MOD)
		{
		    if( (((ptr->Vers & 0x07) >= 2) || ((ptr->RDF & 0x0F) == 2)) &&
			(ptr->Flags & SCSI_INQ_CMDQUEUE) &&
			(pDCB->DevMode & TAG_QUEUING_) &&
			(pDCB->DevMode & EN_DISCONNECT_) )
		    {
			disable_tag = 0;
			for(i=0; i<BADDEVCNT; i++)
			{
			    for(j=0; j<28; j++)
			    {
				if( ((PUCHAR)ptr)[8+j] != baddevname[i][j])
				    break;
			    }
			    if(j == 28)
			    {
				disable_tag = 1;
				break;
			    }
			}

			if( !disable_tag )
			{
			    pDCB->MaxCommand = pACB->TagMaxNum;
			    pDCB->TagMask = 0;
			}
		    }
		}
	    }
	}
    }

    save_flags( flags );
    cli();
/*  ReleaseSRB( pDCB, pSRB ); */

    if(pSRB == pDCB->pGoingSRB )
    {
	pDCB->pGoingSRB = pSRB->pNextSRB;
    }
    else
    {
	psrb = pDCB->pGoingSRB;
	while( psrb->pNextSRB != pSRB )
	    psrb = psrb->pNextSRB;
	psrb->pNextSRB = pSRB->pNextSRB;
	if( pSRB == pDCB->pGoingLast )
	    pDCB->pGoingLast = psrb;
    }
    pSRB->pNextSRB = pACB->pFreeSRB;
    pACB->pFreeSRB = pSRB;
    pDCB->GoingSRBCnt--;

    DoWaitingSRB( pACB );
    restore_flags(flags);

/*  Notify cmd done */
    pcmd->scsi_done( pcmd );

    if( pDCB->QIORBCnt )
	DoNextCmd( pACB, pDCB );
    return;
}


static void
DoingSRB_Done( PACB pACB )
{
    PDCB  pDCB, pdcb;
    PSRB  psrb, psrb2;
    USHORT  cnt, i;
    PSCSICMD pcmd;

    pDCB = pACB->pLinkDCB;
    pdcb = pDCB;
    do
    {
	cnt = pdcb->GoingSRBCnt;
	psrb = pdcb->pGoingSRB;
	for( i=0; i<cnt; i++)
	{
	    psrb2 = psrb->pNextSRB;
	    pcmd = psrb->pcmd;
	    pcmd->result = DID_RESET << 16;

/*	    ReleaseSRB( pDCB, pSRB ); */

	    psrb->pNextSRB = pACB->pFreeSRB;
	    pACB->pFreeSRB = psrb;

	    pcmd->scsi_done( pcmd );
	    psrb  = psrb2;
	}
	pdcb->GoingSRBCnt = 0;;
	pdcb->pGoingSRB = NULL;
	pdcb->TagMask = 0;
	pdcb = pdcb->pNextDCB;
    }
    while( pdcb != pDCB );
}


static void
DC390W_ResetSCSIBus( PACB pACB )
{
    USHORT ioport;
    UCHAR  bval;
    ULONG  flags;

    save_flags(flags);
    cli();
    pACB->ACBFlag |= RESET_DEV;
    ioport = pACB->IOPortBase;
    bval = ABORT_OP;
    outb(bval,ioport+ISTAT);
    udelay(25);
    bval = 0;
    outb(bval,ioport+ISTAT);

    bval = ASSERT_RST;
    outb(bval,ioport+SCNTL1);
    udelay(25); 	 /* 25 us */
    bval = 0;
    outb(bval,ioport+SCNTL1);
    restore_flags(flags);
    return;
}



static void
DC390W_ResetSCSIBus2( PACB pACB )
{
    USHORT ioport;
    UCHAR  bval;
    ULONG  flags;

    save_flags(flags);
    cli();
    ioport = pACB->IOPortBase;
    bval = ASSERT_RST;
    outb(bval,ioport+SCNTL1);
    udelay(25); 	 /* 25 us */
    bval = 0;
    outb(bval,ioport+SCNTL1);
    restore_flags(flags);
    return;
}



static void
DC390W_ScsiRstDetect( PACB pACB )
{
    ULONG wlval, flags;
    USHORT ioport;
    UCHAR  bval;

    save_flags(flags);
    sti();
#ifdef DC390W_DEBUG0
    printk("Reset_Detect0,");
#endif
/* delay 1 sec */
    wlval = jiffies + HZ;
    while( jiffies < wlval );
/*  USHORT  i;
    for( i=0; i<1000; i++ )
	udelay(1000); */

    cli();
    ioport = pACB->IOPortBase;
    bval = inb(ioport+STEST3);
    bval |= CLR_SCSI_FIFO;
    outb(bval,ioport+STEST3);
    bval = CLR_DMA_FIFO;
    outb(bval,ioport+CTEST3);

    if( pACB->ACBFlag & RESET_DEV )
	pACB->ACBFlag |= RESET_DONE;
    else
    {
	pACB->ACBFlag |= RESET_DETECT;

	ResetDevParam( pACB );
/*	DoingSRB_Done( pACB ); ???? */
	RecoverSRB( pACB );
	pACB->pActiveDCB = NULL;
	wlval = pACB->jmp_reselect;
	outl(wlval,(ioport+DSP));
	pACB->ACBFlag = 0;
	DoWaitingSRB( pACB );
    }
    restore_flags(flags);
    return;
}


static void
RequestSense( PACB pACB, PDCB pDCB, PSRB pSRB )
{
    PSCSICMD  pcmd;

    pSRB->SRBFlag |= AUTO_REQSENSE;
    pSRB->Segment0[0][0] = *((PULONG) &(pSRB->CmdBlock[0]));
    pSRB->Segment0[0][1] = *((PULONG) &(pSRB->CmdBlock[4]));
    pSRB->Segment0[1][0] = *((PULONG) &(pSRB->CmdBlock[8]));
    pSRB->Segment0[1][1] = pSRB->__command[0] | (pSRB->SGcount << 8);
    pSRB->Segment0[2][0] = *((PULONG) &(pSRB->pSegmentList));
    pSRB->Segment0[2][1] = pSRB->XferredLen;
    pSRB->AdaptStatus = 0;
    pSRB->TargetStatus = 0;

    pcmd = pSRB->pcmd;

    pSRB->Segmentx.address = (PUCHAR) &(pcmd->sense_buffer);
    pSRB->Segmentx.length = sizeof(pcmd->sense_buffer);
    pSRB->pSegmentList = &pSRB->Segmentx;
    pSRB->SGcount = 1;

    *((PULONG) &(pSRB->CmdBlock[0])) = 0x00000003;
    pSRB->CmdBlock[1] = pDCB->IdentifyMsg << 5;
    *((PUSHORT) &(pSRB->CmdBlock[4])) = sizeof(pcmd->sense_buffer);
    pSRB->__command[0] = 6;
    PrepareSG( pACB, pDCB, pSRB );
    pSRB->XferredLen = 0;
    DC390W_StartSCSI( pACB, pDCB, pSRB );
    return;
}


static void
DC390W_MessageOut( PACB pACB )
{
    DC390W_FatalError( pACB );
}


static void
DC390W_FatalError( PACB pACB )
{
    PSRB  pSRB;
    PDCB  pDCB;
    ULONG flags;

#ifdef DC390W_DEBUG0
   printk("DC390W: Fatal Error!!\n");
#endif

    pDCB = pACB->pActiveDCB;
    pSRB = pDCB->pActiveSRB;
    if( pSRB->SRBState & SRB_UNEXPECT_RESEL )
    {
	save_flags(flags);
	cli();
	pSRB->SRBState &= ~SRB_UNEXPECT_RESEL;
	pSRB->pNextSRB = pACB->pFreeSRB;
	pACB->pFreeSRB = pSRB;
	pACB->pActiveDCB = NULL;
	pDCB->pActiveSRB = NULL;
	restore_flags(flags);
	DoWaitingSRB( pACB );
    }
    else
	DC390W_ResetSCSIBus(pACB);
    return;
}


static void
DC390W_Debug( PACB pACB )
{
   ULONG wlval;
   USHORT ioport;

   ioport = pACB->IOPortBase;
   wlval = inl(ioport+DSP);
   outl(wlval,(ioport+DSP));
   return;
}


