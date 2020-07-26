/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
Copyright (c) 1988,1989 Prime Computer, Inc.  Natick, MA 01760
All Rights Reserved.

Permission to use, copy, modify, and distribute this
software and its documentation for any purpose and
without fee is hereby granted, provided that the above
copyright notice appears in all copies and that both the
copyright notice and this permission notice appear in
supporting documentation, and that the name of Prime
Computer, Inc. not be used in advertising or publicity
pertaining to distribution of the software without
specific, written prior permission.

THIS SOFTWARE IS PROVIDED "AS IS", AND PRIME COMPUTER,
INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN
NO EVENT SHALL PRIME COMPUTER, INC.  BE LIABLE FOR ANY
SPECIAL, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN ACTION OF CONTRACT, NEGLIGENCE, OR
OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef	_I386_PIC_H_
#define	_I386_PIC_H_

#define NINTR	0x10
#define	NPICS	0x02

/*
** The following are definitions used to locate the PICs in the system
*/

#if	defined(AT386)
#define ADDR_PIC_BASE		0x20
#define OFF_ICW			0x00
#define OFF_OCW			0x01
#define SIZE_PIC		0x80
#endif	/* defined(AT386) */

#define PIC_MASTER_ICW		(ADDR_PIC_BASE + OFF_ICW)
#define PIC_MASTER_OCW		(ADDR_PIC_BASE + OFF_OCW)
#define PIC_SLAVE_ICW		(PIC_MASTER_ICW + SIZE_PIC)
#define PIC_SLAVE_OCW		(PIC_MASTER_OCW + SIZE_PIC)

/*
** The following banks of definitions ICW1, ICW2, ICW3, and ICW4 are used
** to define the fields of the various ICWs for initialisation of the PICs
*/

/*
**	ICW1
*/

#define ICW_TEMPLATE		0x10

#define LEVL_TRIGGER		0x08
#define EDGE_TRIGGER		0x00
#define ADDR_INTRVL4		0x04
#define ADDR_INTRVL8		0x00
#define SINGLE__MODE		0x02
#define CASCADE_MODE		0x00
#define ICW4__NEEDED		0x01
#define NO_ICW4_NEED		0x00

/*
**	ICW2
*/

#if	defined(AT386)
#define	PICM_VECTBASE		0x40
#define PICS_VECTBASE		PICM_VECTBASE + 0x08
#endif	/* defined(AT386) */

/*
**	ICW3
*/

#define SLAVE_ON_IR0		0x01
#define SLAVE_ON_IR1		0x02
#define SLAVE_ON_IR2		0x04
#define SLAVE_ON_IR3		0x08
#define SLAVE_ON_IR4		0x10
#define SLAVE_ON_IR5		0x20
#define SLAVE_ON_IR6		0x40
#define SLAVE_ON_IR7		0x80

#define I_AM_SLAVE_0		0x00
#define I_AM_SLAVE_1		0x01
#define I_AM_SLAVE_2		0x02
#define I_AM_SLAVE_3		0x03
#define I_AM_SLAVE_4		0x04
#define I_AM_SLAVE_5		0x05
#define I_AM_SLAVE_6		0x06
#define I_AM_SLAVE_7		0x07

/*
**	ICW4
*/

#define SNF_MODE_ENA		0x10
#define SNF_MODE_DIS		0x00
#define BUFFERD_MODE		0x08
#define NONBUFD_MODE		0x00
#define AUTO_EOI_MOD		0x02
#define NRML_EOI_MOD		0x00
#define I8086_EMM_MOD		0x01
#define SET_MCS_MODE		0x00

/*
**	OCW1
*/
#define PICM_MASK		0xFF
#define	PICS_MASK		0xFF
/*
**	OCW2
*/

#define NON_SPEC_EOI		0x20
#define SPECIFIC_EOI		0x30
#define ROT_NON_SPEC		0x50
#define SET_ROT_AEOI		0x40
#define RSET_ROTAEOI		0x00
#define ROT_SPEC_EOI		0x70
#define SET_PRIORITY		0x60
#define NO_OPERATION		0x20

#define SEND_EOI_IR0		0x00
#define SEND_EOI_IR1		0x01
#define SEND_EOI_IR2		0x02
#define SEND_EOI_IR3		0x03
#define SEND_EOI_IR4		0x04
#define SEND_EOI_IR5		0x05
#define SEND_EOI_IR6		0x06
#define SEND_EOI_IR7		0x07

/*
**	OCW3
*/

#define OCW_TEMPLATE		0x08
#define SPECIAL_MASK		0x40
#define MASK_MDE_SET		0x20
#define MASK_MDE_RST		0x00
#define POLL_COMMAND		0x04
#define NO_POLL_CMND		0x00
#define READ_NEXT_RD		0x02
#define READ_IR_ONRD		0x00
#define READ_IS_ONRD		0x01

#ifndef __ASSEMBLER__
extern void form_pic_mask (void);
extern void picinit (void);
extern int curr_pic_mask;
extern int pic_mask[];
extern void prtnull(int unit);
extern void intnull(int unit);
#endif /* __ASSEMBLER__ */

#endif	/* _I386_PIC_H_ */
