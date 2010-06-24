/*
 * Linux block driver support.
 *
 * Copyright (C) 1996 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL)
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
 *
 *	Author: Shantanu Goel, University of Utah CSL
 */

/*
 *  linux/drivers/block/ll_rw_blk.c
 *
 * Copyright (C) 1991, 1992 Linus Torvalds
 * Copyright (C) 1994,      Karl Keyte: Added support for disk statistics
 */

/*
 *  linux/fs/block_dev.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 *  linux/fs/buffer.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <sys/types.h>
#include <machine/spl.h>
#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/mig_errors.h>
#include <mach/port.h>
#include <mach/vm_param.h>
#include <mach/notify.h>

#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <device/device_types.h>
#include <device/device_port.h>
#include <device/disk_status.h>
#include <device/device_reply.user.h>
#include <device/device_emul.h>

/* TODO.  This should be fixed to not be i386 specific.  */
#include <i386at/disk.h>

#define MACH_INCLUDE
#include <linux/fs.h>
#include <linux/blk.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/malloc.h>
#include <linux/hdreg.h>
#include <asm/io.h>

extern int linux_auto_config;
extern int linux_intr_pri;
extern int linux_to_mach_error (int);

/* This task queue is not used in Mach: just for fixing undefined symbols. */
DECLARE_TASK_QUEUE (tq_disk);

/* Location of VTOC in units for sectors (512 bytes).  */
#define PDLOCATION 29

/* Linux kernel variables.  */

/* Temporary data allocated on the stack.  */
struct temp_data
{
  struct inode inode;
  struct file file;
  struct request req;
  queue_head_t pages;
};

/* One of these exists for each
   driver associated with a major number.  */
struct device_struct
{
  const char *name;		/* device name */
  struct file_operations *fops;	/* operations vector */
  int busy:1;			/* driver is being opened/closed */
  int want:1;			/* someone wants to open/close driver */
  struct gendisk *gd;		/* DOS partition information */
  int default_slice;		/* what slice to use when none is given */
  struct disklabel **labels;	/* disklabels for each DOS partition */
};

/* An entry in the Mach name to Linux major number conversion table.  */
struct name_map
{
  const char *name;	/* Mach name for device */
  unsigned major;	/* Linux major number */
  unsigned unit;	/* Linux unit number */
  int read_only;	/* 1 if device is read only */
};

/* Driver operation table.  */
static struct device_struct blkdevs[MAX_BLKDEV];

/* Driver request function table.  */
struct blk_dev_struct blk_dev[MAX_BLKDEV] =
{
  { NULL, NULL },		/* 0 no_dev */
  { NULL, NULL },		/* 1 dev mem */
  { NULL, NULL },		/* 2 dev fd */
  { NULL, NULL },		/* 3 dev ide0 or hd */
  { NULL, NULL },		/* 4 dev ttyx */
  { NULL, NULL },		/* 5 dev tty */
  { NULL, NULL },		/* 6 dev lp */
  { NULL, NULL },		/* 7 dev pipes */
  { NULL, NULL },		/* 8 dev sd */
  { NULL, NULL },		/* 9 dev st */
  { NULL, NULL },		/* 10 */
  { NULL, NULL },		/* 11 */
  { NULL, NULL },		/* 12 */
  { NULL, NULL },		/* 13 */
  { NULL, NULL },		/* 14 */
  { NULL, NULL },		/* 15 */
  { NULL, NULL },		/* 16 */
  { NULL, NULL },		/* 17 */
  { NULL, NULL },		/* 18 */
  { NULL, NULL },		/* 19 */
  { NULL, NULL },		/* 20 */
  { NULL, NULL },		/* 21 */
  { NULL, NULL }		/* 22 dev ide1 */
};

/*
 * blk_size contains the size of all block-devices in units of 1024 byte
 * sectors:
 *
 * blk_size[MAJOR][MINOR]
 *
 * if (!blk_size[MAJOR]) then no minor size checking is done.
 */
int *blk_size[MAX_BLKDEV] = { NULL, NULL, };

/*
 * blksize_size contains the size of all block-devices:
 *
 * blksize_size[MAJOR][MINOR]
 *
 * if (!blksize_size[MAJOR]) then 1024 bytes is assumed.
 */
int *blksize_size[MAX_BLKDEV] = { NULL, NULL, };

/*
 * hardsect_size contains the size of the hardware sector of a device.
 *
 * hardsect_size[MAJOR][MINOR]
 *
 * if (!hardsect_size[MAJOR])
 *		then 512 bytes is assumed.
 * else
 *		sector_size is hardsect_size[MAJOR][MINOR]
 * This is currently set by some scsi device and read by the msdos fs driver
 * This might be a some uses later.
 */
int *hardsect_size[MAX_BLKDEV] = { NULL, NULL, };

/* This specifies how many sectors to read ahead on the disk.
   This is unused in Mach.  It is here to make drivers compile.  */
int read_ahead[MAX_BLKDEV] = {0, };

/* Use to wait on when there are no free requests.
   This is unused in Mach.  It is here to make drivers compile.  */
struct wait_queue *wait_for_request = NULL;

/* Map for allocating device memory.  */
extern vm_map_t device_io_map;

/* Initialize block drivers.  */
int
blk_dev_init ()
{
#ifdef CONFIG_BLK_DEV_IDE
  ide_init ();
#endif
#ifdef CONFIG_BLK_DEV_FD
  floppy_init ();
#else
  outb_p (0xc, 0x3f2);
#endif
  return 0;
}

/* Return 1 if major number MAJOR corresponds to a disk device.  */
static inline int
disk_major (int major)
{
  return (major == IDE0_MAJOR
	  || major == IDE1_MAJOR
	  || major == IDE2_MAJOR
	  || major == IDE3_MAJOR
	  || major == SCSI_DISK_MAJOR);
}

/* Linux kernel block support routines.  */

/* Register a driver for major number MAJOR,
   with name NAME, and operations vector FOPS.  */
int
register_blkdev (unsigned major, const char *name,
		 struct file_operations *fops)
{
  int err = 0;

  if (major == 0)
    {
      for (major = MAX_BLKDEV - 1; major > 0; major--)
	if (blkdevs[major].fops == NULL)
	  goto out;
      return -LINUX_EBUSY;
    }
  if (major >= MAX_BLKDEV)
    return -LINUX_EINVAL;
  if (blkdevs[major].fops && blkdevs[major].fops != fops)
    return -LINUX_EBUSY;

out:
  blkdevs[major].name = name;
  blkdevs[major].fops = fops;
  blkdevs[major].busy = 0;
  blkdevs[major].want = 0;
  blkdevs[major].gd = NULL;
  blkdevs[major].default_slice = 0;
  blkdevs[major].labels = NULL;
  return 0;
}

