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
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * disk.h
 */

#ifndef _DISK_H_
#define _DISK_H_

#define V_NUMPAR        16              /* maximum number of partitions */

#define VTOC_SANE       0x600DDEEE      /* Indicates a sane VTOC */
#define PDLOCATION	29		/* location of VTOC */

#define	LBLLOC		1		/* label block for xxxbsd */

struct localpartition	{
	u_int 	p_flag;			/*permision flags*/
	long	p_start;		/*physical start sector no of partition*/
	long	p_size;			/*# of physical sectors in partition*/
};
typedef struct localpartition localpartition_t;

struct evtoc {
	u_int 	fill0[6];
	u_int 	cyls;			/*number of cylinders per drive*/
	u_int 	tracks;			/*number tracks per cylinder*/
	u_int 	sectors;		/*number sectors per track*/
	u_int 	fill1[13];
	u_int 	version;		/*layout version*/
	u_int 	alt_ptr;		/*byte offset of alternates table*/
	u_short	alt_len;		/*byte length of alternates table*/
	u_int 	sanity;			/*to verify vtoc sanity*/
	u_int 	xcyls;			/*number of cylinders per drive*/
	u_int 	xtracks;		/*number tracks per cylinder*/
	u_int 	xsectors;		/*number sectors per track*/
	u_short	nparts;			/*number of partitions*/
	u_short	fill2;			/*pad for 286 compiler*/
	char	label[40];
	struct localpartition part[V_NUMPAR];/*partition headers*/
	char	fill[512-352];
};

#endif /* _DISK_H_ */
