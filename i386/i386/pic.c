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

/*
 * Copyright (C) 1995 Shantanu Goel.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>
#include <kern/printf.h>
#include <i386/ipl.h>
#include <i386/pic.h>
#include <i386/spl.h>
#include <i386/pio.h>

spl_t	curr_ipl[NCPUS] = {0};
int	curr_pic_mask;
int	spl_init = 0;

int	iunit[NINTR] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

unsigned short	master_icw, master_ocw, slaves_icw, slaves_ocw;

u_short PICM_ICW1, PICM_OCW1, PICS_ICW1, PICS_OCW1 ;
u_short PICM_ICW2, PICM_OCW2, PICS_ICW2, PICS_OCW2 ;
u_short PICM_ICW3, PICM_OCW3, PICS_ICW3, PICS_OCW3 ;
u_short PICM_ICW4, PICS_ICW4 ;

/*
** picinit() - This routine
**		* Establishes a table of interrupt vectors
**		* Establishes location of PICs in the system
**		* Unmasks all interrupts in the PICs
**		* Initialises them
**
**	At this stage the interrupt functionality of this system should be
**	complete.
*/

/*
** Initialise the PICs , master first, then the slave.
** All the register field definitions are described in pic.h also
** the settings of these fields for the various registers are selected.
*/

void
picinit(void)
{

	asm("cli");

	/*
	** 0. Initialise the current level to match cli() 
	*/
	int i;

	for (i = 0; i < NCPUS; i++)
		curr_ipl[i] = SPLHI;
	curr_pic_mask = 0;

	/*
	** 1. Generate addresses to each PIC port.
	*/

	master_icw = PIC_MASTER_ICW;
	master_ocw = PIC_MASTER_OCW;
	slaves_icw = PIC_SLAVE_ICW;
	slaves_ocw = PIC_SLAVE_OCW;

	/*
	** 2. Select options for each ICW and each OCW for each PIC.
	*/

	PICM_ICW1 =
 	(ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8 | CASCADE_MODE | ICW4__NEEDED);

	PICS_ICW1 =
 	(ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8 | CASCADE_MODE | ICW4__NEEDED);

	PICM_ICW2 = PICM_VECTBASE;
	PICS_ICW2 = PICS_VECTBASE;

#ifdef	AT386
	PICM_ICW3 = ( SLAVE_ON_IR2 );
	PICS_ICW3 = ( I_AM_SLAVE_2 );
#endif	/* AT386 */

	PICM_ICW4 =
 	(SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD | I8086_EMM_MOD);
	PICS_ICW4 =
 	(SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD | I8086_EMM_MOD);

	PICM_OCW1 = (curr_pic_mask & 0x00FF);
	PICS_OCW1 = ((curr_pic_mask & 0xFF00)>>8);

	PICM_OCW2 = NON_SPEC_EOI;
	PICS_OCW2 = NON_SPEC_EOI;

	PICM_OCW3 = (OCW_TEMPLATE | READ_NEXT_RD | READ_IR_ONRD );
	PICS_OCW3 = (OCW_TEMPLATE | READ_NEXT_RD | READ_IR_ONRD );

	/*
	** 3.	Initialise master - send commands to master PIC
	*/

	outb ( master_icw, PICM_ICW1 );
	outb ( master_ocw, PICM_ICW2 );
	outb ( master_ocw, PICM_ICW3 );
	outb ( master_ocw, PICM_ICW4 );

	outb ( master_ocw, PICM_MASK );
	outb ( master_icw, PICM_OCW3 );

	/*
	** 4.	Initialise slave - send commands to slave PIC
	*/

	outb ( slaves_icw, PICS_ICW1 );
	outb ( slaves_ocw, PICS_ICW2 );
	outb ( slaves_ocw, PICS_ICW3 );
	outb ( slaves_ocw, PICS_ICW4 );


	outb ( slaves_ocw, PICS_OCW1 );
	outb ( slaves_icw, PICS_OCW3 );

	/*
	** 5. Initialise interrupts
	*/
	outb ( master_ocw, PICM_OCW1 );

}

void
intnull(int unit_dev)
{
	static char warned[NINTR];

	if (unit_dev >= NINTR)
		printf("Unknown interrupt %d\n", unit_dev);
	else if (!warned[unit_dev])
	{
		printf("intnull(%d)\n", unit_dev);
		warned[unit_dev] = 1;
	}

}

/*
 * Mask a PIC IRQ.
 */
void
mask_irq (unsigned int irq_nr)
{
	int new_pic_mask = curr_pic_mask | 1 << irq_nr;

	if (curr_pic_mask != new_pic_mask)
	{
		curr_pic_mask = new_pic_mask;
		if (irq_nr < 8)
		{
			outb (PIC_MASTER_OCW, curr_pic_mask & 0xff);
		}
		else
		{
			outb (PIC_SLAVE_OCW, curr_pic_mask >> 8);
		}
	}
}

/*
 * Unmask a PIC IRQ.
 */
void
unmask_irq (unsigned int irq_nr)
{
	int mask;
	int new_pic_mask;

	mask = 1 << irq_nr;
	if (irq_nr >= 8)
	{
		mask |= 1 << 2;
	}

	new_pic_mask = curr_pic_mask & ~mask;

	if (curr_pic_mask != new_pic_mask)
	{
		curr_pic_mask = new_pic_mask;
		if (irq_nr < 8)
		{
			outb (PIC_MASTER_OCW, curr_pic_mask & 0xff);
		}
		else
		{
			outb (PIC_SLAVE_OCW, curr_pic_mask >> 8);
		}
	}
}

