/* 
 * Mach Operating System
 * Copyright (c) 1993,1991,1990,1989,1988 Carnegie Mellon University
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
 * Random device subroutines and stubs.
 */

#include <kern/debug.h>
#include <kern/printf.h>
#include <vm/vm_kern.h>
#include <vm/vm_user.h>
#include <device/buf.h>
#include <device/if_hdr.h>
#include <device/if_ether.h>
#include <device/subrs.h>



/*
 * Print out disk name and block number for hard disk errors.
 */
void harderr(const io_req_t ior, const char *cp)
{
	printf("%s%d%c: hard error sn%lu ",
	       cp,
	       minor(ior->io_unit) >> 3,
	       'a' + (minor(ior->io_unit) & 0x7),
	       ior->io_recnum);
}

/*
 * Convert Ethernet address to printable (loggable) representation.
 */
char *
ether_sprintf(const u_char *ap)
{
	int i;
	static char etherbuf[18];
	char *cp = etherbuf;
	static char digits[] = "0123456789abcdef";

	for (i = 0; i < 6; i++) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = ':';
	}
	*--cp = 0;
	return (etherbuf);
}

/*
 * Initialize send and receive queues on an interface.
 */
void if_init_queues(struct ifnet *ifp)
{
	IFQ_INIT(&ifp->if_snd);
	queue_init(&ifp->if_rcv_port_list);
	queue_init(&ifp->if_snd_port_list);
	simple_lock_init(&ifp->if_rcv_port_list_lock);
	simple_lock_init(&ifp->if_snd_port_list_lock);
}


/*
 * Compatibility with BSD device drivers.
 */
void sleep(vm_offset_t channel, int priority)
{
	assert_wait((event_t) channel, FALSE);	/* not interruptible XXX */
	thread_block((void (*)()) 0);
}

void wakeup(vm_offset_t channel)
{
	thread_wakeup((event_t) channel);
}

io_req_t
geteblk(int size)
{
	io_req_t	ior;

	io_req_alloc(ior, 0);
	ior->io_device = (mach_device_t)0;
	ior->io_unit = 0;
	ior->io_op = 0;
	ior->io_mode = 0;
	ior->io_recnum = 0;
	ior->io_count = size;
	ior->io_residual = 0;
	ior->io_error = 0;

	size = round_page(size);
	ior->io_alloc_size = size;
	if (kmem_alloc(kernel_map, (vm_offset_t *)&ior->io_data, size)
		!= KERN_SUCCESS)
		    panic("geteblk");

	return (ior);
}

void brelse(io_req_t ior)
{
	(void) vm_deallocate(kernel_map,
			(vm_offset_t) ior->io_data,
			ior->io_alloc_size);
	io_req_free(ior);
}
