/*
 *    linux/kernel/softirq.c
 *
 *      Copyright (C) 1992 Linus Torvalds
 *
 * do_bottom_half() runs at normal kernel priority: all interrupts
 * enabled.  do_bottom_half() is atomic with respect to itself: a
 * bottom_half handler need not be re-entrant.
 */

#define MACH_INCLUDE
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <asm/system.h>

int bh_mask_count[32];
unsigned long bh_active = 0;
unsigned long bh_mask = 0;
void (*bh_base[32]) (void);

void
linux_soft_intr (void)
{
  unsigned long active;
  unsigned long mask, left;
  void (**bh) (void);

  sti ();
  bh = bh_base;
  active = bh_active & bh_mask;
  for (mask = 1, left = ~0; left & active; bh++, mask += mask, left += left)
    {
      if (mask & active)
	{
	  void (*fn) (void);
	  bh_active &= ~mask;
	  fn = *bh;
	  if (!fn)
	    goto bad_bh;
	  fn ();
	}
    }
  return;
bad_bh:
  printk ("linux_soft_intr:bad interrupt handler entry %08lx\n", mask);
}
