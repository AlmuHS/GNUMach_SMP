/*
 * Copyright (c) 2010, 2011, 2016, 2019 Free Software Foundation, Inc.
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

#include <kern/assert.h>
#include <device/intr.h>
#include <device/device_types.h>
#include <device/device_port.h>
#include <device/notify.h>
#include <kern/printf.h>
#include <machine/spl.h>
#include <machine/irq.h>
#include <ipc/ipc_space.h>

#ifndef MACH_XEN

queue_head_t main_intr_queue;
static boolean_t deliver_intr (int id, ipc_port_t dst_port);

#ifndef LINUX_DEV
#define SA_SHIRQ 0x04000000

struct intr_list {
  user_intr_t *user_intr;
  unsigned long flags;
  struct intr_list *next;
};
static struct intr_list *user_intr_handlers[NINTR];
#endif

static user_intr_t *
search_intr (struct irqdev *dev, ipc_port_t dst_port)
{
  user_intr_t *e;
  queue_iterate (dev->intr_queue, e, user_intr_t *, chain)
    {
      if (e->dst_port == dst_port)
	return e;
    }
  return NULL;
}


/*
 * Interrupt handling logic:
 *
 * interrupt.S raises spl (thus IF cleared)
 * interrupt.S EOI
 * interrupt.S calls the handler
 *   - for pure in-kernel handlers, they do whatever they want with IF cleared.
 *   - when a userland handler is registered, queue_intr masks the irq.
 * interrupt.S lowers spl with splx_cli, thus IF still cleared
 * iret, that also sets IF
 *
 * later on, (irq_acknowledge), userland acks the IRQ, that unmasks the irq
 */
kern_return_t
irq_acknowledge (ipc_port_t receive_port)
{
  user_intr_t *e;
  kern_return_t ret = 0;

  spl_t s = splhigh ();
  e = search_intr (&irqtab, receive_port);

  if (!e)
    {
      printf("didn't find user intr for interrupt !?\n");
      ret = KERN_INVALID_ARGUMENT;
    }
  else
    {
      if (!e->n_unacked)
        ret = D_INVALID_OPERATION;
      else
        e->n_unacked--;
    }
  splx (s);

  if (ret)
    return ret;

  __enable_irq (irqtab.irq[e->id]);

  return D_SUCCESS;
}

/* This function can only be used in the interrupt handler. */
static void
queue_intr (struct irqdev *dev, int id, user_intr_t *e)
{
  /* Until userland has handled the IRQ in the driver, we have to keep it
   * disabled. Level-triggered interrupts would keep raising otherwise. */
  __disable_irq (dev->irq[id]);

  spl_t s = splhigh ();
  e->n_unacked++;
  e->interrupts++;
  dev->tot_num_intr++;
  splx (s);

  thread_wakeup ((event_t) &intr_thread);
}

int
deliver_user_intr (struct irqdev *dev, int id, user_intr_t *e)
{
  /* The reference of the port was increased
   * when the port was installed.  If the reference is 1, it means
   * the port was deallocated and we should clean after it. */
  if (!e->dst_port || e->dst_port->ip_references == 1)
    {
      thread_wakeup ((event_t) &intr_thread);
      return 0;
    }
  else
    {
      queue_intr (dev, id, e);
      return 1;
    }
}

/* insert an interrupt entry in the queue.
 * This entry exists in the queue until
 * the corresponding interrupt port is removed.*/
user_intr_t *
insert_intr_entry (struct irqdev *dev, int id, ipc_port_t dst_port)
{
  user_intr_t *e, *new, *ret;
  int free = 0;

  new = (user_intr_t *) kalloc (sizeof (*new));
  if (new == NULL)
    return NULL;

  /* check whether the intr entry has been in the queue. */
  spl_t s = splhigh ();
  e = search_intr (dev, dst_port);
  if (e)
    {
      printf ("the interrupt entry for irq[%d] and port %p has already been inserted\n", id, dst_port);
      free = 1;
      ret = NULL;
      goto out;
    }
  printf("irq handler [%d]: new delivery port %p entry %p\n", id, dst_port, new);
  ret = new;
  new->id = id;
  new->dst_port = dst_port;
  new->interrupts = 0;
  new->n_unacked = 0;

  queue_enter (dev->intr_queue, new, user_intr_t *, chain);
out:
  splx (s);
  if (free)
    kfree ((vm_offset_t) new, sizeof (*new));
  return ret;
}

#ifndef LINUX_DEV

static void
user_irq_handler (int id)
{
  struct intr_list *handler;
  struct intr_list **prev = &user_intr_handlers[id];
  user_intr_t *e;
  spl_t s;

  s = splhigh();

  for (handler = *prev; handler; handler = handler->next)
    {
      e = handler->user_intr;
      if (!deliver_user_intr(&irqtab, id, e))
        {
          /* We failed to deliver this interrupt, remove handler from list */
	  *prev = handler->next;
        }
      prev = &handler->next;
    }
  splx(s);
}