/* Unregister the driver associated with
   major number MAJOR and having the name NAME.  */
int
unregister_blkdev (unsigned major, const char *name)
{
  int err;

  if (major >= MAX_BLKDEV)
    return -LINUX_EINVAL;
  if (! blkdevs[major].fops || strcmp (blkdevs[major].name, name))
    return -LINUX_EINVAL;
  blkdevs[major].fops = NULL;
  if (blkdevs[major].labels)
    {
      assert (blkdevs[major].gd);
      kfree ((vm_offset_t) blkdevs[major].labels,
	     (sizeof (struct disklabel *)
	      * blkdevs[major].gd->max_p * blkdevs[major].gd->max_nr));
    }
  return 0;
}

void
set_blocksize (kdev_t dev, int size)
{
  extern int *blksize_size[];

  if (! blksize_size[MAJOR (dev)])
    return;

  switch (size)
    {
    case 512:
    case 1024:
    case 2048:
    case 4096:
      break;
    default:
      panic ("Invalid blocksize passed to set_blocksize");
      break;
    }
  blksize_size[MAJOR (dev)][MINOR (dev)] = size;
}

/* Allocate a buffer SIZE bytes long.  */
static void *
alloc_buffer (int size)
{
  vm_page_t m;
  struct temp_data *d;

  assert (size <= PAGE_SIZE);

  if (! linux_auto_config)
    {
      while ((m = vm_page_grab (FALSE)) == 0)
	VM_PAGE_WAIT (0);
      d = current_thread ()->pcb->data;
      assert (d);
      queue_enter (&d->pages, m, vm_page_t, pageq);
      return (void *) m->phys_addr;
    }
  return (void *) __get_free_pages (GFP_KERNEL, 0, ~0UL);
}

/* Free buffer P which is SIZE bytes long.  */
static void
free_buffer (void *p, int size)
{
  int i;
  struct temp_data *d;
  vm_page_t m;

  assert (size <= PAGE_SIZE);

  if (! linux_auto_config)
    {
      d = current_thread ()->pcb->data;
      assert (d);
      queue_iterate (&d->pages, m, vm_page_t, pageq)
	{
	  if (m->phys_addr == (vm_offset_t) p)
	    {
	      queue_remove (&d->pages, m, vm_page_t, pageq);
	      VM_PAGE_FREE (m);
	      return;
	    }
	}
      panic ("free_buffer");
    }
  free_pages ((unsigned long) p, 0);
}

/* Allocate a buffer of SIZE bytes and
   associate it with block number BLOCK of device DEV.  */
struct buffer_head *
getblk (kdev_t dev, int block, int size)
{
  struct buffer_head *bh;

  assert (size <= PAGE_SIZE);

  bh = (struct buffer_head *) kalloc (sizeof (struct buffer_head));
  if (bh)
    {
      memset (bh, 0, sizeof (struct buffer_head));
      bh->b_data = alloc_buffer (size);
      if (! bh->b_data)
	{
	  kfree ((vm_offset_t) bh, sizeof (struct buffer_head));
	  return NULL;
	}
      bh->b_dev = dev;
      bh->b_size = size;
      bh->b_state = 1 << BH_Lock;
      bh->b_blocknr = block;
    }
  return bh;
}

/* Release buffer BH previously allocated by getblk.  */
void
__brelse (struct buffer_head *bh)
{
  free_buffer (bh->b_data, bh->b_size);
  kfree ((vm_offset_t) bh, sizeof (*bh));
}

/* Allocate a buffer of SIZE bytes and fill it with data
   from device DEV starting at block number BLOCK.  */
struct buffer_head *
bread (kdev_t dev, int block, int size)
{
  int err;
  struct buffer_head *bh;

  bh = getblk (dev, block, size);
  if (bh)
    {
      ll_rw_block (READ, 1, &bh);
      wait_on_buffer (bh);
      if (! buffer_uptodate (bh))
	{
	  __brelse (bh);
	  return NULL;
	}
    }
  return bh;
}

/* Return the block size for device DEV in *BSIZE and
   log2(block size) in *BSHIFT.  */
static void
get_block_size (kdev_t dev, int *bsize, int *bshift)
{
  int i;

  *bsize = BLOCK_SIZE;
  if (blksize_size[MAJOR (dev)]
      && blksize_size[MAJOR (dev)][MINOR (dev)])
    *bsize = blksize_size[MAJOR (dev)][MINOR (dev)];
  for (i = *bsize, *bshift = 0; i != 1; i >>= 1, (*bshift)++)
    ;
}

/* Enqueue request REQ on a driver's queue.  */
static inline void
enqueue_request (struct request *req)
{
  struct request *tmp;
  struct blk_dev_struct *dev;

  dev = blk_dev + MAJOR (req->rq_dev);
  cli ();
  tmp = dev->current_request;
  if (! tmp)
    {
      dev->current_request = req;
      (*dev->request_fn) ();
      sti ();
      return;
    }
  while (tmp->next)
    {
      if ((IN_ORDER (tmp, req) || ! IN_ORDER (tmp, tmp->next))
	  && IN_ORDER (req, tmp->next))
	break;
      tmp = tmp->next;
    }
  req->next = tmp->next;
  tmp->next = req;
  if (scsi_blk_major (MAJOR (req->rq_dev)))
    (*dev->request_fn) ();
  sti ();
}

/* Perform the I/O operation RW on the buffer list BH
   containing NR buffers.  */
void
ll_rw_block (int rw, int nr, struct buffer_head **bh)
{
  int i, bshift, bsize;
  unsigned major;
  struct request *r;
  static struct request req;

  major = MAJOR (bh[0]->b_dev);
  assert (major < MAX_BLKDEV);

  get_block_size (bh[0]->b_dev, &bsize, &bshift);

  if (! linux_auto_config)
    {
      assert (current_thread ()->pcb->data);
      r = &((struct temp_data *) current_thread ()->pcb->data)->req;
    }
  else
    r = &req;

  for (i = 0, r->nr_sectors = 0; i < nr - 1; i++)
    {
      r->nr_sectors += bh[i]->b_size >> 9;
      bh[i]->b_reqnext = bh[i + 1];
    }
  r->nr_sectors += bh[i]->b_size >> 9;
  bh[i]->b_reqnext = NULL;

  r->rq_status = RQ_ACTIVE;
  r->rq_dev = bh[0]->b_dev;
  r->cmd = rw;
  r->errors = 0;
  r->sector = bh[0]->b_blocknr << (bshift - 9);
  r->current_nr_sectors = bh[0]->b_size >> 9;
  r->buffer = bh[0]->b_data;
  r->bh = bh[0];
  r->bhtail = bh[nr - 1];
  r->sem = NULL;
  r->next = NULL;

  enqueue_request (r);
}

