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
#include <vm/vm_user.h>
#include <device/device_types.h>
#include <device/device_port.h>
#include <device/disk_status.h>
#include <device/device_reply.user.h>
#include <device/device_emul.h>
#include <device/ds_routines.h>
#include <xen/public/io/blkif.h>
#include <xen/evt.h>
#include <string.h>
#include <util/atoi.h>
#include "store.h"
#include "block.h"
#include "grant.h"
#include "ring.h"
#include "xen.h"

/* Hypervisor part */

struct block_data {
	struct device	device;
	char		*name;
	int		open_count;
	char		*backend;
	domid_t		domid;
	char		*vbd;
	int		handle;
	unsigned	info;
	dev_mode_t	mode;
	unsigned	sector_size;
	unsigned long	nr_sectors;
	ipc_port_t	port;
	blkif_front_ring_t	ring;
	evtchn_port_t	evt;
	simple_lock_data_t lock;
	simple_lock_data_t pushlock;
};

static int n_vbds;
static struct block_data *vbd_data;

struct device_emulation_ops hyp_block_emulation_ops;

static void hyp_block_intr(int unit) {
	struct block_data *bd = &vbd_data[unit];
	blkif_response_t *rsp;
	int more;
	io_return_t *err;

	simple_lock(&bd->lock);
	more = RING_HAS_UNCONSUMED_RESPONSES(&bd->ring);
	while (more) {
		rmb(); /* make sure we see responses */
		rsp = RING_GET_RESPONSE(&bd->ring, bd->ring.rsp_cons++);
		err = (void *) (unsigned long) rsp->id;
		switch (rsp->status) {
		case BLKIF_RSP_ERROR:
			*err = D_IO_ERROR;
			break;
		case BLKIF_RSP_OKAY:
			break;
		default:
			printf("Unrecognized blkif status %d\n", rsp->status);
			goto drop;
		}
		thread_wakeup(err);
drop:
		thread_wakeup_one(bd);
		RING_FINAL_CHECK_FOR_RESPONSES(&bd->ring, more);
	}
	simple_unlock(&bd->lock);
}

