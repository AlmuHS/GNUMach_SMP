/*
 * Copyright (C) 2020 Free Software Foundation, Inc.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE FREE SOFTWARE FOUNDATION ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE FREE SOFTWARE FOUNDATION DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

#ifndef _I386_IRQ_H
#define _I386_IRQ_H

#ifdef APIC
# include <i386/apic.h>
#else
# include <i386/pic.h>
#endif

typedef unsigned int irq_t;

void __enable_irq (irq_t irq);
void __disable_irq (irq_t irq);

extern struct irqdev irqtab;

#endif