#define BSIZE	(1 << bshift)
#define BMASK	(BSIZE - 1)

/* Perform read/write operation RW on device DEV
   starting at *off to/from buffer *BUF of size *RESID.
   The device block size is given by BSHIFT.  *OFF and
   *RESID may be non-multiples of the block size.
   *OFF, *BUF and *RESID are updated if the operation
   completed successfully.  */
static int
rdwr_partial (int rw, kdev_t dev, loff_t *off,
	      char **buf, int *resid, int bshift)
{
  int c, err = 0, o;
  long sect, nsect;
  struct buffer_head bhead, *bh = &bhead;
  struct gendisk *gd;

  memset (bh, 0, sizeof (struct buffer_head));
  bh->b_state = 1 << BH_Lock;
  bh->b_dev = dev;
  bh->b_blocknr = *off >> bshift;
  bh->b_size = BSIZE;

  /* Check if this device has non even number of blocks.  */
  for (gd = gendisk_head, nsect = -1; gd; gd = gd->next)
    if (gd->major == MAJOR (dev))
      {
	nsect = gd->part[MINOR (dev)].nr_sects;
	break;
      }
  if (nsect > 0)
    {
      sect = bh->b_blocknr << (bshift - 9);
      assert ((nsect - sect) > 0);
      if (nsect - sect < (BSIZE >> 9))
	bh->b_size = (nsect - sect) << 9;
    }
  bh->b_data = alloc_buffer (bh->b_size);
  if (! bh->b_data)
    return -LINUX_ENOMEM;
  ll_rw_block (READ, 1, &bh);
  wait_on_buffer (bh);
  if (buffer_uptodate (bh))
    {
      o = *off & BMASK;
      c = bh->b_size - o;
      if (c > *resid)
	c = *resid;
      if (rw == READ)
	memcpy (*buf, bh->b_data + o, c);
      else
	{
	  memcpy (bh->b_data + o, *buf, c);
	  bh->b_state = (1 << BH_Dirty) | (1 << BH_Lock);
	  ll_rw_block (WRITE, 1, &bh);
	  wait_on_buffer (bh);
	  if (! buffer_uptodate (bh))
	    {
	      err = -LINUX_EIO;
	      goto out;
	    }
	}
      *buf += c;
      *resid -= c;
      *off += c;
    }
  else
    err = -LINUX_EIO;
out:
  free_buffer (bh->b_data, bh->b_size);
  return err;
}

#define BH_Bounce	16
#define MAX_BUF		8

/* Perform read/write operation RW on device DEV
   starting at *off to/from buffer *BUF of size *RESID.
   The device block size is given by BSHIFT.  *OFF and
   *RESID must be multiples of the block size.
   *OFF, *BUF and *RESID are updated if the operation
   completed successfully.  */
static int
rdwr_full (int rw, kdev_t dev, loff_t *off, char **buf, int *resid, int bshift)
{
  int cc, err = 0, i, j, nb, nbuf;
  long blk;
  struct buffer_head bhead[MAX_BUF], *bh, *bhp[MAX_BUF];

  assert ((*off & BMASK) == 0);

  nbuf = *resid >> bshift;
  blk = *off >> bshift;
  for (i = nb = 0, bh = bhead; nb < nbuf; bh++)
    {
      memset (bh, 0, sizeof (*bh));
      bh->b_dev = dev;
      bh->b_blocknr = blk;
      set_bit (BH_Lock, &bh->b_state);
      if (rw == WRITE)
	set_bit (BH_Dirty, &bh->b_state);
      cc = PAGE_SIZE - (((int) *buf) & PAGE_MASK);
      if (cc >= BSIZE && ((int) *buf & 511) == 0)
	cc &= ~BMASK;
      else
	{
	  cc = PAGE_SIZE;
	  set_bit (BH_Bounce, &bh->b_state);
	}
      if (cc > ((nbuf - nb) << bshift))
	cc = (nbuf - nb) << bshift;
      if (! test_bit (BH_Bounce, &bh->b_state))
	bh->b_data = (char *) pmap_extract (vm_map_pmap (device_io_map),
					    (((vm_offset_t) *buf)
					     + (nb << bshift)));
      else
	{
	  bh->b_data = alloc_buffer (cc);
	  if (! bh->b_data)
	    {
	      err = -LINUX_ENOMEM;
	      break;
	    }
	  if (rw == WRITE)
	    memcpy (bh->b_data, *buf + (nb << bshift), cc);
	}
      bh->b_size = cc;
      bhp[i] = bh;
      nb += cc >> bshift;
      blk += cc >> bshift;
      if (++i == MAX_BUF)
	break;
    }
  if (! err)
    {
      ll_rw_block (rw, i, bhp);
      wait_on_buffer (bhp[i - 1]);
    }
  for (bh = bhead, cc = 0, j = 0; j < i; cc += bh->b_size, bh++, j++)
    {
      if (! err && buffer_uptodate (bh)
	  && rw == READ && test_bit (BH_Bounce, &bh->b_state))
	memcpy (*buf + cc, bh->b_data, bh->b_size);
      else if (! err && ! buffer_uptodate (bh))
	  err = -LINUX_EIO;
      if (test_bit (BH_Bounce, &bh->b_state))
	free_buffer (bh->b_data, bh->b_size);
    }
  if (! err)
    {
      *buf += cc;
      *resid -= cc;
      *off += cc;
    }
  return err;
}

/* Perform read/write operation RW on device DEV
   starting at *off to/from buffer BUF of size COUNT.
   *OFF is updated if the operation completed successfully.  */
static int
do_rdwr (int rw, kdev_t dev, loff_t *off, char *buf, int count)
{
  int bsize, bshift, err = 0, resid = count;

  get_block_size (dev, &bsize, &bshift);
  if (*off & BMASK)
    err = rdwr_partial (rw, dev, off, &buf, &resid, bshift);
  while (resid >= bsize && ! err)
    err = rdwr_full (rw, dev, off, &buf, &resid, bshift);
  if (! err && resid)
    err = rdwr_partial (rw, dev, off, &buf, &resid, bshift);
  return err ? err : count - resid;
}

