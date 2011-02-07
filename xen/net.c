/*
 *  Copyright (C) 2006-2009, 2011 Samuel Thibault <samuel.thibault@ens-lyon.org>
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>
#include <mach/mig_errors.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <vm/vm_kern.h>
#include <device/device_types.h>
#include <device/device_port.h>
#include <device/if_hdr.h>
#include <device/if_ether.h>
#include <device/net_io.h>
#include <device/device_reply.user.h>
#include <device/device_emul.h>
#include <intel/pmap.h>
#include <xen/public/io/netif.h>
#include <xen/public/memory.h>
#include <string.h>
#include <util/atoi.h>
#include "evt.h"
#include "store.h"
#include "net.h"
#include "grant.h"
#include "ring.h"
#include "time.h"
#include "xen.h"

/* Hypervisor part */

#define ADDRESS_SIZE 6
#define WINDOW __RING_SIZE((netif_rx_sring_t*)0, PAGE_SIZE)

/* Are we paranoid enough to not leak anything to backend? */
static const int paranoia = 0;

struct net_data {
	struct device	device;
	struct ifnet	ifnet;
	int		open_count;
	char		*backend;
	domid_t		domid;
	char		*vif;
	u_char		address[ADDRESS_SIZE];
	int		handle;
	ipc_port_t	port;
	netif_tx_front_ring_t	tx;
	netif_rx_front_ring_t	rx;
	void		*rx_buf[WINDOW];
	grant_ref_t	rx_buf_gnt[WINDOW];
	unsigned long	rx_buf_pfn[WINDOW];
	int		rx_copy;
	evtchn_port_t	evt;
	simple_lock_data_t lock;
	simple_lock_data_t pushlock;
};

static int n_vifs;
static struct net_data *vif_data;

struct device_emulation_ops hyp_net_emulation_ops;

int hextoi(char *cp, int *nump)
{
	int	number;
	char	*original;
	char	c;

	original = cp;
	for (number = 0, c = *cp | 0x20; (('0' <= c) && (c <= '9')) || (('a' <= c) && (c <= 'f')); c = *(++cp)) {
		number *= 16;
		if (c <= '9')
			number += c - '0';
		else
			number += c - 'a' + 10;
	}
	if (original == cp)
		*nump = -1;
	else
		*nump = number;
	return(cp - original);
}

static void enqueue_rx_buf(struct net_data *nd, int number) {
	unsigned reqn = nd->rx.req_prod_pvt++;
	netif_rx_request_t *req = RING_GET_REQUEST(&nd->rx, reqn);
	grant_ref_t gref;

	assert(number < WINDOW);

	req->id = number;
	if (nd->rx_copy) {
		/* Let domD write the data */
		gref = hyp_grant_give(nd->domid, nd->rx_buf_pfn[number], 0);
	} else {
		/* Accept pages from domD */
		gref = hyp_grant_accept_transfer(nd->domid, nd->rx_buf_pfn[number]);
		/* give back page */
		hyp_free_page(nd->rx_buf_pfn[number], nd->rx_buf[number]);
	}

	req->gref = nd->rx_buf_gnt[number] = gref;
}

