/* GNU Mach Kernel Message Device.

   Copyright (C) 1998, 1999, 2007 Free Software Foundation, Inc.

   Written by OKUJI Yoshinori.

This is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This software is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the software; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* kmsg provides a stream interface.  */

#include <sys/types.h>
#include <string.h>

#include <device/conf.h>
#include <device/ds_routines.h>
#include <device/io_req.h>
#include <mach/boolean.h>
#include <kern/lock.h>
#include <device/kmsg.h>


#define KMSGBUFSIZE	(4096)  /* XXX */

/* Simple array for buffering messages */
static char kmsg_buffer[KMSGBUFSIZE];
/* Point to the offset to write */
static int kmsg_write_offset;
/* Point to the offset to read */
static int kmsg_read_offset;
/* I/O request queue for blocking read */
static queue_head_t kmsg_read_queue;
/* Used for exclusive access to the device */
static int kmsg_in_use;
/* Used for exclusive access to the routines */
decl_simple_lock_data (static, kmsg_lock);
/* If already initialized or not  */
static int kmsg_init_done = 0;

/* Kernel Message Initializer */
static void
kmsginit (void)
{
  kmsg_write_offset = 0;
  kmsg_read_offset = 0;
  queue_init (&kmsg_read_queue);
  kmsg_in_use = 0;
  simple_lock_init (&kmsg_lock);
}

/* Kernel Message Open Handler */
io_return_t
kmsgopen (dev_t dev, int flag, io_req_t ior)
{
  simple_lock (&kmsg_lock);
  if (kmsg_in_use)
    {
      simple_unlock (&kmsg_lock);
      return D_ALREADY_OPEN;
    }
  
  kmsg_in_use = 1;

  simple_unlock (&kmsg_lock);
  return D_SUCCESS;
}

/* Kernel Message Close Handler */
io_return_t
kmsgclose (dev_t dev, int flag)
{
  simple_lock (&kmsg_lock);
  kmsg_in_use = 0;
  
  simple_unlock (&kmsg_lock);
  return D_SUCCESS;
}

static boolean_t kmsg_read_done (io_req_t ior);

/* Kernel Message Read Handler */
io_return_t
kmsgread (dev_t dev, io_req_t ior)
{
  int err;
  int amt, len;
  
  err = device_read_alloc (ior, ior->io_count);
  if (err != KERN_SUCCESS)
    return err;

  simple_lock (&kmsg_lock);
  if (kmsg_read_offset == kmsg_write_offset)
    {
      /* The queue is empty.  */
      if (ior->io_mode & D_NOWAIT)
	{
	  simple_unlock (&kmsg_lock);
	  return D_WOULD_BLOCK;
	}

      ior->io_done = kmsg_read_done;
      enqueue_tail (&kmsg_read_queue, (queue_entry_t) ior);
      simple_unlock (&kmsg_lock);
      return D_IO_QUEUED;
    }

  len = kmsg_write_offset - kmsg_read_offset;
  if (len < 0)
    len += KMSGBUFSIZE;

  amt = ior->io_count;
  if (amt > len)
    amt = len;
  
  if (kmsg_read_offset + amt <= KMSGBUFSIZE)
    {
      memcpy (ior->io_data, kmsg_buffer + kmsg_read_offset, amt);
    }
  else
    {
      int cnt;

      cnt = KMSGBUFSIZE - kmsg_read_offset;
      memcpy (ior->io_data, kmsg_buffer + kmsg_read_offset, cnt);
      memcpy (ior->io_data + cnt, kmsg_buffer, amt - cnt);
    }

  kmsg_read_offset += amt;
  if (kmsg_read_offset >= KMSGBUFSIZE)
    kmsg_read_offset -= KMSGBUFSIZE;
  
  ior->io_residual = ior->io_count - amt;
  
  simple_unlock (&kmsg_lock);
  return D_SUCCESS;
}

static boolean_t
kmsg_read_done (io_req_t ior)
{
  int amt, len;

  simple_lock (&kmsg_lock);
  if (kmsg_read_offset == kmsg_write_offset)
    {
      /* The queue is empty.  */
      ior->io_done = kmsg_read_done;
      enqueue_tail (&kmsg_read_queue, (queue_entry_t) ior);
      simple_unlock (&kmsg_lock);
      return FALSE;
    }

  len = kmsg_write_offset - kmsg_read_offset;
  if (len < 0)
    len += KMSGBUFSIZE;

  amt = ior->io_count;
  if (amt > len)
    amt = len;
  
  if (kmsg_read_offset + amt <= KMSGBUFSIZE)
    {
      memcpy (ior->io_data, kmsg_buffer + kmsg_read_offset, amt);
    }
  else
    {
      int cnt;

      cnt = KMSGBUFSIZE - kmsg_read_offset;
      memcpy (ior->io_data, kmsg_buffer + kmsg_read_offset, cnt);
      memcpy (ior->io_data + cnt, kmsg_buffer, amt - cnt);
    }

  kmsg_read_offset += amt;
  if (kmsg_read_offset >= KMSGBUFSIZE)
    kmsg_read_offset -= KMSGBUFSIZE;
  
  ior->io_residual = ior->io_count - amt;

  simple_unlock (&kmsg_lock);
  ds_read_done (ior);
  
  return TRUE;
}

io_return_t
kmsggetstat (dev_t dev, int flavor, int *data, unsigned int *count)
{
  switch (flavor)
    {
    case DEV_GET_SIZE:
      data[DEV_GET_SIZE_DEVICE_SIZE] = 0;
      data[DEV_GET_SIZE_RECORD_SIZE] = 1;
      *count = DEV_GET_SIZE_COUNT;
      break;

    default:
      return D_INVALID_OPERATION;
    }

  return D_SUCCESS;
}

/* Write to Kernel Message Buffer */
void
kmsg_putchar (int c)
{
  io_req_t ior;
  int offset;

  /* XXX: cninit is not called before cnputc is used. So call kmsginit
     here if not initialized yet.  */
  if (!kmsg_init_done)
    {
      kmsginit ();
      kmsg_init_done = 1;
    }
  
  simple_lock (&kmsg_lock);
  offset = kmsg_write_offset + 1;
  if (offset == KMSGBUFSIZE)
    offset = 0;

  if (offset == kmsg_read_offset)
    {
      /* Discard C.  */
      simple_unlock (&kmsg_lock);
      return;
    }

  kmsg_buffer[kmsg_write_offset++] = c;
  if (kmsg_write_offset == KMSGBUFSIZE)
    kmsg_write_offset = 0;

  while ((ior = (io_req_t) dequeue_head (&kmsg_read_queue)) != NULL)
    iodone (ior);

  simple_unlock (&kmsg_lock);
}