int
block_write (struct inode *inode, struct file *filp,
	     const char *buf, int count)
{
  return do_rdwr (WRITE, inode->i_rdev, &filp->f_pos, (char *) buf, count);
}

int
block_read (struct inode *inode, struct file *filp, char *buf, int count)
{
  return do_rdwr (READ, inode->i_rdev, &filp->f_pos, buf, count);
}

/*
 * This routine checks whether a removable media has been changed,
 * and invalidates all buffer-cache-entries in that case. This
 * is a relatively slow routine, so we have to try to minimize using
 * it. Thus it is called only upon a 'mount' or 'open'. This
 * is the best way of combining speed and utility, I think.
 * People changing diskettes in the middle of an operation deserve
 * to loose :-)
 */
int
check_disk_change (kdev_t dev)
{
  unsigned i;
  struct file_operations * fops;

  i = MAJOR(dev);
  if (i >= MAX_BLKDEV || (fops = blkdevs[i].fops) == NULL)
    return 0;
  if (fops->check_media_change == NULL)
    return 0;
  if (! (*fops->check_media_change) (dev))
    return 0;

  /*  printf ("Disk change detected on device %s\n", kdevname(dev));*/

  if (fops->revalidate)
    (*fops->revalidate) (dev);

  return 1;
}

/* Mach device interface routines.  */

/* Mach name to Linux major/minor number mapping table.  */
static struct name_map name_to_major[] =
{
  /* IDE disks */
  { "hd0", IDE0_MAJOR, 0, 0 },
  { "hd1", IDE0_MAJOR, 1, 0 },
  { "hd2", IDE1_MAJOR, 0, 0 },
  { "hd3", IDE1_MAJOR, 1, 0 },
  { "hd4", IDE2_MAJOR, 0, 0 },
  { "hd5", IDE2_MAJOR, 1, 0 },
  { "hd6", IDE3_MAJOR, 0, 0 },
  { "hd7", IDE3_MAJOR, 1, 0 },

  /* IDE CDROMs */
  { "wcd0", IDE0_MAJOR, 0, 1 },
  { "wcd1", IDE0_MAJOR, 1, 1 },
  { "wcd2", IDE1_MAJOR, 0, 1 },
  { "wcd3", IDE1_MAJOR, 1, 1 },
  { "wcd4", IDE2_MAJOR, 0, 1 },
  { "wcd5", IDE2_MAJOR, 1, 1 },
  { "wcd6", IDE3_MAJOR, 0, 1 },
  { "wcd7", IDE3_MAJOR, 1, 1 },

  /* SCSI disks */
  { "sd0", SCSI_DISK_MAJOR, 0, 0 },
  { "sd1", SCSI_DISK_MAJOR, 1, 0 },
  { "sd2", SCSI_DISK_MAJOR, 2, 0 },
  { "sd3", SCSI_DISK_MAJOR, 3, 0 },
  { "sd4", SCSI_DISK_MAJOR, 4, 0 },
  { "sd5", SCSI_DISK_MAJOR, 5, 0 },
  { "sd6", SCSI_DISK_MAJOR, 6, 0 },
  { "sd7", SCSI_DISK_MAJOR, 7, 0 },

  /* SCSI CDROMs */
  { "cd0", SCSI_CDROM_MAJOR, 0, 1 },
  { "cd1", SCSI_CDROM_MAJOR, 1, 1 },

  /* Floppy disks */
  { "fd0", FLOPPY_MAJOR, 0, 0 },
  { "fd1", FLOPPY_MAJOR, 1, 0 },
};

#define NUM_NAMES (sizeof (name_to_major) / sizeof (name_to_major[0]))

/* One of these is associated with each open instance of a device.  */
struct block_data
{
  const char *name;		/* Mach name for device */
  int want:1;			/* someone is waiting for I/O to complete */
  int open_count;		/* number of opens */
  int iocount;			/* number of pending I/O operations */
  int part;			/* BSD partition number (-1 if none) */
  int flags;			/* Linux file flags */
  int mode;			/* Linux file mode */
  kdev_t dev;			/* Linux device number */
  ipc_port_t port;		/* port representing device */
  struct device_struct *ds;	/* driver operation table entry */
  struct device device;		/* generic device header */
  struct name_map *np;		/* name to inode map */
  struct block_data *next;	/* forward link */
};

/* List of open devices.  */
static struct block_data *open_list;

/* Forward declarations.  */

extern struct device_emulation_ops linux_block_emulation_ops;

static io_return_t device_close (void *);

/* Return a send right for block device BD.  */
static ipc_port_t
dev_to_port (void *bd)
{
  return (bd
	  ? ipc_port_make_send (((struct block_data *) bd)->port)
	  : IP_NULL);
}

/* Return 1 if C is a letter of the alphabet.  */
static inline int
isalpha (int c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Return 1 if C is a digit.  */
static inline int
isdigit (int c)
{
  return c >= '0' && c <= '9';
}

/* Find the name map entry for device NAME.
   Set *SLICE to be the DOS partition and
   *PART the BSD/Mach partition, if any.  */
static struct name_map *
find_name (char *name, int *slice, int *part)
{
  char *p, *q;
  int i, len;
  struct name_map *np;

  /* Parse name into name, unit, DOS partition (slice) and partition.  */
  for (*slice = 0, *part = -1, p = name; isalpha (*p); p++)
    ;
  if (p == name || ! isdigit (*p))
    return NULL;
  do
    p++;
  while (isdigit (*p));
  if (*p)
    {
      q = p;
      if (*q == 's' && isdigit (*(q + 1)))
	{
	  q++;
	  do
	    *slice = *slice * 10 + *q++ - '0';
	  while (isdigit (*q));
	  if (! *q)
	    goto find_major;
	}
      if (! isalpha (*q) || *(q + 1))
	return NULL;
      *part = *q - 'a';
    }

find_major:
  /* Convert name to major number.  */
  for (i = 0, np = name_to_major; i < NUM_NAMES; i++, np++)
    {
      len = strlen (np->name);
      if (len == (p - name) && ! strncmp (np->name, name, len))
	return np;
    }
  return NULL;
}

/* Attempt to read a BSD disklabel from device DEV.  */
static struct disklabel *
read_bsd_label (kdev_t dev)
{
  int bsize, bshift;
  struct buffer_head *bh;
  struct disklabel *dlp, *lp = NULL;

  get_block_size (dev, &bsize, &bshift);
  bh = bread (dev, LBLLOC >> (bshift - 9), bsize);
  if (bh)
    {
      dlp = (struct disklabel *) (bh->b_data + ((LBLLOC << 9) & (bsize - 1)));
      if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC)
	{
	  lp = (struct disklabel *) kalloc (sizeof (*lp));
	  assert (lp);
	  memcpy (lp, dlp, sizeof (*lp));
	}
      __brelse (bh);
    }
  return lp;
}