static void hyp_net_intr(int unit) {
	ipc_kmsg_t kmsg;
	struct ether_header *eh;
	struct packet_header *ph;
	netif_rx_response_t *rx_rsp;
	netif_tx_response_t *tx_rsp;
	void *data;
	int len, more;
	struct net_data *nd = &vif_data[unit];

	simple_lock(&nd->lock);
	if ((nd->rx.sring->rsp_prod - nd->rx.rsp_cons) >= (WINDOW*3)/4)
		printf("window %ld a bit small!\n", WINDOW);

	more = RING_HAS_UNCONSUMED_RESPONSES(&nd->rx);
	while (more) {
		rmb(); /* make sure we see responses */
		rx_rsp = RING_GET_RESPONSE(&nd->rx, nd->rx.rsp_cons++);

		unsigned number = rx_rsp->id;
		assert(number < WINDOW);
		if (nd->rx_copy) {
			hyp_grant_takeback(nd->rx_buf_gnt[number]);
		} else {
			unsigned long mfn = hyp_grant_finish_transfer(nd->rx_buf_gnt[number]);
#ifdef	MACH_PSEUDO_PHYS
			mfn_list[nd->rx_buf_pfn[number]] = mfn;
#endif	/* MACH_PSEUDO_PHYS */
			pmap_map_mfn(nd->rx_buf[number], mfn);
		}

		kmsg = net_kmsg_get();
		if (!kmsg)
			/* gasp! Drop */
			goto drop;

		if (rx_rsp->status <= 0)
			switch (rx_rsp->status) {
				case NETIF_RSP_DROPPED:
					printf("Packet dropped\n");
					goto drop_kmsg;
				case NETIF_RSP_ERROR:
					printf("Packet error\n");
					goto drop_kmsg;
				case 0:
					printf("nul packet\n");
					goto drop_kmsg;
				default:
					printf("Unknown error %d\n", rx_rsp->status);
					goto drop_kmsg;
			}

		data = nd->rx_buf[number] + rx_rsp->offset;
		len = rx_rsp->status;

		eh = (void*) (net_kmsg(kmsg)->header);
		ph = (void*) (net_kmsg(kmsg)->packet);
		memcpy(eh, data, sizeof (struct ether_header));
		memcpy(ph + 1, data + sizeof (struct ether_header), len - sizeof(struct ether_header));
		RING_FINAL_CHECK_FOR_RESPONSES(&nd->rx, more);
		enqueue_rx_buf(nd, number);
		ph->type = eh->ether_type;
		ph->length  = len - sizeof(struct ether_header) + sizeof (struct packet_header);

		net_kmsg(kmsg)->sent = FALSE; /* Mark packet as received.  */

		net_packet(&nd->ifnet, kmsg, ph->length, ethernet_priority(kmsg));
		continue;

drop_kmsg:
		net_kmsg_put(kmsg);
drop:
		RING_FINAL_CHECK_FOR_RESPONSES(&nd->rx, more);
		enqueue_rx_buf(nd, number);
	}

	/* commit new requests */
	int notify;
	wmb(); /* make sure it sees requests */
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&nd->rx, notify);
	if (notify)
		hyp_event_channel_send(nd->evt);

	/* Now the tx side */
	more = RING_HAS_UNCONSUMED_RESPONSES(&nd->tx);
	spl_t s = splsched ();
	while (more) {
		rmb(); /* make sure we see responses */
		tx_rsp = RING_GET_RESPONSE(&nd->tx, nd->tx.rsp_cons++);
		switch (tx_rsp->status) {
			case NETIF_RSP_DROPPED:
				printf("Packet dropped\n");
				break;
			case NETIF_RSP_ERROR:
				printf("Packet error\n");
				break;
			case NETIF_RSP_OKAY:
				break;
			default:
				printf("Unknown error %d\n", tx_rsp->status);
				break;
		}
		thread_wakeup((event_t) hyp_grant_address(tx_rsp->id));
		thread_wakeup_one(nd);
		RING_FINAL_CHECK_FOR_RESPONSES(&nd->tx, more);
	}
	splx(s);

	simple_unlock(&nd->lock);
}

