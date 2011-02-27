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
#include <asm/hardirq.h>

extern void linux_timer_intr (void);
extern spl_t splhigh (void);
extern spl_t spl0 (void);
extern void form_pic_mask (void);

#if 0
/* XXX: This is the way it's done in linux 2.2. GNU Mach currently uses intr_count. It should be made using local_{bh/irq}_count instead (through hardirq_enter/exit) for SMP support. */
unsigned int local_bh_count[NR_CPUS];
unsigned int local_irq_count[NR_CPUS];
#endif

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

extern void intnull (), prtnull ();

/*
 * Generic interrupt handler for Linux devices.
 * Set up a fake `struct pt_regs' then call the real handler.
 */
static void
linux_intr (int irq)
{
  struct pt_regs regs;
  struct linux_action *action = *(irq_action + irq);
  unsigned long flags;

  kstat.interrupts[irq]++;
  intr_count++;

  save_flags (flags);
  if (action && (action->flags & SA_INTERRUPT))
    cli ();

  while (action)
    {
      action->handler (irq, action->dev_id, &regs);
      action = action->next;
    }

  restore_flags (flags);

  intr_count--;
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
void
linux_bad_intr (int irq)
{
  mask_irq (irq);
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

      /* Can't share at different levels */
      if (intpri[irq] && linux_intr_pri != intpri[irq])
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

#ifdef __SMP__
unsigned char global_irq_holder = NO_PROC_ID;
unsigned volatile int global_irq_lock;
atomic_t global_irq_count;

atomic_t global_bh_count;
atomic_t global_bh_lock;

/*
 * "global_cli()" is a special case, in that it can hold the
 * interrupts disabled for a longish time, and also because
 * we may be doing TLB invalidates when holding the global
 * IRQ lock for historical reasons. Thus we may need to check
 * SMP invalidate events specially by hand here (but not in
 * any normal spinlocks)
 */
#if 0
/* XXX: check how Mach handles this */
static inline void check_smp_invalidate(int cpu)
{
	if (test_bit(cpu, &smp_invalidate_needed)) {
		clear_bit(cpu, &smp_invalidate_needed);
		local_flush_tlb();
	}
}
#endif

static void show(char * str)
{
	int i;
	unsigned long *stack;
	int cpu = smp_processor_id();
	extern char *get_options(char *str, int *ints);

	printk("\n%s, CPU %d:\n", str, cpu);
	printk("irq:  %d [%d %d]\n",
		atomic_read(&global_irq_count), local_irq_count[0], local_irq_count[1]);
	printk("bh:   %d [%d %d]\n",
		atomic_read(&global_bh_count), local_bh_count[0], local_bh_count[1]);
	stack = (unsigned long *) &stack;
	for (i = 40; i ; i--) {
		unsigned long x = *++stack;
		//if (x > (unsigned long) &get_options && x < (unsigned long) &vsprintf) {
			printk("<[%08lx]> ", x);
		//}
	}
}
	
#define MAXCOUNT 100000000

static inline void wait_on_bh(void)
{
	int count = MAXCOUNT;
	do {
		if (!--count) {
			show("wait_on_bh");
			count = ~0;
		}
		/* nothing .. wait for the other bh's to go away */
	} while (atomic_read(&global_bh_count) != 0);
}

/*
 * I had a lockup scenario where a tight loop doing
 * spin_unlock()/spin_lock() on CPU#1 was racing with
 * spin_lock() on CPU#0. CPU#0 should have noticed spin_unlock(), but
 * apparently the spin_unlock() information did not make it
 * through to CPU#0 ... nasty, is this by design, do we have to limit
 * 'memory update oscillation frequency' artificially like here?
 *
 * Such 'high frequency update' races can be avoided by careful design, but
 * some of our major constructs like spinlocks use similar techniques,
 * it would be nice to clarify this issue. Set this define to 0 if you
 * want to check whether your system freezes.  I suspect the delay done
 * by SYNC_OTHER_CORES() is in correlation with 'snooping latency', but
 * i thought that such things are guaranteed by design, since we use
 * the 'LOCK' prefix.
 */
#define SUSPECTED_CPU_OR_CHIPSET_BUG_WORKAROUND 1

#if SUSPECTED_CPU_OR_CHIPSET_BUG_WORKAROUND
# define SYNC_OTHER_CORES(x) udelay(x+1)
#else
/*
 * We have to allow irqs to arrive between __sti and __cli
 */
# define SYNC_OTHER_CORES(x) __asm__ __volatile__ ("nop")
#endif

static inline void wait_on_irq(int cpu)
{
	int count = MAXCOUNT;

	for (;;) {

		/*
		 * Wait until all interrupts are gone. Wait
		 * for bottom half handlers unless we're
		 * already executing in one..
		 */
		if (!atomic_read(&global_irq_count)) {
			if (local_bh_count[cpu] || !atomic_read(&global_bh_count))
				break;
		}

		/* Duh, we have to loop. Release the lock to avoid deadlocks */
		clear_bit(0,&global_irq_lock);

		for (;;) {
			if (!--count) {
				show("wait_on_irq");
				count = ~0;
			}
			__sti();
			SYNC_OTHER_CORES(cpu);
			__cli();
			//check_smp_invalidate(cpu);
			if (atomic_read(&global_irq_count))
				continue;
			if (global_irq_lock)
				continue;
			if (!local_bh_count[cpu] && atomic_read(&global_bh_count))
				continue;
			if (!test_and_set_bit(0,&global_irq_lock))
				break;
		}
	}
}

/*
 * This is called when we want to synchronize with
 * bottom half handlers. We need to wait until
 * no other CPU is executing any bottom half handler.
 *
 * Don't wait if we're already running in an interrupt
 * context or are inside a bh handler. 
 */
void synchronize_bh(void)
{
	if (atomic_read(&global_bh_count) && !in_interrupt())
		wait_on_bh();
}

/*
 * This is called when we want to synchronize with
 * interrupts. We may for example tell a device to
 * stop sending interrupts: but to make sure there
 * are no interrupts that are executing on another
 * CPU we need to call this function.
 */
void synchronize_irq(void)
{
	if (atomic_read(&global_irq_count)) {
		/* Stupid approach */
		cli();
		sti();
	}
}

static inline void get_irqlock(int cpu)
{
	if (test_and_set_bit(0,&global_irq_lock)) {
		/* do we already hold the lock? */
		if ((unsigned char) cpu == global_irq_holder)
			return;
		/* Uhhuh.. Somebody else got it. Wait.. */
		do {
			do {
				//check_smp_invalidate(cpu);
			} while (test_bit(0,&global_irq_lock));
		} while (test_and_set_bit(0,&global_irq_lock));		
	}
	/* 
	 * We also to make sure that nobody else is running
	 * in an interrupt context. 
	 */
	wait_on_irq(cpu);

	/*
	 * Ok, finally..
	 */
	global_irq_holder = cpu;
}

#define EFLAGS_IF_SHIFT 9

/*
 * A global "cli()" while in an interrupt context
 * turns into just a local cli(). Interrupts
 * should use spinlocks for the (very unlikely)
 * case that they ever want to protect against
 * each other.
 *
 * If we already have local interrupts disabled,
 * this will not turn a local disable into a
 * global one (problems with spinlocks: this makes
 * save_flags+cli+sti usable inside a spinlock).
 */
void __global_cli(void)
{
	unsigned int flags;

	__save_flags(flags);
	if (flags & (1 << EFLAGS_IF_SHIFT)) {
		int cpu = smp_processor_id();
		__cli();
		if (!local_irq_count[cpu])
			get_irqlock(cpu);
	}
}

void __global_sti(void)
{
	int cpu = smp_processor_id();

	if (!local_irq_count[cpu])
		release_irqlock(cpu);
	__sti();
}

/*
 * SMP flags value to restore to:
 * 0 - global cli
 * 1 - global sti
 * 2 - local cli
 * 3 - local sti
 */
unsigned long __global_save_flags(void)
{
	int retval;
	int local_enabled;
	unsigned long flags;

	__save_flags(flags);
	local_enabled = (flags >> EFLAGS_IF_SHIFT) & 1;
	/* default to local */
	retval = 2 + local_enabled;

	/* check for global flags if we're not in an interrupt */
	if (!local_irq_count[smp_processor_id()]) {
		if (local_enabled)
			retval = 1;
		if (global_irq_holder == (unsigned char) smp_processor_id())
			retval = 0;
	}
	return retval;
}

void __global_restore_flags(unsigned long flags)
{
	switch (flags) {
	case 0:
		__global_cli();
		break;
	case 1:
		__global_sti();
		break;
	case 2:
		__cli();
		break;
	case 3:
		__sti();
		break;
	default:
		printk("global_restore_flags: %08lx (%08lx)\n",
			flags, (&flags)[-1]);
	}
}

#endif

static void (*old_clock_handler) ();
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
  