/* Attempt to read a VTOC from device DEV.  */
static struct disklabel *
read_vtoc (kdev_t dev)
{
  int bshift, bsize, i;
  struct buffer_head *bh;
  struct evtoc *evp;
  struct disklabel *lp = NULL;

  get_block_size (dev, &bsize, &bshift);
  bh = bread (dev, PDLOCATION >> (bshift - 9), bsize);
  if (bh)
    {
      evp = (struct evtoc *) (bh->b_data + ((PDLOCATION << 9) & (bsize - 1)));
      if (evp->sanity == VTOC_SANE)
	{
	  lp = (struct disklabel *) kalloc (sizeof (*lp));
	  assert (lp);
	  lp->d_npartitions = evp->nparts;
	  if (lp->d_npartitions > MAXPARTITIONS)
	    lp->d_npartitions = MAXPARTITIONS;
	  for (i = 0; i < lp->d_npartitions; i++)
	    {
	      lp->d_partitions[i].p_size = evp->part[i].p_size;
	      lp->d_partitions[i].p_offset = evp->part[i].p_start;
	      lp->d_partitions[i].p_fstype = FS_BSDFFS;
	    }
	}
      __brelse (bh);
    }
  return lp;
}

/* Initialize BSD/Mach partition table for device
   specified by NP, DS and *DEV.  Check SLICE and *PART for validity.  */
static kern_return_t
init_partition (struct name_map *np, kdev_t *dev,
		struct device_struct *ds, int slice, int *part)
{
  int err, i, j;
  struct disklabel *lp;
  struct gendisk *gd = ds->gd;
  struct partition *p;
  struct temp_data *d = current_thread ()->pcb->data;

  if (! gd)
    {
      *part = -1;
      return 0;
    }
  if (ds->labels)
    goto check;
  ds->labels = (struct disklabel **) kalloc (sizeof (struct disklabel *)
					     * gd->max_nr * gd->max_p);
  if (! ds->labels)
    return D_NO_MEMORY;
  memset ((void *) ds->labels, 0,
	  sizeof (struct disklabel *) * gd->max_nr * gd->max_p);
  for (i = 1; i < gd->max_p; i++)
    {
      d->inode.i_rdev = *dev | i;
      if (gd->part[MINOR (d->inode.i_rdev)].nr_sects <= 0
	  || gd->part[MINOR (d->inode.i_rdev)].start_sect < 0)
	continue;
      linux_intr_pri = SPL5;
      d->file.f_flags = 0;
      d->file.f_mode = O_RDONLY;
      if (ds->fops->open && (*ds->fops->open) (&d->inode, &d->file))
	continue;
      lp = read_bsd_label (d->inode.i_rdev);
      if (! lp && gd->part[MINOR (d->inode.i_rdev)].nr_sects > PDLOCATION)
	lp = read_vtoc (d->inode.i_rdev);
      if (ds->fops->release)
	(*ds->fops->release) (&d->inode, &d->file);
      if (lp)
	{
	  if (ds->default_slice == 0)
	    ds->default_slice = i;
	  for (j = 0, p = lp->d_partitions; j < lp->d_npartitions; j++, p++)
	    {
	      if (p->p_offset < 0 || p->p_size <= 0)
		continue;

	      /* Sanity check.  */
	      if (p->p_size > gd->part[MINOR (d->inode.i_rdev)].nr_sects)
		p->p_size = gd->part[MINOR (d->inode.i_rdev)].nr_sects;
	    }
	}
      ds->labels[MINOR (d->inode.i_rdev)] = lp;
    }

check:
  if (*part >= 0 && slice == 0)
    slice = ds->default_slice;
  if (*part >= 0 && slice == 0)
    return D_NO_SUCH_DEVICE;
  *dev = MKDEV (MAJOR (*dev), MINOR (*dev) | slice);
  if (slice >= gd->max_p
      || gd->part[MINOR (*dev)].start_sect < 0
      || gd->part[MINOR (*dev)].nr_sects <= 0)
    return D_NO_SUCH_DEVICE;
  if (*part >= 0)
    {
      lp = ds->labels[MINOR (*dev)];
      if (! lp
	  || *part >= lp->d_npartitions
	  || lp->d_partitions[*part].p_offset < 0
	  || lp->d_partitions[*part].p_size <= 0)
	return D_NO_SUCH_DEVICE;
    }
  return 0;
}

#define DECL_DATA	struct temp_data td
#define INIT_DATA()			\
{					\
  queue_init (&td.pages);		\
  td.inode.i_rdev = bd->dev;		\
  td.file.f_mode = bd->mode;		\
  td.file.f_flags = bd->flags;		\
  current_thread ()->pcb->data = &td;	\
}