#define VBD_PATH "device/vbd"
void hyp_block_init(void) {
	char **vbds, **vbd;
	char *c;
	int i, disk, partition;
	int n; 
	int grant;
	char port_name[10];
	char *prefix;
	char device_name[32];
	domid_t domid;
	evtchn_port_t evt;
	hyp_store_transaction_t t;
	vm_offset_t addr;
	struct block_data *bd;
	blkif_sring_t	*ring;

	vbds = hyp_store_ls(0, 1, VBD_PATH);
	if (!vbds) {
		printf("hd: No block device (%s). Hoping you don't need any\n", hyp_store_error);
		n_vbds = 0;
		return;
	}

	n = 0;
	for (vbd = vbds; *vbd; vbd++)
		n++;

	vbd_data = (void*) kalloc(n * sizeof(*vbd_data));
	if (!vbd_data) {
		printf("hd: No memory room for VBD\n");
		n_vbds = 0;
		return;
	}
	n_vbds = n;

	for (n = 0; n < n_vbds; n++) {
		bd = &vbd_data[n];
		mach_atoi((u_char *) vbds[n], &bd->handle);
		if (bd->handle == MACH_ATOI_DEFAULT)
			continue;

		bd->open_count = -2;
		bd->vbd = vbds[n];

		/* Get virtual number.  */
		i = hyp_store_read_int(0, 5, VBD_PATH, "/", vbds[n], "/", "virtual-device");
		if (i == -1)
			panic("hd: couldn't virtual device of VBD %s\n",vbds[n]);
		if ((i >> 28) == 1) {
			/* xvd, new format */
			prefix = "xvd";
			disk = (i >> 8) & ((1 << 20) - 1);
			partition = i & ((1 << 8) - 1);
		} else if ((i >> 8) == 202) {
			/* xvd, old format */
			prefix = "xvd";
			disk = (i >> 4) & ((1 << 4) - 1);
			partition = i & ((1 << 4) - 1);
		} else if ((i >> 8) == 8) {
			/* SCSI */
			prefix = "sd";
			disk = (i >> 4) & ((1 << 4) - 1);
			partition = i & ((1 << 4) - 1);
		} else if ((i >> 8) == 3) {
			/* IDE primary */
			prefix = "hd";
			disk = (i >> 6) & ((1 << 2) - 1);
			partition = i & ((1 << 6) - 1);
		} else if ((i >> 8) == 22) {
			/* IDE secondary */
			prefix = "hd";
			disk = ((i >> 6) & ((1 << 2) - 1)) + 2;
			partition = i & ((1 << 6) - 1);
		} else if ((i >> 8) == 33) {
			/* IDE 3 */
			prefix = "hd";
			disk = ((i >> 6) & ((1 << 2) - 1)) + 4;
			partition = i & ((1 << 6) - 1);
		} else if ((i >> 8) == 34) {
			/* IDE 4 */
			prefix = "hd";
			disk = ((i >> 6) & ((1 << 2) - 1)) + 6;
			partition = i & ((1 << 6) - 1);
		} else if ((i >> 8) == 56) {
			/* IDE 5 */
			prefix = "hd";
			disk = ((i >> 6) & ((1 << 2) - 1)) + 8;
			partition = i & ((1 << 6) - 1);
		} else if ((i >> 8) == 57) {
			/* IDE 6 */
			prefix = "hd";
			disk = ((i >> 6) & ((1 << 2) - 1)) + 10;
			partition = i & ((1 << 6) - 1);
		} else if ((i >> 8) == 88) {
			/* IDE 7 */
			prefix = "hd";
			disk = ((i >> 6) & ((1 << 2) - 1)) + 12;
			partition = i & ((1 << 6) - 1);
		} else if ((i >> 8) == 89) {
			/* IDE 8 */
			prefix = "hd";
			disk = ((i >> 6) & ((1 << 2) - 1)) + 14;
			partition = i & ((1 << 6) - 1);
		} else if ((i >> 8) == 90) {
			/* IDE 9 */
			prefix = "hd";
			disk = ((i >> 6) & ((1 << 2) - 1)) + 16;
			partition = i & ((1 << 6) - 1);
		} else if ((i >> 8) == 91) {
			/* IDE 10 */
			prefix = "hd";
			disk = ((i >> 6) & ((1 << 2) - 1)) + 18;
			partition = i & ((1 << 6) - 1);
		} else {
			printf("unsupported VBD number %d\n", i);
			continue;
		}
		if (partition)
			sprintf(device_name, "%s%us%u", prefix, disk, partition);
		else
			sprintf(device_name, "%s%u", prefix, disk);
		bd->name = (char*) kalloc(strlen(device_name));
		strcpy(bd->name, device_name);

		/* Get domain id of backend driver.  */
		i = hyp_store_read_int(0, 5, VBD_PATH, "/", vbds[n], "/", "backend-id");
		if (i == -1)
			panic("%s: couldn't read backend domid (%s)", device_name, hyp_store_error);
		bd->domid = domid = i;

		do {
			t = hyp_store_transaction_start();

			/* Get a page for ring */
			if (kmem_alloc_wired(kernel_map, &addr, PAGE_SIZE) != KERN_SUCCESS)
				panic("%s: couldn't allocate space for store ring\n", device_name);
			ring = (void*) addr;
			SHARED_RING_INIT(ring);
			FRONT_RING_INIT(&bd->ring, ring, PAGE_SIZE);
			grant = hyp_grant_give(domid, atop(kvtophys(addr)), 0);

			/* and give it to backend.  */
			i = sprintf(port_name, "%u", grant);
			c = hyp_store_write(t, port_name, 5, VBD_PATH, "/", vbds[n], "/", "ring-ref");
			if (!c)
				panic("%s: couldn't store ring reference (%s)", device_name, hyp_store_error);
			kfree((vm_offset_t) c, strlen(c)+1);

			/* Allocate an event channel and give it to backend.  */
			bd->evt = evt = hyp_event_channel_alloc(domid);
			hyp_evt_handler(evt, hyp_block_intr, n, SPL7);
			i = sprintf(port_name, "%lu", evt);
			c = hyp_store_write(t, port_name, 5, VBD_PATH, "/", vbds[n], "/", "event-channel");
			if (!c)
				panic("%s: couldn't store event channel (%s)", device_name, hyp_store_error);
			kfree((vm_offset_t) c, strlen(c)+1);
			c = hyp_store_write(t, hyp_store_state_initialized, 5, VBD_PATH, "/", vbds[n], "/", "state");
			if (!c)
				panic("%s: couldn't store state (%s)", device_name, hyp_store_error);
			kfree((vm_offset_t) c, strlen(c)+1);
		} while (!hyp_store_transaction_stop(t));
	/* TODO randomly wait? */

		c = hyp_store_read(0, 5, VBD_PATH, "/", vbds[n], "/", "backend");
		if (!c)
			panic("%s: couldn't get path to backend (%s)", device_name, hyp_store_error);
		bd->backend = c;

		while(1) {
			i = hyp_store_read_int(0, 3, bd->backend, "/", "state");
			if (i == MACH_ATOI_DEFAULT)
				panic("can't read state from %s", bd->backend);
			if (i == XenbusStateConnected)
				break;
			hyp_yield();
		}

		i = hyp_store_read_int(0, 3, bd->backend, "/", "sectors");
		if (i == -1)
			panic("%s: couldn't get number of sectors (%s)", device_name, hyp_store_error);
		bd->nr_sectors = i;
		
		i = hyp_store_read_int(0, 3, bd->backend, "/", "sector-size");
		if (i == -1)
			panic("%s: couldn't get sector size (%s)", device_name, hyp_store_error);
		if (i & ~(2*(i-1)+1))
			panic("sector size %d is not a power of 2\n", i);
		if (i > PAGE_SIZE || PAGE_SIZE % i != 0)
			panic("%s: couldn't handle sector size %d with pages of size %d\n", device_name, i, PAGE_SIZE);
		bd->sector_size = i;

		i = hyp_store_read_int(0, 3, bd->backend, "/", "info");
		if (i == -1)
			panic("%s: couldn't get info (%s)", device_name, hyp_store_error);
		bd->info = i;
		
		c = hyp_store_read(0, 3, bd->backend, "/", "mode");
		if (!c)
			panic("%s: couldn't get backend's mode (%s)", device_name, hyp_store_error);
		if ((c[0] == 'w') && !(bd->info & VDISK_READONLY))
			bd->mode = D_READ|D_WRITE;
		else
			bd->mode = D_READ;

		c = hyp_store_read(0, 3, bd->backend, "/", "params");
		if (!c)
			panic("%s: couldn't get backend's real device (%s)", device_name, hyp_store_error);

		/* TODO: change suffix */
		printf("%s: dom%d's VBD %s (%s,%c%s) %ldMB\n", device_name, domid,
			vbds[n], c, bd->mode & D_WRITE ? 'w' : 'r',
			bd->info & VDISK_CDROM ? ", cdrom" : "",
			bd->nr_sectors / ((1<<20) / 512));
		kfree((vm_offset_t) c, strlen(c)+1);

		c = hyp_store_write(0, hyp_store_state_connected, 5, VBD_PATH, "/", bd->vbd, "/", "state");
		if (!c)
			panic("couldn't store state for %s (%s)", device_name, hyp_store_error);
		kfree((vm_offset_t) c, strlen(c)+1);

		bd->open_count = -1;
		bd->device.emul_ops = &hyp_block_emulation_ops;
		bd->device.emul_data = bd;
		simple_lock_init(&bd->lock);
		simple_lock_init(&bd->pushlock);
	}
}

