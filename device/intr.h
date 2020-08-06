/*
 * Copyright (c) 2010, 2011, 2019 Free Software Foundation, Inc.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE FREE SOFTWARE FOUNDATIONALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE FREE SOFTWARE FOUNDATION DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

#ifndef __INTR_H__
#define __INTR_H__

#ifndef MACH_XEN

#include <mach/kern_return.h>
#include <mach/port.h>
#include <kern/queue.h>
#include <ipc/ipc_port.h>
#include <device/conf.h>

#define DEVICE_NOTIFY_MSGH_SEQNO 0

#include <sys/types.h>

struct irqdev;
#include <machine/irq.h>

typedef struct {
  queue_chain_t chain;
  int interrupts; /* Number of interrupts occurred since last run of intr_thread */
  int n_unacked;  /* Number of times irqs were disabled for this */
  ipc_port_t dst_port; /* Notification port */
  int id; /* Mapping to machine dependent irq_t array elem */
} user_intr_t;

struct irqdev {
  char *name;
  void (*irqdev_ack)(struct irqdev *dev, int id);

  queue_head_t *intr_queue;
  int tot_num_intr; /* Total number of unprocessed interrupts */

  /* Machine dependent */
  irq_t irq[NINTR];
};

extern queue_head_t main_intr_queue;
extern int install_user_intr_handler (struct irqdev *dev, int id, unsigned long flags, user_intr_t *e);
extern int deliver_user_intr (struct irqdev *dev, int id, user_intr_t *e);
extern user_intr_t *insert_intr_entry (struct irqdev *dev, int id, ipc_port_t receive_port);

void intr_thread (void);
kern_return_t irq_acknowledge (ipc_port_t receive_port);

#endif /* MACH_XEN */

#endif