#define VIF_PATH "device/vif"
void hyp_net_init(void) {
	char **vifs, **vif;
	char *c;
	int i;
	int n; 
	int grant;
	char port_name[10];
	domid_t domid;
	evtchn_port_t evt;
	hyp_store_transaction_t t;
	vm_offset_t addr;
	struct net_data *nd;
	struct ifnet *ifp;
	netif_tx_sring_t *tx_ring;
	netif_rx_sring_t *rx_ring;

	vifs = hyp_store_ls(0, 1, VIF_PATH);
	if (!vifs) {
		printf("eth: No net device (%s). Hoping you don't need any\n", hyp_store_error);
		n_vifs = 0;
		return;
	}

	n = 0;
	for (vif = vifs; *vif; vif++)
		n++;

	vif_data = (void*) kalloc(n * sizeof(*vif_data));
	if (!vif_data) {
		printf("eth: No memory room for VIF\n");
		n_vifs = 0;
		return;
	}
	n_vifs = n;

	for (n = 0; n < n_vifs; n++) {
		nd = &vif_data[n];
		mach_atoi((u_char *) vifs[n], &nd->handle);
		if (nd->handle == MACH_ATOI_DEFAULT)
			continue;

		nd->open_count = -2;
		nd->vif = vifs[n];

		/* Get domain id of frontend driver.  */
		i = hyp_store_read_int(0, 5, VIF_PATH, "/", vifs[n], "/", "backend-id");
		if (i == -1)
			panic("eth: couldn't read frontend domid of VIF %s (%s)",vifs[n], hyp_store_error);
		nd->domid = domid = i;

		do {
			t = hyp_store_transaction_start();

			/* Get a page for tx_ring */
			if (kmem_alloc_wired(kernel_map, &addr, PAGE_SIZE) != KERN_SUCCESS)
				panic("eth: couldn't allocate space for store tx_ring");
			tx_ring = (void*) addr;
			SHARED_RING_INIT(tx_ring);
			FRONT_RING_INIT(&nd->tx, tx_ring, PAGE_SIZE);
			grant = hyp_grant_give(domid, atop(kvtophys(addr)), 0);

			/* and give it to backend.  */
			i = sprintf(port_name, "%u", grant);
			c = hyp_store_write(t, port_name, 5, VIF_PATH, "/", vifs[n], "/", "tx-ring-ref");
			if (!c)
				panic("eth: couldn't store tx_ring reference for VIF %s (%s)", vifs[n], hyp_store_error);
			kfree((vm_offset_t) c, strlen(c)+1);

			/* Get a page for rx_ring */
			if (kmem_alloc_wired(kernel_map, &addr, PAGE_SIZE) != KERN_SUCCESS)
				panic("eth: couldn't allocate space for store tx_ring");
			rx_ring = (void*) addr;
			SHARED_RING_INIT(rx_ring);
			FRONT_RING_INIT(&nd->rx, rx_ring, PAGE_SIZE);
			grant = hyp_grant_give(domid, atop(kvtophys(addr)), 0);

			/* and give it to backend.  */
			i = sprintf(port_name, "%u", grant);
			c = hyp_store_write(t, port_name, 5, VIF_PATH, "/", vifs[n], "/", "rx-ring-ref");
			if (!c)
				panic("eth: couldn't store rx_ring reference for VIF %s (%s)", vifs[n], hyp_store_error);
			kfree((vm_offset_t) c, strlen(c)+1);

			/* tell we need csums.  */
			c = hyp_store_write(t, "1", 5, VIF_PATH, "/", vifs[n], "/", "feature-no-csum-offload");
			if (!c)
				panic("eth: couldn't store feature-no-csum-offload reference for VIF %s (%s)", vifs[n], hyp_store_error);
			kfree((vm_offset_t) c, strlen(c)+1);

			/* Allocate an event channel and give it to backend.  */
			nd->evt = evt = hyp_event_channel_alloc(domid);
			i = sprintf(port_name, "%lu", evt);
			c = hyp_store_write(t, port_name, 5, VIF_PATH, "/", vifs[n], "/", "event-channel");
			if (!c)
				panic("eth: couldn't store event channel for VIF %s (%s)", vifs[n], hyp_store_error);
			kfree((vm_offset_t) c, strlen(c)+1);
			c = hyp_store_write(t, hyp_store_state_initialized, 5, VIF_PATH, "/", vifs[n], "/", "state");
			if (!c)
				panic("eth: couldn't store state for VIF %s (%s)", vifs[n], hyp_store_error);
			kfree((vm_offset_t) c, strlen(c)+1);
		} while ((!hyp_store_transaction_stop(t)));
	/* TODO randomly wait? */

		c = hyp_store_read(0, 5, VIF_PATH, "/", vifs[n], "/", "backend");
		if (!c)
			panic("eth: couldn't get path to VIF %s backend (%s)", vifs[n], hyp_store_error);
		nd->backend = c;

		while(1) {
			i = hyp_store_read_int(0, 3, nd->backend, "/", "state");
			if (i == MACH_ATOI_DEFAULT)
				panic("can't read state from %s", nd->backend);
			if (i == XenbusStateInitWait)
				break;
			hyp_yield();
		}

		c = hyp_store_read(0, 3, nd->backend, "/", "mac");
		if (!c)
			panic("eth: couldn't get VIF %s's mac (%s)", vifs[n], hyp_store_error);

		for (i=0; ; i++) {
			int val;
			hextoi(&c[3*i], &val);
			if (val == -1)
				panic("eth: couldn't understand %dth number of VIF %s's mac %s", i, vifs[n], c);
			nd->address[i] = val;
			if (i==ADDRESS_SIZE-1)
				break;
			if (c[3*i+2] != ':')
				panic("eth: couldn't understand %dth separator of VIF %s's mac %s", i, vifs[n], c);
		}
		kfree((vm_offset_t) c, strlen(c)+1);

		printf("eth%d: dom%d's VIF %s ", n, domid, vifs[n]);
		for (i=0; ; i++) {
			printf("%02x", nd->address[i]);
			if (i==ADDRESS_SIZE-1)
				break;
			printf(":");
		}
		printf("\n");

		nd->rx_copy = hyp_store_read_int(0, 3, nd->backend, "/", "feature-rx-copy");
		if (nd->rx_copy == 1) {
			c = hyp_store_write(0, "1", 5, VIF_PATH, "/", vifs[n], "/", "request-rx-copy");
			if (!c)
				panic("eth: couldn't request rx copy feature for VIF %s (%s)", vifs[n], hyp_store_error);
		} else
			nd->rx_copy = 0;

		c = hyp_store_write(0, hyp_store_state_connected, 5, VIF_PATH, "/", nd->vif, "/", "state");
		if (!c)
			panic("couldn't store state for eth%d (%s)", nd - vif_data, hyp_store_error);
		kfree((vm_offset_t) c, strlen(c)+1);

		while(1) {
			i = hyp_store_read_int(0, 3, nd->backend, "/", "state");
			if (i == MACH_ATOI_DEFAULT)
				panic("can't read state from %s", nd->backend);
			if (i == XenbusStateConnected)
				break;
			hyp_yield();
		}

		/* Get a page for packet reception */
		for (i= 0; i<WINDOW; i++) {
			if (kmem_alloc_wired(kernel_map, &addr, PAGE_SIZE) != KERN_SUCCESS)
				panic("eth: couldn't allocate space for store tx_ring");
			nd->rx_buf[i] = (void*)phystokv(kvtophys(addr));
			nd->rx_buf_pfn[i] = atop(kvtophys((vm_offset_t)nd->rx_buf[i]));
			if (!nd->rx_copy) {
				if (hyp_do_update_va_mapping(kvtolin(addr), 0, UVMF_INVLPG|UVMF_ALL))
					panic("eth: couldn't clear rx kv buf %d at %p", i, addr);
			}
			/* and enqueue it to backend.  */
			enqueue_rx_buf(nd, i);
		}
		int notify;
		wmb(); /* make sure it sees requests */
		RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&nd->rx, notify);
		if (notify)
			hyp_event_channel_send(nd->evt);


		nd->open_count = -1;
		nd->device.emul_ops = &hyp_net_emulation_ops;
		nd->device.emul_data = nd;
		simple_lock_init(&nd->lock);
		simple_lock_init(&nd->pushlock);

		ifp = &nd->ifnet;
		ifp->if_unit = n;
		ifp->if_flags = IFF_UP | IFF_RUNNING;
		ifp->if_header_size = 14;
		ifp->if_header_format = HDR_ETHERNET;
		/* Set to the maximum that we can handle in device_write.  */
		ifp->if_mtu = PAGE_SIZE - ifp->if_header_size;
		ifp->if_address_size = ADDRESS_SIZE;
		ifp->if_address = (void*) nd->address;
		if_init_queues (ifp);

		/* Now we can start receiving */
		hyp_evt_handler(evt, hyp_net_intr, n, SPL6);
	}
}