static ipc_port_t
dev_to_port(void *d)
{
	struct block_data *b = d;
	if (!d)
		return IP_NULL;
	return ipc_port_make_send(b->port);
}

static int
device_close(void *devp)
{
	struct block_data *bd = devp;
	if (--bd->open_count < 0)
		panic("too many closes on %s", bd->name);
	printf("close, %s count %d\n", bd->name, bd->open_count);
	if (bd->open_count)
		return 0;
	ipc_kobject_set(bd->port, IKO_NULL, IKOT_NONE);
	ipc_port_dealloc_kernel(bd->port);
	return 0;
}

static io_return_t
device_open (ipc_port_t reply_port, mach_msg_type_name_t reply_port_type,
	    dev_mode_t mode, char *name, device_t *devp /* out */)
{
	int i, err = 0;
	ipc_port_t port, notify;
	struct block_data *bd;

	for (i = 0; i < n_vbds; i++)
		if (!strcmp(name, vbd_data[i].name))
			break;

	if (i == n_vbds)
		return D_NO_SUCH_DEVICE;

	bd = &vbd_data[i];
	if (bd->open_count == -2)
		/* couldn't be initialized */
		return D_NO_SUCH_DEVICE;

	if ((mode & D_WRITE) && !(bd->mode & D_WRITE))
		return D_READ_ONLY;

	if (bd->open_count >= 0) {
		*devp = &bd->device ;
		bd->open_count++ ;
		printf("re-open, %s count %d\n", bd->name, bd->open_count);
		return D_SUCCESS;
	}

	bd->open_count = 1;
	printf("%s count %d\n", bd->name, bd->open_count);

	port = ipc_port_alloc_kernel();
	if (port == IP_NULL) {
		err = KERN_RESOURCE_SHORTAGE;
		goto out;
	}
	bd->port = port;

	*devp = &bd->device;

	ipc_kobject_set (port, (ipc_kobject_t) &bd->device, IKOT_DEVICE);

	notify = ipc_port_make_sonce (bd->port);
	ip_lock (bd->port);
	ipc_port_nsrequest (bd->port, 1, notify, &notify);
	assert (notify == IP_NULL);

out:
	if (IP_VALID (reply_port))
		ds_device_open_reply (reply_port, reply_port_type, D_SUCCESS, port);
	else
		device_close(bd);
	return MIG_NO_REPLY;
}

