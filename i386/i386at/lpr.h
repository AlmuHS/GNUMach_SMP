/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 *	Parallel port printer driver v1.0
 *	All rights reserved.
 */ 

#ifndef _LPRREG_H_
#define _LPRREG_H_
  
#define DATA(addr)	(addr + 0)
#define STATUS(addr)	(addr + 1)
#define INTR_ENAB(addr)	(addr + 2)

extern void lprintr(int unit);
int lprprobe(vm_offset_t port, struct bus_ctlr *dev);
void lprstop(struct tty *tp, int flags);
void lprstart(struct tty *tp);
void lprattach(struct bus_device *dev);

extern io_return_t
lprgetstat(
	dev_t		dev,
	dev_flavor_t	flavor,
	dev_status_t	data,
	mach_msg_type_number_t	*count);

extern io_return_t
lprsetstat(
	dev_t		dev,
	dev_flavor_t	flavor,
	dev_status_t	data,
	mach_msg_type_number_t	count);

void lprpr_addr(unsigned short addr);

extern int lpropen(dev_t dev, int flag, io_req_t ior);
extern void lprclose(dev_t dev, int flag);
extern int lprread(dev_t dev, io_req_t ior);
extern int lprwrite(dev_t	dev, io_req_t ior);
extern int lprportdeath(dev_t dev, mach_port_t port);

#endif /* _LPRREG_H_ */