static ipc_port_t
dev_to_port(void *d)
{
	struct net_data *b = d;
	if (!d)
		return IP_NULL;
	return ipc_port_make_send(b->port);
}

static int
device_close(void *devp)
{
	struct net_data *nd = devp;
	if (--nd->open_count < 0)
		panic("too many closes on eth%d", nd - vif_data);
	printf("close, eth%d count %d\n",nd-vif_data,nd->open_count);
	if (nd->open_count)
		return 0;
	ipc_kobject_set(nd->port, IKO_NULL, IKOT_NONE);
	ipc_port_dealloc_kernel(nd->port);
	return 0;
}

static io_return_t
device_open (ipc_port_t reply_port, mach_msg_type_name_t reply_port_type,
	    dev_mode_t mode, char *name, device_t *devp /* out */)
{
	int i, n, err = 0;
	ipc_port_t port, notify;
	struct net_data *nd;

	if (name[0] != 'e' || name[1] != 't' || name[2] != 'h' || name[3] < '0' || name[3] > '9')
		return D_NO_SUCH_DEVICE;
	i = mach_atoi((u_char *) &name[3], &n);
	if (n == MACH_ATOI_DEFAULT)
		return D_NO_SUCH_DEVICE;
	if (name[3 + i])
		return D_NO_SUCH_DEVICE;
	if (n >= n_vifs)
		return D_NO_SUCH_DEVICE;
	nd = &vif_data[n];
	if (nd->open_count == -2)
		/* couldn't be initialized */
		return D_NO_SUCH_DEVICE;

	if (nd->open_count >= 0) {
		*devp = &nd->device ;
		nd->open_count++ ;
		printf("re-open, eth%d count %d\n",nd-vif_data,nd->open_count);
		return D_SUCCESS;
	}

	nd->open_count = 1;
	printf("eth%d count %d\n",nd-vif_data,nd->open_count);

	port = ipc_port_alloc_kernel();
	if (port == IP_NULL) {
		err = KERN_RESOURCE_SHORTAGE;
		goto out;
	}
	nd->port = port;

	*devp = &nd->device;

	ipc_kobject_set (port, (ipc_kobject_t) &nd->device, IKOT_DEVICE);

	notify = ipc_port_make_sonce (nd->port);
	ip_lock (nd->port);
	ipc_port_nsrequest (nd->port, 1, notify, &notify);
	assert (notify == IP_NULL);

out:
	if (IP_VALID (reply_port))
		ds_device_open_reply (reply_port, reply_port_type, D_SUCCESS, dev_to_port(nd));
	else
		device_close(nd);
	return MIG_NO_REPLY;
}