static io_return_t
device_read (void *d, ipc_port_t reply_port,
	     mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	     recnum_t bn, int count, io_buf_ptr_t *data,
	     unsigned *bytes_read)
{
  int resid, amt;
  io_return_t err = 0;
  vm_page_t pages[BLKIF_MAX_SEGMENTS_PER_REQUEST];
  grant_ref_t gref[BLKIF_MAX_SEGMENTS_PER_REQUEST];
  int nbpages;
  vm_map_copy_t copy;
  vm_offset_t offset, alloc_offset, o;
  vm_object_t object;
  vm_page_t m;
  vm_size_t len, size;
  struct block_data *bd = d;
  struct blkif_request *req;

  *data = 0;
  *bytes_read = 0;

  if (count < 0)
    return D_INVALID_SIZE;
  if (count == 0)
    return 0;

  /* Allocate an object to hold the data.  */
  size = round_page (count);
  object = vm_object_allocate (size);
  if (! object)
    {
      err = D_NO_MEMORY;
      goto out;
    }
  alloc_offset = offset = 0;
  resid = count;

  while (resid && !err)
    {
      unsigned reqn;
      int i;
      int last_sect;

      nbpages = 0;

      /* Determine size of I/O this time around.  */
      len = round_page(offset + resid) - offset;
      if (len > PAGE_SIZE * BLKIF_MAX_SEGMENTS_PER_REQUEST)
	len = PAGE_SIZE * BLKIF_MAX_SEGMENTS_PER_REQUEST;

      /* Allocate pages.  */
      while (alloc_offset < offset + len)
	{
	  while ((m = vm_page_grab (FALSE)) == 0)
	    VM_PAGE_WAIT (0);
	  assert (! m->active && ! m->inactive);
	  m->busy = TRUE;
	  assert(nbpages < BLKIF_MAX_SEGMENTS_PER_REQUEST);
	  pages[nbpages++] = m;
	  alloc_offset += PAGE_SIZE;
	}

      /* Do the read.  */
      amt = len;
      if (amt > resid)
	amt = resid;

      /* allocate a request */
      spl_t spl = splsched();
      while(1) {
        simple_lock(&bd->lock);
        if (!RING_FULL(&bd->ring))
	  break;
	thread_sleep(bd, &bd->lock, FALSE);
      }
      mb();
      reqn = bd->ring.req_prod_pvt++;;
      simple_lock(&bd->pushlock);
      simple_unlock(&bd->lock);
      (void) splx(spl);

      req = RING_GET_REQUEST(&bd->ring, reqn);
      req->operation = BLKIF_OP_READ;
      req->nr_segments = nbpages;
      req->handle = bd->handle;
      req->id = (unsigned64_t) (unsigned long) &err; /* pointer on the stack */
      req->sector_number = bn + offset / 512;
      for (i = 0; i < nbpages; i++) {
	req->seg[i].gref = gref[i] = hyp_grant_give(bd->domid, atop(pages[i]->phys_addr), 0);
	req->seg[i].first_sect = 0;
	req->seg[i].last_sect = PAGE_SIZE/512 - 1;
      }
      last_sect = ((amt - 1) & PAGE_MASK) / 512;
      req->seg[nbpages-1].last_sect = last_sect;

      memset((void*) phystokv(pages[nbpages-1]->phys_addr
	      + (last_sect + 1) * 512),
	      0, PAGE_SIZE - (last_sect + 1) * 512);

      /* no need for a lock: as long as the request is not pushed, the event won't be triggered */
      assert_wait((event_t) &err, FALSE);

      int notify;
      wmb(); /* make sure it sees requests */
      RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&bd->ring, notify);
      if (notify)
	hyp_event_channel_send(bd->evt);
      simple_unlock(&bd->pushlock);

      thread_block(NULL);

      if (err)
	printf("error reading %d bytes at sector %d\n", amt,
	  bn + offset / 512);

      for (i = 0; i < nbpages; i++)
	hyp_grant_takeback(gref[i]);

      /* Compute number of pages to insert in object.  */
      o = offset;

      resid -= amt;
      if (resid == 0)
	offset = o + len;
      else
	offset += amt;

      /* Add pages to the object.  */
      vm_object_lock (object);
      for (i = 0; i < nbpages; i++)
        {
	  m = pages[i];
	  assert (m->busy);
	  vm_page_lock_queues ();
	  PAGE_WAKEUP_DONE (m);
	  m->dirty = TRUE;
	  vm_page_insert (m, object, o);
	  vm_page_unlock_queues ();
	  o += PAGE_SIZE;
	}
      vm_object_unlock (object);
    }

