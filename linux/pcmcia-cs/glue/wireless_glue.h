/*
 * wireless network glue code
 *
 * Copyright (C) 2006 Free Software Foundation, Inc.
 * Written by Stefan Siegl <stesie@brokenpipe.de>.
 *
 * This file is part of GNU Mach.
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

#ifndef _WIRELESS_GLUE_H
#define _WIRELESS_GLUE_H

/*
 * Wireless glue configuration.
 */

/*
 * Include the pcmcia glue as well, in case the kernel is configured for
 * it.
 */
#ifdef CONFIG_PCMCIA
#define PCMCIA_CLIENT
#include "pcmcia_glue.h"
#endif


/*
 * Definition of a `BUG' function: write message and panic out.
 */
#ifndef BUG
#define BUG() \
  do { printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
    *(int *)0=0; } while (0)
#endif


/*
 * We compile everything directly into the GNU Mach kernel, there are no
 * modules.
 */
#define SET_MODULE_OWNER(a)     do{ } while(0)
#define EXPORT_SYMBOL(a)



/*
 * We need some `schedule_task' replacement.  This is defined in
 * kernel/context.c in the Linux kernel.
 */
static inline int
schedule_task(struct tq_struct *task)
{
  printk(KERN_INFO "schedule_task: not implemented, task=%p\n", task);
  Debugger("schedule_task");
  return 0; /* fail */
}


/*
 * min() and max() macros that also do strict type-checking.  See the
 * "unnecessary" pointer comparison.
 */
#define min(x,y) ({ \
        const typeof(x) _x = (x);       \
        const typeof(y) _y = (y);       \
        (void) (&_x == &_y);            \
        _x < _y ? _x : _y; })

#define max(x,y) ({ \
        const typeof(x) _x = (x);       \
        const typeof(y) _y = (y);       \
        (void) (&_x == &_y);            \
        _x > _y ? _x : _y; })

/*
 * ... and if you can't take the strict types, you can specify one
 * yourself.
 */
#define min_t(type,x,y) \
        ({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#define max_t(type,x,y) \
        ({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })


#define DEV_KFREE_SKB(skb)      dev_kfree_skb(skb, FREE_WRITE)


/*
 * TODO: this is i386 specific.
 */
#define le16_to_cpus(x)  do { } while(0)


/*
 * Some wireless drivers check for a return value from `copy_to_user',
 * however the `memcpy_tofs' implementation does return void.
 */
#undef copy_to_user
#define copy_to_user(a,b,c)    ((memcpy_tofs(a,b,c)), 0)


/*
 * Some more macros that are available on 2.2 and 2.4 Linux kernels.
 */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


/*
 * TQUEUE glue.
 */
#define PREPARE_TQUEUE(_tq, _routine, _data)                    \
  do {                                                          \
    (_tq)->routine = _routine;                                  \
    (_tq)->data = _data;					\
  } while (0)
#define INIT_TQUEUE(_tq, _routine, _data)                       \
  do {								\
    (_tq)->next = 0;						\
    (_tq)->sync = 0;						\
    PREPARE_TQUEUE((_tq), (_routine), (_data));			\
  } while (0)


/*
 * `etherdev' allocator.
 */
static inline struct net_device *
alloc_etherdev(int sz)
{
  struct net_device *dev;
  sz += sizeof(*dev) + 31;

  if (!(dev = kmalloc(sz, GFP_KERNEL)))
    return NULL;
  memset(dev, 0, sz);

  if (sz)
    dev->priv = (void *)(((long)dev + sizeof(*dev) + 31) & ~31);

  /* just allocate some space for the device name,
   * register_netdev will happily provide one to us 
   */
  dev->name = kmalloc(8, GFP_KERNEL);
  dev->name[0] = 0;

  ether_setup(dev);
  return dev;
}


#endif /* _WIRELESS_GLUE_H */