static io_return_t
device_write(void *d, ipc_port_t reply_port,
	    mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	    recnum_t bn, io_buf_ptr_t data, unsigned int count,
	    int *bytes_written)
{
	vm_map_copy_t copy = (vm_map_copy_t) data;
	grant_ref_t gref;
	struct net_data *nd = d;
	struct ifnet *ifp = &nd->ifnet;
	netif_tx_request_t *req;
	unsigned reqn;
	vm_offset_t offset;
	vm_page_t m;
	vm_size_t size;

	/* The maximum that we can handle.  */
	assert(ifp->if_header_size + ifp->if_mtu <= PAGE_SIZE);

	if (count < ifp->if_header_size ||
	    count > ifp->if_header_size + ifp->if_mtu)
		return D_INVALID_SIZE;

  	assert(copy->type == VM_MAP_COPY_PAGE_LIST);

	assert(copy->cpy_npages <= 2);
	assert(copy->cpy_npages >= 1);

	offset = copy->offset & PAGE_MASK;
	if (paranoia || copy->cpy_npages == 2) {
		/* have to copy :/ */
		while ((m = vm_page_grab(FALSE)) == 0)
			VM_PAGE_WAIT (0);
		assert (! m->active && ! m->inactive);
		m->busy = TRUE;

		if (copy->cpy_npages == 1)
			size = count;
		else
			size = PAGE_SIZE - offset;

		memcpy((void*)phystokv(m->phys_addr), (void*)phystokv(copy->cpy_page_list[0]->phys_addr + offset), size);
		if (copy->cpy_npages == 2)
			memcpy((void*)phystokv(m->phys_addr + size), (void*)phystokv(copy->cpy_page_list[1]->phys_addr), count - size);

		offset = 0;
	} else
		m = copy->cpy_page_list[0];

	/* allocate a request */
	spl_t spl = splimp();
	while (1) {
		simple_lock(&nd->lock);
		if (!RING_FULL(&nd->tx))
			break;
		thread_sleep(nd, &nd->lock, FALSE);
	}
	mb();
	reqn = nd->tx.req_prod_pvt++;;
	simple_lock(&nd->pushlock);
	simple_unlock(&nd->lock);
	(void) splx(spl);

	req = RING_GET_REQUEST(&nd->tx, reqn);
	req->gref = gref = hyp_grant_give(nd->domid, atop(m->phys_addr), 1);
	req->offset = offset;
	req->flags = 0;
	req->id = gref;
	req->size = count;

	assert_wait(hyp_grant_address(gref), FALSE);

	int notify;
	wmb(); /* make sure it sees requests */
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&nd->tx, notify);
	if (notify)
		hyp_event_channel_send(nd->evt);
	simple_unlock(&nd->pushlock);

	thread_block(NULL);
	
	hyp_grant_takeback(gref);

	/* Send packet to filters.  */
	{
	  struct packet_header *packet;
	  struct ether_header *header;
	  ipc_kmsg_t kmsg;
	
	  kmsg = net_kmsg_get ();
	
	  if (kmsg != IKM_NULL)
	    {
	      /* Suitable for Ethernet only.  */
	      header = (struct ether_header *) (net_kmsg (kmsg)->header);
	      packet = (struct packet_header *) (net_kmsg (kmsg)->packet);
	      memcpy (header, (void*)phystokv(m->phys_addr + offset), sizeof (struct ether_header));
	
	      /* packet is prefixed with a struct packet_header,
	         see include/device/net_status.h.  */
	      memcpy (packet + 1, (void*)phystokv(m->phys_addr + offset + sizeof (struct ether_header)),
	              count - sizeof (struct ether_header));
	      packet->length = count - sizeof (struct ether_header)
	                       + sizeof (struct packet_header);
	      packet->type = header->ether_type;
	      net_kmsg (kmsg)->sent = TRUE; /* Mark packet as sent.  */
	      spl_t s = splimp ();
	      net_packet (&nd->ifnet, kmsg, packet->length,
	                  ethernet_priority (kmsg));
	      splx (s);
	    }
	}

	if (paranoia || copy->cpy_npages == 2)
		VM_PAGE_FREE(m);

	vm_map_copy_discard (copy);

	*bytes_written = count;

	if (IP_VALID(reply_port))
		ds_device_write_reply (reply_port, reply_port_type, 0, count);

	return MIG_NO_REPLY;
}

