/*
 * Linux IRQ management.
 * Copyright (C) 1995 Shantanu Goel.
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

/*
 *    linux/arch/i386/kernel/irq.c
 *
 *      Copyright (C) 1992 Linus Torvalds
 */

#include <sys/types.h>
#include <mach/mach_types.h>
#include <mach/vm_param.h>
#include <kern/assert.h>

#include <i386/spl.h>
#include <i386/pic.h>
#include <i386/pit.h>

#define MACH_INCLUDE
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/delay.h>
#include <linux/kernel_stat.h>
#include <linux/malloc.h>
#include <linux/ioport.h>

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/irq.h>
#include <asm/io.h>

extern int linux_timer_intr (void);
extern spl_t splhigh (void);
extern spl_t spl0 (void);
extern void form_pic_mask (void);

/*
 * XXX Move this into more suitable place...
 * Set if the machine has an EISA bus.
 */
int EISA_bus = 0;

/*
 * Priority at which a Linux handler should be called.
 * This is used at the time of an IRQ allocation.  It is
 * set by emulation routines for each class of device.
 */
spl_t linux_intr_pri;

/*
 * Flag indicating an interrupt is being handled.
 */
unsigned long intr_count = 0;

/*
 * List of Linux interrupt handlers.
 */
struct linux_action
{
  void (*handler) (int, void *, struct pt_regs *);
  void *dev_id;
  struct linux_action *next;
  unsigned long flags;
};

static struct linux_action *irq_action[16] =
{
  NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL
};

extern spl_t curr_ipl;
extern int curr_pic_mask;
extern int pic_mask[];

extern int intnull (), prtnull ();

/*
 * Generic interrupt handler for Linux devices.
 * Set up a fake `struct pt_regs' then call the real handler.
 */
static int
linux_intr (int irq)
{
  struct pt_regs regs;
  struct linux_action *action = *(irq_action + irq);

  kstat.interrupts[irq]++;
  intr_count++;

  while (action)
    {
      action->handler (irq, action->dev_id, &regs);
      action = action->next;
    }

  intr_count--;

  /* Not used. by OKUJI Yoshinori. */
  return 0;
}

/*
 * Mask an IRQ.
 */
static inline void
mask_irq (unsigned int irq_nr)
{
  int i;

  for (i = 0; i < intpri[irq_nr]; i++)
    pic_mask[i] |= 1 << irq_nr;

  if (curr_pic_mask != pic_mask[curr_ipl])
    {
      curr_pic_mask = pic_mask[curr_ipl];
      if (irq_nr < 8)
	outb (curr_pic_mask & 0xff, PIC_MASTER_OCW);
      else
	outb (curr_pic_mask >> 8, PIC_SLAVE_OCW);
    }
}

/*
 * Unmask an IRQ.
 */
static inline void
unmask_irq (unsigned int irq_nr)
{
  int mask, i;

  mask = 1 << irq_nr;
  if (irq_nr >= 8)
    mask |= 1 << 2;

  for (i = 0; i < intpri[irq_nr]; i++)
    pic_mask[i] &= ~mask;

  if (curr_pic_mask != pic_mask[curr_ipl])
    {
      curr_pic_mask = pic_mask[curr_ipl];
      if (irq_nr < 8)
	outb (curr_pic_mask & 0xff, PIC_MASTER_OCW);
      else
	outb (curr_pic_mask >> 8, PIC_SLAVE_OCW);
    }
}

void
disable_irq (unsigned int irq_nr)
{
  unsigned long flags;

  assert (irq_nr < NR_IRQS);

  save_flags (flags);
  cli ();
  mask_irq (irq_nr);
  restore_flags (flags);
}

void
enable_irq (unsigned int irq_nr)
{
  unsigned long flags;

  assert (irq_nr < NR_IRQS);

  save_flags (flags);
  cli ();
  unmask_irq (irq_nr);
  restore_flags (flags);
}

/*
 * Default interrupt handler for Linux.
 */
int
linux_bad_intr (int irq)
{
  mask_irq (irq);
  return 0;
}

static int
setup_x86_irq (int irq, struct linux_action *new)
{
  int shared = 0;
  struct linux_action *old, **p;
  unsigned long flags;

  p = irq_action + irq;
  if ((old = *p) != NULL)
    {
      /* Can't share interrupts unless both agree to */
      if (!(old->flags & new->flags & SA_SHIRQ))
	return (-LINUX_EBUSY);

      /* Can't share interrupts unless both are same type */
      if ((old->flags ^ new->flags) & SA_INTERRUPT)
	return (-LINUX_EBUSY);

      /* add new interrupt at end of irq queue */
      do
	{
	  p = &old->next;
	  old = *p;
	}
      while (old);
      shared = 1;
    }

  save_flags (flags);
  cli ();
  *p = new;

  if (!shared)
    {
      ivect[irq] = linux_intr;
      iunit[irq] = irq;
      intpri[irq] = linux_intr_pri;
      unmask_irq (irq);
    }
  restore_flags (flags);
  return 0;
}

/*
 * Attach a handler to an IRQ.
 */
