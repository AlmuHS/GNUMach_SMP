/*
 * Copyright (C) 1995 Shantanu Goel
 * Copyright (C) 2020 Free Software Foundation, Inc
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

#include <i386/irq.h>
#include <device/intr.h>
#include <mach/kern_return.h>
#include <kern/queue.h>
#include <kern/assert.h>
#include <machine/spl.h>

extern queue_head_t main_intr_queue;

static void
irq_eoi (struct irqdev *dev, int id)
{
#ifdef APIC
  ioapic_irq_eoi (dev->irq[id]);
#endif
}

static unsigned int ndisabled_irq[NINTR];

void
__disable_irq (irq_t irq_nr)
{
  assert (irq_nr < NINTR);

  spl_t s = splhigh();
  ndisabled_irq[irq_nr]++;
  assert (ndisabled_irq[irq_nr] > 0);
  if (ndisabled_irq[irq_nr] == 1)
    mask_irq (irq_nr);
  splx(s);
}

void
__enable_irq (irq_t irq_nr)
{
  assert (irq_nr < NINTR);

  spl_t s = splhigh();
  assert (ndisabled_irq[irq_nr] > 0);
  ndisabled_irq[irq_nr]--;
  if (ndisabled_irq[irq_nr] == 0)
    unmask_irq (irq_nr);
  splx(s);
}

struct irqdev irqtab = {
  "irq", irq_eoi, &main_intr_queue, 0,
#ifdef APIC
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},
#else
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
#endif
};