static io_return_t
device_open (ipc_port_t reply_port, mach_msg_type_name_t reply_port_type,
	     dev_mode_t mode, char *name, device_t *devp)
{
  int part, slice, err;
  unsigned major, minor;
  kdev_t dev;
  ipc_port_t notify;
  struct block_data *bd = NULL, *bdp;
  struct device_struct *ds;
  struct gendisk *gd;
  struct name_map *np;
  DECL_DATA;

  np = find_name (name, &slice, &part);
  if (! np)
    return D_NO_SUCH_DEVICE;
  major = np->major;
  ds = &blkdevs[major];

  /* Check that driver exists.  */
  if (! ds->fops)
    return D_NO_SUCH_DEVICE;

  /* Wait for any other open/close calls to finish.  */
  ds = &blkdevs[major];
  while (ds->busy)
    {
      ds->want = 1;
      assert_wait ((event_t) ds, FALSE);
      schedule ();
    }
  ds->busy = 1;

  /* Compute minor number.  */
  if (! ds->gd)
    {
      for (gd = gendisk_head; gd && gd->major != major; gd = gd->next)
	;
      ds->gd = gd;
    }
  minor = np->unit;
  gd = ds->gd;
  if (gd)
    minor <<= gd->minor_shift;
  dev = MKDEV (major, minor);

  queue_init (&td.pages);
  current_thread ()->pcb->data = &td;

  /* Check partition.  */
  err = init_partition (np, &dev, ds, slice, &part);
  if (err)
    goto out;

  /* Initialize file structure.  */
  switch (mode & (D_READ|D_WRITE))
    {
    case D_WRITE:
      td.file.f_mode = O_WRONLY;
      break;

    case D_READ|D_WRITE:
      td.file.f_mode = O_RDWR;
      break;

    default:
      td.file.f_mode = O_RDONLY;
      break;
    }
  td.file.f_flags = (mode & D_NODELAY) ? O_NDELAY : 0;

  /* Check if the device is currently open.  */
  for (bdp = open_list; bdp; bdp = bdp->next)
    if (bdp->dev == dev
	&& bdp->part == part
	&& bdp->mode == td.file.f_mode
	&& bdp->flags == td.file.f_flags)
      {
	bd = bdp;
	goto out;
      }

  /* Open the device.  */
  if (ds->fops->open)
    {
      td.inode.i_rdev = dev;
      linux_intr_pri = SPL5;
      err = (*ds->fops->open) (&td.inode, &td.file);
      if (err)
	{
	  err = linux_to_mach_error (err);
	  goto out;
	}
    }

  /* Allocate and initialize device data.  */
  bd = (struct block_data *) kalloc (sizeof (struct block_data));
  if (! bd)
    {
      err = D_NO_MEMORY;
      goto bad;
    }
  bd->want = 0;
  bd->open_count = 0;
  bd->iocount = 0;
  bd->part = part;
  bd->ds = ds;
  bd->device.emul_data = bd;
  bd->device.emul_ops = &linux_block_emulation_ops;
  bd->dev = dev;
  bd->mode = td.file.f_mode;
  bd->flags = td.file.f_flags;
  bd->port = ipc_port_alloc_kernel ();
  if (bd->port == IP_NULL)
    {
      err = KERN_RESOURCE_SHORTAGE;
      goto bad;
    }
  ipc_kobject_set (bd->port, (ipc_kobject_t) &bd->device, IKOT_DEVICE);
  notify = ipc_port_make_sonce (bd->port);
  ip_lock (bd->port);
  ipc_port_nsrequest (bd->port, 1, notify, &notify);
  assert (notify == IP_NULL);
  goto out;

bad:
  if (ds->fops->release)
    (*ds->fops->release) (&td.inode, &td.file);

out:
  ds->busy = 0;
  if (ds->want)
    {
      ds->want = 0;
      thread_wakeup ((event_t) ds);
    }

  if (bd && bd->open_count > 0)
    {
      if (err)
	*devp = NULL;
      else
	{
	  *devp = &bd->device;
	  bd->open_count++;
	}
      return err;
    }

  if (err)
    {
      if (bd)
	{
	  if (bd->port != IP_NULL)
	    {
	      ipc_kobject_set (bd->port, IKO_NULL, IKOT_NONE);
	      ipc_port_dealloc_kernel (bd->port);
	    }
	  kfree ((vm_offset_t) bd, sizeof (struct block_data));
	  bd = NULL;
	}
    }
  else
    {
      bd->open_count = 1;
      bd->next = open_list;
      open_list = bd;
    }

  if (IP_VALID (reply_port))
    ds_device_open_reply (reply_port, reply_port_type, err, dev_to_port (bd));
  else if (! err)
    device_close (bd);

  return MIG_NO_REPLY;
}

static io_return_t
device_close (void *d)
{
  struct block_data *bd = d, *bdp, **prev;
  struct device_struct *ds = bd->ds;
  DECL_DATA;

  INIT_DATA ();

  /* Wait for any other open/close to complete.  */
  while (ds->busy)
    {
      ds->want = 1;
      assert_wait ((event_t) ds, FALSE);
      schedule ();
    }
  ds->busy = 1;

  if (--bd->open_count == 0)
    {
      /* Wait for pending I/O to complete.  */
      while (bd->iocount > 0)
	{
	  bd->want = 1;
	  assert_wait ((event_t) bd, FALSE);
	  schedule ();
	}

      /* Remove device from open list.  */
      prev = &open_list;
      bdp = open_list;
      while (bdp)
	{
	  if (bdp == bd)
	    {
	      *prev = bdp->next;
	      break;
	    }
	  prev = &bdp->next;
	  bdp = bdp->next;
	}

      assert (bdp == bd);

      if (ds->fops->release)
	(*ds->fops->release) (&td.inode, &td.file);

      ipc_kobject_set (bd->port, IKO_NULL, IKOT_NONE);
      ipc_port_dealloc_kernel (bd->port);
      kfree ((vm_offset_t) bd, sizeof (struct block_data));
    }

  ds->busy = 0;
  if (ds->want)
    {
      ds->want = 0;
      thread_wakeup ((event_t) ds);
    }
  return D_SUCCESS;
}

#define MAX_COPY	(VM_MAP_COPY_PAGE_LIST_MAX << PAGE_SHIFT)

/* Check block BN and size COUNT for I/O validity
   to from device BD.  Set *OFF to the byte offset
   where I/O is to begin and return the size of transfer.  */
static int
check_limit (struct block_data *bd, loff_t *off, long bn, int count)
{
  int major, minor;
  long maxsz, sz;
  struct disklabel *lp = NULL;

  if (count <= 0)
    return count;

  major = MAJOR (bd->dev);
  minor = MINOR (bd->dev);

  if (bd->ds->gd)
    {
      if (bd->part >= 0)
	{
	  assert (bd->ds->labels);
	  assert (bd->ds->labels[minor]);
	  lp = bd->ds->labels[minor];
	  maxsz = lp->d_partitions[bd->part].p_size;
	}
      else
	maxsz = bd->ds->gd->part[minor].nr_sects;
    }
  else
    {
      assert (blk_size[major]);
      maxsz = blk_size[major][minor] << (BLOCK_SIZE_BITS - 9);
    }
  assert (maxsz > 0);
  sz = maxsz - bn;
  if (sz <= 0)
    return sz;
  if (sz < ((count + 511) >> 9))
    count = sz << 9;
  if (lp)
    bn += (lp->d_partitions[bd->part].p_offset
	   - bd->ds->gd->part[minor].start_sect);
  *off = (loff_t) bn << 9;
  bd->iocount++;
  return count;
}