out:
  if (! err)
    err = vm_map_copyin_object (object, 0, round_page (count), &copy);
  if (! err)
    {
      *data = (io_buf_ptr_t) copy;
      *bytes_read = count - resid;
    }
  else
    vm_object_deallocate (object);
  return err;
}

static io_return_t
device_write(void *d, ipc_port_t reply_port,
	    mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	    recnum_t bn, io_buf_ptr_t data, unsigned int count,
	    int *bytes_written)
{
  io_return_t err = 0;
  vm_map_copy_t copy = (vm_map_copy_t) data;
  vm_offset_t aligned_buffer = 0;
  int copy_npages = atop(round_page(count));
  vm_offset_t phys_addrs[copy_npages];
  struct block_data *bd = d;
  blkif_request_t *req;
  grant_ref_t gref[BLKIF_MAX_SEGMENTS_PER_REQUEST];
  unsigned reqn, size;
  int i, nbpages, j;

  if (!(bd->mode & D_WRITE))
    return D_READ_ONLY;

  if (count == 0) {
    vm_map_copy_discard(copy);
    return 0;
  }

  if (count % bd->sector_size)
    return D_INVALID_SIZE;

  if (count > copy->size)
    return D_INVALID_SIZE;

  if (copy->type != VM_MAP_COPY_PAGE_LIST || copy->offset & PAGE_MASK) {
    /* Unaligned write.  Has to copy data before passing it to the backend.  */
    kern_return_t kr;
    vm_offset_t buffer;

    kr = kmem_alloc(device_io_map, &aligned_buffer, count);
    if (kr != KERN_SUCCESS)
      return kr;

    kr = vm_map_copyout(device_io_map, &buffer, vm_map_copy_copy(copy));
    if (kr != KERN_SUCCESS) {
      kmem_free(device_io_map, aligned_buffer, count);
      return kr;
    }

    memcpy((void*) aligned_buffer, (void*) buffer, count);

    vm_deallocate (device_io_map, buffer, count);

    for (i = 0; i < copy_npages; i++)
      phys_addrs[i] = kvtophys(aligned_buffer + ptoa(i));
  } else {
    for (i = 0; i < copy_npages; i++)
      phys_addrs[i] = copy->cpy_page_list[i]->phys_addr;
  }

  for (i=0; i<copy_npages; i+=nbpages) {

    nbpages = BLKIF_MAX_SEGMENTS_PER_REQUEST;
    if (nbpages > copy_npages-i)
      nbpages = copy_npages-i;

    /* allocate a request */
    spl_t spl = splsched();
    while(1) {
      simple_lock(&bd->lock);
      if (!RING_FULL(&bd->ring))
	break;
      thread_sleep(bd, &bd->lock, FALSE);
    }
    mb();
    reqn = bd->ring.req_prod_pvt++;;
    simple_lock(&bd->pushlock);
    simple_unlock(&bd->lock);
    (void) splx(spl);

    req = RING_GET_REQUEST(&bd->ring, reqn);
    req->operation = BLKIF_OP_WRITE;
    req->nr_segments = nbpages;
    req->handle = bd->handle;
    req->id = (unsigned64_t) (unsigned long) &err; /* pointer on the stack */
    req->sector_number = bn + i*PAGE_SIZE / 512;

    for (j = 0; j < nbpages; j++) {
      req->seg[j].gref = gref[j] = hyp_grant_give(bd->domid, atop(phys_addrs[i + j]), 1);
      req->seg[j].first_sect = 0;
      size = PAGE_SIZE;
      if ((i + j + 1) * PAGE_SIZE > count)
	size = count - (i + j) * PAGE_SIZE;
      req->seg[j].last_sect = size/512 - 1;
    }

    /* no need for a lock: as long as the request is not pushed, the event won't be triggered */
    assert_wait((event_t) &err, FALSE);

    int notify;
    wmb(); /* make sure it sees requests */
    RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&bd->ring, notify);
    if (notify)
	    hyp_event_channel_send(bd->evt);
    simple_unlock(&bd->pushlock);

    thread_block(NULL);

    for (j = 0; j < nbpages; j++)
      hyp_grant_takeback(gref[j]);

    if (err) {
      printf("error writing %d bytes at sector %d\n", count, bn);
      break;
    }
  }

  if (aligned_buffer)
    kmem_free(device_io_map, aligned_buffer, count);

  vm_map_copy_discard (copy);

  if (!err)
    *bytes_written = count;

  if (IP_VALID(reply_port))
    ds_device_write_reply (reply_port, reply_port_type, err, count);

  return MIG_NO_REPLY;
}