int
install_user_intr_handler (struct irqdev *dev, int id, unsigned long flags,
			  user_intr_t *user_intr)
{
  unsigned int irq = dev->irq[id];
  struct intr_list **head = &user_intr_handlers[id];
  struct intr_list *new, *old = *head;
  spl_t s;

  flags |= SA_SHIRQ;

  assert (irq < NINTR);

  /* Don't allow overriding hardclock/kdintr etc */
  if ((ivect[irq] != user_irq_handler) && (ivect[irq] != intnull))
    {
      printf("You can't have this interrupt\n");
      return D_ALREADY_OPEN;
    }

  if (old)
    {
      if (!(old->flags & flags & SA_SHIRQ))
        {
          printf ("Cannot share irq\n");
          return D_ALREADY_OPEN;
        }
    }

  new = (struct intr_list *)kalloc (sizeof (struct intr_list));
  new->user_intr = user_intr;
  new->flags = flags;

  s = splhigh();
  new->next = *head;
  *head = new;
  ivect[irq] = user_irq_handler;
  iunit[irq] = (int)irq;
  unmask_irq (irq);
  splx(s);

  return D_SUCCESS;
}
#endif

void
intr_thread (void)
{
  user_intr_t *e;
  int id;
  ipc_port_t dst_port;
  queue_init (&main_intr_queue);

  for (;;)
    {
      assert_wait ((event_t) &intr_thread, FALSE);
      /* Make sure we wake up from times to times to check for aborted processes */
      thread_set_timeout (hz);
      spl_t s = splhigh ();

      /* Now check for interrupts */
      int del;
      do
	{
	  del = 0;

	  queue_iterate (&main_intr_queue, e, user_intr_t *, chain)
	    {
	      /* The reference of the port was increased
               * when the port was installed.  If the reference is 1, it means
               * the port was deallocated and we should clean after it. */
	      if (e->dst_port->ip_references == 1)
		{
		  clear_wait (current_thread (), 0, 0);
		  del = 1;
		  break;
		}

	      if (e->interrupts)
		{
		  clear_wait (current_thread (), 0, 0);
		  id = e->id;
		  dst_port = e->dst_port;
		  e->interrupts--;
		  irqtab.tot_num_intr--;

		  splx (s);
		  deliver_intr (id, dst_port);
		  s = splhigh ();
		}
	    }

	  /* remove the entry without dest port from the queue and free it. */
	  if (del)
	    {
	      /*
	       * We clear unacked irqs now, so the Linux handling can trigger,
	       * and we will cleanup later after the Linux handler is cleared.
	       */
	      assert (!queue_empty (&main_intr_queue));
	      queue_remove (&main_intr_queue, e, user_intr_t *, chain);

	      printf ("irq handler [%d]: release a dead delivery port %p entry %p\n", e->id, e->dst_port, e);
	      ipc_port_release (e->dst_port);
	      e->dst_port = MACH_PORT_NULL;

	      if (e->n_unacked)
		printf("irq handler [%d]: still %d unacked irqs in entry %p\n", e->id, e->n_unacked, e);
	      while (e->n_unacked)
	      {
		__enable_irq (irqtab.irq[e->id]);
		e->n_unacked--;
	      }

#if 0
#ifndef LINUX_DEV
	      // TODO: remove from the action list
#else
	      // FIXME: with the Linux irq handler we don't actually control the action list
#endif
	      splx (s);
	      kfree ((vm_offset_t) e, sizeof (*e));
	      s = splhigh ();
#endif
	    }
	}
      while (del || irqtab.tot_num_intr);
      splx (s);
      thread_block (NULL);
    }
}

static boolean_t
deliver_intr (int id, ipc_port_t dst_port)
{
  ipc_kmsg_t kmsg;
  device_intr_notification_t *n;
  mach_port_t dest = (mach_port_t) dst_port;

  if (dest == MACH_PORT_NULL)
    return FALSE;

  kmsg = ikm_alloc(sizeof *n);
  if (kmsg == IKM_NULL)
    return FALSE;

  ikm_init(kmsg, sizeof *n);
  n = (device_intr_notification_t *) &kmsg->ikm_header;

  mach_msg_header_t *m = &n->intr_header;
  mach_msg_type_t *t = &n->intr_type;

  m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND, 0);
  m->msgh_size = sizeof *n;
  m->msgh_seqno = DEVICE_NOTIFY_MSGH_SEQNO;
  m->msgh_local_port = MACH_PORT_NULL;
  m->msgh_remote_port = MACH_PORT_NULL;
  m->msgh_id = DEVICE_INTR_NOTIFY;

  t->msgt_name = MACH_MSG_TYPE_INTEGER_32;
  t->msgt_size = 32;
  t->msgt_number = 1;
  t->msgt_inline = TRUE;
  t->msgt_longform = FALSE;
  t->msgt_deallocate = FALSE;
  t->msgt_unused = 0;

  n->intr_header.msgh_remote_port = dest;
  n->id = id;

  ipc_port_copy_send (dst_port);
  ipc_mqueue_send_always(kmsg);

  return TRUE;
}

#endif	/* MACH_XEN */
