/*
 * Miscellaneous routines and data for Linux emulation.
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
 *      Author: Shantanu Goel, University of Utah CSL
 */

/*
 *  linux/fs/proc/scsi.c  
 *  (c) 1995 Michael Neuffer neuffer@goofy.zdv.uni-mainz.de
 *
 *  The original version was derived from linux/fs/proc/net.c,
 *  which is Copyright (C) 1991, 1992 Linus Torvalds. 
 *  Much has been rewritten, but some of the code still remains.
 *
 *  /proc/scsi directory handling functions
 *
 *  last change: 95/07/04    
 *
 *  Initial version: March '95
 *  95/05/15 Added subdirectories for each driver and show every
 *           registered HBA as a single file. 
 *  95/05/30 Added rudimentary write support for parameter passing
 *  95/07/04 Fixed bugs in directory handling
 *  95/09/13 Update to support the new proc-dir tree
 *
 *  TODO: Improve support to write to the driver files
 *        Add some more comments
 */

/*
 *  linux/fs/buffer.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <sys/types.h>
#include <mach/vm_param.h>
#include <kern/thread.h>
#include <kern/printf.h>
#include <kern/mach_host.server.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <device/device_types.h>

#define MACH_INCLUDE
#include <linux/types.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/blk.h>
#include <linux/proc_fs.h>
#include <linux/kernel_stat.h>
#include <linux/dev/glue/glue.h>

int (*dispatch_scsi_info_ptr) (int ino, char *buffer, char **start,
			       off_t offset, int length, int inout) = 0;

struct kernel_stat kstat;

int
linux_to_mach_error (int err)
{
  switch (err)
    {
    case 0:
      return D_SUCCESS;

    case -EPERM:
      return D_INVALID_OPERATION;

    case -EIO:
      return D_IO_ERROR;

    case -ENXIO:
      return D_NO_SUCH_DEVICE;

    case -EACCES:
      return D_INVALID_OPERATION;

    case -EFAULT:
      return D_INVALID_SIZE;

    case -EBUSY:
      return D_ALREADY_OPEN;

    case -EINVAL:
      return D_INVALID_SIZE;

    case -EROFS:
      return D_READ_ONLY;

    case -EWOULDBLOCK:
      return D_WOULD_BLOCK;

    case -ENOMEM:
      return D_NO_MEMORY;

    default:
      printf ("linux_to_mach_error: unknown code %d\n", err);
      return D_IO_ERROR;
    }
}

int
issig ()
{
  if (!current_thread())
    return 0;
  return current_thread ()->wait_result != THREAD_AWAKENED;
}

int
block_fsync (struct inode *inode, struct file *filp)
{
  return 0;
}

int
verify_area (int rw, const void *p, unsigned long size)
{
  vm_prot_t prot = (rw == VERIFY_WRITE) ? VM_PROT_WRITE : VM_PROT_READ;
  vm_offset_t addr = trunc_page ((vm_offset_t) p);
  vm_size_t len = round_page ((vm_size_t) size);
  vm_map_entry_t entry;

  vm_map_lock_read (current_map ());

  while (1)
    {
      if (!vm_map_lookup_entry (current_map (), addr, &entry)
	  || (entry->protection & prot) != prot)
	{
	  vm_map_unlock_read (current_map ());
	  return -EFAULT;
	}
      if (entry->vme_end - entry->vme_start >= len)
	break;
      len -= entry->vme_end - entry->vme_start;
      addr += entry->vme_end - entry->vme_start;
    }

  vm_map_unlock_read (current_map ());
  return 0;
}

/*
 * Print device name (in decimal, hexadecimal or symbolic) -
 * at present hexadecimal only.
 * Note: returns pointer to static data!
 */
char *
kdevname (kdev_t dev)
{
  static char buffer[32];
  linux_sprintf (buffer, "%02x:%02x", MAJOR (dev), MINOR (dev));
  return buffer;
}

/* RO fail safe mechanism */

static long ro_bits[MAX_BLKDEV][8];

int
is_read_only (kdev_t dev)
{
  int minor, major;

  major = MAJOR (dev);
  minor = MINOR (dev);
  if (major < 0 || major >= MAX_BLKDEV)
    return 0;
  return ro_bits[major][minor >> 5] & (1 << (minor & 31));
}

void
set_device_ro (kdev_t dev, int flag)
{
  int minor, major;

  major = MAJOR (dev);
  minor = MINOR (dev);
  if (major < 0 || major >= MAX_BLKDEV)
    return;
  if (flag)
    ro_bits[major][minor >> 5] |= 1 << (minor & 31);
  else
    ro_bits[major][minor >> 5] &= ~(1 << (minor & 31));
}

struct proc_dir_entry proc_scsi;
struct inode_operations proc_scsi_inode_operations;
struct proc_dir_entry proc_net;
struct inode_operations proc_net_inode_operations;

int
proc_register (struct proc_dir_entry *xxx1, struct proc_dir_entry *xxx2)
{
  return 0;
}

int
proc_unregister (struct proc_dir_entry *xxx1, int xxx2)
{
  return 0;
}

void
add_blkdev_randomness (int major)
{
}

void
do_gettimeofday (struct timeval *tv)
{
  /*
   * XXX: The first argument should be mach_host_self (), but that's too
   * expensive, and the host argument is not used by host_get_time (),
   * only checked not to be HOST_NULL.
   */
  time_value64_t tv64;
  host_get_time64 ((host_t) 1, &tv64);
  tv->tv_sec = tv64.seconds;
  tv->tv_usec = tv64.nanoseconds / 1000;
}

int
dev_get_info (char *buffer, char **start, off_t offset, int length, int dummy)
{
  return 0;
}