static io_return_t
device_get_status(void *d, dev_flavor_t flavor, dev_status_t status,
		  mach_msg_type_number_t *status_count)
{
	struct block_data *bd = d;

	switch (flavor)
	{
		case DEV_GET_SIZE:
			status[DEV_GET_SIZE_DEVICE_SIZE] = (unsigned long long) bd->nr_sectors * 512;
			status[DEV_GET_SIZE_RECORD_SIZE] = bd->sector_size;
			*status_count = DEV_GET_SIZE_COUNT;
			break;
		case DEV_GET_RECORDS:
			status[DEV_GET_RECORDS_DEVICE_RECORDS] = ((unsigned long long) bd->nr_sectors * 512) / bd->sector_size;
			status[DEV_GET_RECORDS_RECORD_SIZE] = bd->sector_size;
			*status_count = DEV_GET_RECORDS_COUNT;
			break;
		default:
			printf("TODO: block_%s(%d)\n", __func__, flavor);
			return D_INVALID_OPERATION;
	}
	return D_SUCCESS;
}

struct device_emulation_ops hyp_block_emulation_ops = {
	NULL,	/* dereference */
	NULL,	/* deallocate */
	dev_to_port,
	device_open,
	device_close,
	device_write,
	NULL,	/* write_inband */
	device_read,
	NULL,	/* read_inband */
	NULL,	/* set_status */
	device_get_status,
	NULL,	/* set_filter */
	NULL,	/* map */
	NULL,	/* no_senders */
	NULL,	/* write_trap */
	NULL,	/* writev_trap */
};