static io_return_t
device_get_status(void *d, dev_flavor_t flavor, dev_status_t status,
		  mach_msg_type_number_t *status_count)
{
	struct net_data *nd = d;

	return net_getstat (&nd->ifnet, flavor, status, status_count);
}

static io_return_t
device_set_status(void *d, dev_flavor_t flavor, dev_status_t status,
		  mach_msg_type_number_t count)
{
	struct net_data *nd = d;

	switch (flavor)
	{
		default:
			printf("TODO: net_%s(%p, 0x%x)\n", __func__, nd, flavor);
			return D_INVALID_OPERATION;
	}
	return D_SUCCESS;
}

static io_return_t
device_set_filter(void *d, ipc_port_t port, int priority,
		  filter_t * filter, unsigned filter_count)
{
	struct net_data *nd = d;

	if (!nd)
		return D_NO_SUCH_DEVICE;

	return net_set_filter (&nd->ifnet, port, priority, filter, filter_count);
}

struct device_emulation_ops hyp_net_emulation_ops = {
	NULL,	/* dereference */
	NULL,	/* deallocate */
	dev_to_port,
	device_open,
	device_close,
	device_write,
	NULL,	/* write_inband */
	NULL,
	NULL,	/* read_inband */
	device_set_status,	/* set_status */
	device_get_status,
	device_set_filter,	/* set_filter */
	NULL,	/* map */
	NULL,	/* no_senders */
	NULL,	/* write_trap */
	NULL,	/* writev_trap */
};