static io_return_t
device_write (void *d, ipc_port_t reply_port,
	      mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	      recnum_t bn, io_buf_ptr_t data, unsigned int orig_count,
	      int *bytes_written)
{
  int resid, amt, i;
  int count = (int) orig_count;
  io_return_t err = 0;
  vm_map_copy_t copy;
  vm_offset_t addr, uaddr;
  vm_size_t len, size;
  struct block_data *bd = d;
  DECL_DATA;

  INIT_DATA ();

  *bytes_written = 0;

  if (bd->mode == O_RDONLY)
    return D_INVALID_OPERATION;
  if (! bd->ds->fops->write)
    return D_READ_ONLY;
  count = check_limit (bd, &td.file.f_pos, bn, count);
  if (count < 0)
    return D_INVALID_SIZE;
  if (count == 0)
    {
      vm_map_copy_discard (copy);
      return 0;
    }

  resid = count;
  copy = (vm_map_copy_t) data;
  uaddr = copy->offset;

  /* Allocate a kernel buffer.  */
  size = round_page (uaddr + count) - trunc_page (uaddr);
  if (size > MAX_COPY)
    size = MAX_COPY;
  addr = vm_map_min (device_io_map);
  err = vm_map_enter (device_io_map, &addr, size, 0, TRUE,
		      NULL, 0, FALSE, VM_PROT_READ|VM_PROT_WRITE,
		      VM_PROT_READ|VM_PROT_WRITE, VM_INHERIT_NONE);
  if (err)
    {
      vm_map_copy_discard (copy);
      goto out;
    }

  /* Determine size of I/O this time around.  */
  len = size - (uaddr & PAGE_MASK);
  if (len > resid)
    len = resid;

  while (1)
    {
      /* Map user pages.  */
      for (i = 0; i < copy->cpy_npages; i++)
	pmap_enter (vm_map_pmap (device_io_map),
		    addr + (i << PAGE_SHIFT),
		    copy->cpy_page_list[i]->phys_addr,
		    VM_PROT_READ|VM_PROT_WRITE, TRUE);

      /* Do the write.  */
      amt = (*bd->ds->fops->write) (&td.inode, &td.file,
				    (char *) addr + (uaddr & PAGE_MASK), len);

      /* Unmap pages and deallocate copy.  */
      pmap_remove (vm_map_pmap (device_io_map),
		   addr, addr + (copy->cpy_npages << PAGE_SHIFT));
      vm_map_copy_discard (copy);

      /* Check result of write.  */
      if (amt > 0)
	{
	  resid -= amt;
	  if (resid == 0)
	    break;
	  uaddr += amt;
	}
      else
	{
	  if (amt < 0)
	    err = linux_to_mach_error (amt);
	  break;
	}

      /* Determine size of I/O this time around and copy in pages.  */
      len = round_page (uaddr + resid) - trunc_page (uaddr);
      if (len > MAX_COPY)
	len = MAX_COPY;
      len -= uaddr & PAGE_MASK;
      if (len > resid)
	len = resid;
      err = vm_map_copyin_page_list (current_map (), uaddr, len,
				     FALSE, FALSE, &copy, FALSE);
      if (err)
	break;
    }

  /* Delete kernel buffer.  */
  vm_map_remove (device_io_map, addr, addr + size);

out:
  if (--bd->iocount == 0 && bd->want)
    {
      bd->want = 0;
      thread_wakeup ((event_t) bd);
    }
  if (IP_VALID (reply_port))
    ds_device_write_reply (reply_port, reply_port_type, err, count - resid);
  return MIG_NO_REPLY;
}

static io_return_t
device_read (void *d, ipc_port_t reply_port,
	     mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	     recnum_t bn, int count, io_buf_ptr_t *data,
	     unsigned *bytes_read)
{
  boolean_t dirty;
  int resid, amt;
  io_return_t err = 0;
  queue_head_t pages;
  vm_map_copy_t copy;
  vm_offset_t addr, offset, alloc_offset, o;
  vm_object_t object;
  vm_page_t m;
  vm_size_t len, size;
  struct block_data *bd = d;
  DECL_DATA;

  INIT_DATA ();

  *data = 0;
  *bytes_read = 0;

  if (! bd->ds->fops->read)
    return D_INVALID_OPERATION;
  count = check_limit (bd, &td.file.f_pos, bn, count);
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

  /* Allocate a kernel buffer.  */
  addr = vm_map_min (device_io_map);
  if (size > MAX_COPY)
    size = MAX_COPY;
  err = vm_map_enter (device_io_map, &addr, size, 0, TRUE, NULL,
		      0, FALSE, VM_PROT_READ|VM_PROT_WRITE,
		      VM_PROT_READ|VM_PROT_WRITE, VM_INHERIT_NONE);
  if (err)
    goto out;

  queue_init (&pages);

  while (resid)
    {
      /* Determine size of I/O this time around.  */
      len = round_page (offset + resid) - trunc_page (offset);
      if (len > MAX_COPY)
	len = MAX_COPY;

      /* Map any pages left from previous operation.  */
      o = trunc_page (offset);
      queue_iterate (&pages, m, vm_page_t, pageq)
	{
	  pmap_enter (vm_map_pmap (device_io_map),
		      addr + o - trunc_page (offset),
		      m->phys_addr, VM_PROT_READ|VM_PROT_WRITE, TRUE);
	  o += PAGE_SIZE;
	}
      assert (o == alloc_offset);

      /* Allocate and map pages.  */
      while (alloc_offset < trunc_page (offset) + len)
	{
	  while ((m = vm_page_grab (FALSE)) == 0)
	    VM_PAGE_WAIT (0);
	  assert (! m->active && ! m->inactive);
	  m->busy = TRUE;
	  queue_enter (&pages, m, vm_page_t, pageq);
	  pmap_enter (vm_map_pmap (device_io_map),
		      addr + alloc_offset - trunc_page (offset),
		      m->phys_addr, VM_PROT_READ|VM_PROT_WRITE, TRUE);
	  alloc_offset += PAGE_SIZE;
	}

      /* Do the read.  */
      amt = len - (offset & PAGE_MASK);
      if (amt > resid)
	amt = resid;
      amt = (*bd->ds->fops->read) (&td.inode, &td.file,
				   (char *) addr + (offset & PAGE_MASK), amt);

      /* Compute number of pages to insert in object.  */
      o = trunc_page (offset);
      if (amt > 0)
	{
	  dirty = TRUE;
	  resid -= amt;
	  if (resid == 0)
	    {
	      /* Zero any unused space.  */
	      if (offset + amt < o + len)
		memset ((void *) (addr + offset - o + amt),
			0, o + len - offset - amt);
	      offset = o + len;
	    }
	  else
	    offset += amt;
	}
      else
	{
	  dirty = FALSE;
	  offset = o + len;
	}

      /* Unmap pages and add them to the object.  */
      pmap_remove (vm_map_pmap (device_io_map), addr, addr + len);
      vm_object_lock (object);
      while (o < trunc_page (offset))
	{
	  m = (vm_page_t) queue_first (&pages);
	  assert (! queue_end (&pages, (queue_entry_t) m));
	  queue_remove (&pages, m, vm_page_t, pageq);
	  assert (m->busy);
	  vm_page_lock_queues ();
	  if (dirty)
	    {
	      PAGE_WAKEUP_DONE (m);
	      m->dirty = TRUE;
	      vm_page_insert (m, object, o);
	    }
	  else
	    vm_page_free (m);
	  vm_page_unlock_queues ();
	  o += PAGE_SIZE;
	}
      vm_object_unlock (object);
      if (amt <= 0)
	{
	  if (amt < 0)
	    err = linux_to_mach_error (amt);
	  break;
	}
    }

  /* Delete kernel buffer.  */
  vm_map_remove (device_io_map, addr, addr + size);

  assert (queue_empty (&pages));

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
  if (--bd->iocount == 0 && bd->want)
    {
      bd->want = 0;
      thread_wakeup ((event_t) bd);
    }
  return err;
}