int
request_irq (unsigned int irq, void (*handler) (int, void *, struct pt_regs *),
	     unsigned long flags, const char *device, void *dev_id)
{
  struct linux_action *action;
  int retval;

  assert (irq < 16);

  if (!handler)
    return -LINUX_EINVAL;
  
  /*
   * Hmm... Should I use `kalloc()' ?
   * By OKUJI Yoshinori.
   */
  action = (struct linux_action *)
    linux_kmalloc (sizeof (struct linux_action), GFP_KERNEL);
  if (action == NULL)
    return -LINUX_ENOMEM;
  
  action->handler = handler;
  action->next = NULL;
  action->dev_id = dev_id;
  action->flags = flags;
  
  retval = setup_x86_irq (irq, action);
  if (retval)
    linux_kfree (action);
  
  return retval;
}

/*
 * Deallocate an irq.
 */
void
free_irq (unsigned int irq, void *dev_id)
{
  struct linux_action *action, **p;
  unsigned long flags;

  if (irq > 15)
    panic ("free_irq: bad irq number");

  for (p = irq_action + irq; (action = *p) != NULL; p = &action->next)
    {
      if (action->dev_id != dev_id)
	continue;

      save_flags (flags);
      cli ();
      *p = action->next;
      if (!irq_action[irq])
	{
	  mask_irq (irq);
	  ivect[irq] = linux_bad_intr;
	  iunit[irq] = irq;
	  intpri[irq] = SPL0;
	}
      restore_flags (flags);
      linux_kfree (action);
      return;
    }

  panic ("free_irq: bad irq number");
}

/*
 * Set for an irq probe.
 */
unsigned long
probe_irq_on (void)
{
  unsigned i, irqs = 0;
  unsigned long delay;

  assert (curr_ipl == 0);

  /*
   * Allocate all available IRQs.
   */
  for (i = 15; i > 0; i--)
    {
      if (!irq_action[i] && ivect[i] == linux_bad_intr)
	{
	  intpri[i] = linux_intr_pri;
	  enable_irq (i);
	  irqs |= 1 << i;
	}
    }

  /*
   * Wait for spurious interrupts to mask themselves out.
   */
  for (delay = jiffies + HZ / 10; delay > jiffies;)
    ;

  return (irqs & ~curr_pic_mask);
}

/*
 * Return the result of an irq probe.
 */
int
probe_irq_off (unsigned long irqs)
{
  unsigned int i;

  assert (curr_ipl == 0);

  irqs &= curr_pic_mask;

  /*
   * Disable unnecessary IRQs.
   */
  for (i = 15; i > 0; i--)
    {
      if (!irq_action[i] && ivect[i] == linux_bad_intr)
	{
	  disable_irq (i);
	  intpri[i] = SPL0;
	}
    }
  
  /*
   * Return IRQ number.
   */
  if (!irqs)
    return 0;
  i = ffz (~irqs);
  if (irqs != (irqs & (1 << i)))
    i = -i;
  return i;
}

/*
 * Reserve IRQs used by Mach drivers.
 * Must be called before Linux IRQ detection, after Mach IRQ detection.
 */
static void
reserve_mach_irqs (void)
{
  unsigned int i;

  for (i = 0; i < 16; i++)
    {
      if (ivect[i] != prtnull && ivect[i] != intnull)
	/* Set non-NULL value. */
	irq_action[i] = (struct linux_action *) -1;
    }
}

static int (*old_clock_handler) ();
static int old_clock_pri;

void
init_IRQ (void)
{
  int i;
  char *p;
  int latch = (CLKNUM + hz / 2) / hz;
  
  /*
   * Ensure interrupts are disabled.
   */
  (void) splhigh ();
  
  /*
   * Program counter 0 of 8253 to interrupt hz times per second.
   */
  outb_p (PIT_C0 | PIT_SQUAREMODE | PIT_READMODE, PITCTL_PORT);
  outb_p (latch && 0xff, PITCTR0_PORT);
  outb (latch >> 8, PITCTR0_PORT);
  
  /*
   * Install our clock interrupt handler.
   */
  old_clock_handler = ivect[0];
  old_clock_pri = intpri[0];
  ivect[0] = linux_timer_intr;
  intpri[0] = SPLHI;

  reserve_mach_irqs ();

  for (i = 1; i < 16; i++)
    {
      /*
       * irq2 and irq13 should be igonored.
       */
      if (i == 2 || i == 13)
	continue;
      if (ivect[i] == prtnull || ivect[i] == intnull)
	{
          ivect[i] = linux_bad_intr;
          iunit[i] = i;
          intpri[i] = SPL0;
	}
    }
  
  form_pic_mask ();
  
  /*
   * Enable interrupts.
   */
  (void) spl0 ();
  
  /*
   * Check if the machine has an EISA bus.
   */
  p = (char *) 0x0FFFD9;
  if (*p++ == 'E' && *p++ == 'I' && *p++ == 'S' && *p == 'A')
    EISA_bus = 1;
  
  /*
   * Permanently allocate standard device ports.
   */
  request_region (0x00, 0x20, "dma1");
  request_region (0x20, 0x20, "pic1");
  request_region (0x40, 0x20, "timer");
  request_region (0x70, 0x10, "rtc");
  request_region (0x80, 0x20, "dma page reg");
  request_region (0xa0, 0x20, "pic2");
  request_region (0xc0, 0x20, "dma2");
  request_region (0xf0, 0x10, "npu");
}

void
restore_IRQ (void)
{
  /*
   * Disable interrupts.
   */
  (void) splhigh ();
  
  /*
   * Restore clock interrupt handler.
   */
  ivect[0] = old_clock_handler;
  intpri[0] = old_clock_pri;
  form_pic_mask ();
}
  