static io_return_t
device_get_status (void *d, dev_flavor_t flavor, dev_status_t status,
		   mach_msg_type_number_t *status_count)
{
  struct block_data *bd = d;

  switch (flavor)
    {
    case DEV_GET_SIZE:
      if (disk_major (MAJOR (bd->dev)))
	{
	  assert (bd->ds->gd);

	  if (bd->part >= 0)
	    {
	      struct disklabel *lp;

	      assert (bd->ds->labels);
	      lp = bd->ds->labels[MINOR (bd->dev)];
	      assert (lp);
	      (status[DEV_GET_SIZE_DEVICE_SIZE]
	       = lp->d_partitions[bd->part].p_size << 9);
	    }
	  else
	    (status[DEV_GET_SIZE_DEVICE_SIZE]
	     = bd->ds->gd->part[MINOR (bd->dev)].nr_sects << 9);
	}
      else
	{
	  assert (blk_size[MAJOR (bd->dev)]);
	  (status[DEV_GET_SIZE_DEVICE_SIZE]
	   = (blk_size[MAJOR (bd->dev)][MINOR (bd->dev)]
	      << BLOCK_SIZE_BITS));
	}
      /* It would be nice to return the block size as reported by
	 the driver, but a lot of user level code assumes the sector
	 size to be 512.  */
      status[DEV_GET_SIZE_RECORD_SIZE] = 512;
      /* Always return DEV_GET_SIZE_COUNT.  This is what all native
         Mach drivers do, and makes it possible to detect the absence
         of the call by setting it to a different value on input.  MiG
         makes sure that we will never return more integers than the
         user asked for.  */
      *status_count = DEV_GET_SIZE_COUNT;
      break;

    case DEV_GET_RECORDS:
      if (disk_major (MAJOR (bd->dev)))
	{
	  assert (bd->ds->gd);

	  if (bd->part >= 0)
	    {
	      struct disklabel *lp;

	      assert (bd->ds->labels);
	      lp = bd->ds->labels[MINOR (bd->dev)];
	      assert (lp);
	      (status[DEV_GET_RECORDS_DEVICE_RECORDS]
	       = lp->d_partitions[bd->part].p_size);
	    }
	  else
	    (status[DEV_GET_RECORDS_DEVICE_RECORDS]
	     = bd->ds->gd->part[MINOR (bd->dev)].nr_sects);
	}
      else
	{
	  assert (blk_size[MAJOR (bd->dev)]);
	  status[DEV_GET_RECORDS_DEVICE_RECORDS]
	    = (blk_size[MAJOR (bd->dev)][MINOR (bd->dev)]
	       << (BLOCK_SIZE_BITS - 9));
	}
      /* It would be nice to return the block size as reported by
	 the driver, but a lot of user level code assumes the sector
	 size to be 512.  */
      status[DEV_GET_SIZE_RECORD_SIZE] = 512;
      /* Always return DEV_GET_RECORDS_COUNT.  This is what all native
         Mach drivers do, and makes it possible to detect the absence
         of the call by setting it to a different value on input.  MiG
         makes sure that we will never return more integers than the
         user asked for.  */
      *status_count = DEV_GET_RECORDS_COUNT;
      break;

    case V_GETPARMS:
      if (*status_count < (sizeof (struct disk_parms) / sizeof (int)))
	return D_INVALID_OPERATION;
      else
	{
	  struct disk_parms *dp = status;
	  struct hd_geometry hg;
	  DECL_DATA;

	  INIT_DATA();

	  if ((*bd->ds->fops->ioctl) (&td.inode, &td.file,
				      HDIO_GETGEO, &hg))
	    return D_INVALID_OPERATION;

	  dp->dp_type = DPT_WINI;  /* XXX: It may be a floppy...  */
	  dp->dp_heads = hg.heads;
	  dp->dp_cyls = hg.cylinders;
	  dp->dp_sectors = hg.sectors;
	  dp->dp_dosheads = hg.heads;
	  dp->dp_doscyls = hg.cylinders;
	  dp->dp_dossectors = hg.sectors;
	  dp->dp_secsiz = 512;  /* XXX */
	  dp->dp_ptag = 0;
	  dp->dp_pflag = 0;

	  /* XXX */
	  dp->dp_pstartsec = -1;
	  dp->dp_pnumsec = -1;

	  *status_count = sizeof (struct disk_parms) / sizeof (int);
	}

      break;

    default:
      return D_INVALID_OPERATION;
    }

  return D_SUCCESS;
}

static io_return_t
device_set_status (void *d, dev_flavor_t flavor, dev_status_t status,
		   mach_msg_type_number_t *status_count)
{
  struct block_data *bd = d;

  switch (flavor)
    {
      case BLKRRPART:
	{
	  DECL_DATA;
	  INIT_DATA();
	  return (*bd->ds->fops->ioctl) (&td.inode, &td.file, flavor, 0);
	}
    }

  return D_INVALID_OPERATION;
}

struct device_emulation_ops linux_block_emulation_ops =
{
  NULL,
  NULL,
  dev_to_port,
  device_open,
  device_close,
  device_write,
  NULL,
  device_read,
  NULL,
  device_set_status,
  device_get_status,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};
