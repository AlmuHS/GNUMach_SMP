/*
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
#ifndef _IMPS_APIC_
#define _IMPS_APIC_

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef struct ApicReg {
        unsigned r;	/* the actual register */
        unsigned p[3];	/* pad to the next 128-bit boundary */
} ApicReg;

typedef struct ApicIoUnit {
        ApicReg select;
        ApicReg window;
} ApicIoUnit;


typedef struct ApicLocalUnit {
        ApicReg reserved0;               /* 0x000 */
        ApicReg reserved1;               /* 0x010 */
        ApicReg apic_id;                 /* 0x020 */
        ApicReg version;                 /* 0x030 */
        ApicReg reserved4;               /* 0x040 */
        ApicReg reserved5;               /* 0x050 */
        ApicReg reserved6;               /* 0x060 */
        ApicReg reserved7;               /* 0x070 */
        ApicReg task_pri;                /* 0x080 */
        ApicReg arbitration_pri;         /* 0x090 */
        ApicReg processor_pri;           /* 0x0a0 */
        ApicReg eoi;                     /* 0x0b0 */
        ApicReg remote;                  /* 0x0c0 */
        ApicReg logical_dest;            /* 0x0d0 */
        ApicReg dest_format;             /* 0x0e0 */
        ApicReg spurious_vector;         /* 0x0f0 */
        ApicReg isr[8];                  /* 0x100 */
        ApicReg tmr[8];                  /* 0x180 */
        ApicReg irr[8];                  /* 0x200 */
        ApicReg error_status;            /* 0x280 */
        ApicReg reserved28[6];           /* 0x290 */
        ApicReg lvt_cmci;                /* 0x2f0 */
        ApicReg icr_low;                 /* 0x300 */
        ApicReg icr_high;                /* 0x310 */
        ApicReg lvt_timer;               /* 0x320 */
        ApicReg lvt_thermal;             /* 0x330 */
        ApicReg lvt_performance_monitor; /* 0x340 */
        ApicReg lvt_lint0;               /* 0x350 */
        ApicReg lvt_lint1;               /* 0x360 */
        ApicReg lvt_error;               /* 0x370 */
        ApicReg init_count;              /* 0x380 */
        ApicReg cur_count;               /* 0x390 */
        ApicReg reserved3a;              /* 0x3a0 */
        ApicReg reserved3b;              /* 0x3b0 */
        ApicReg reserved3c;              /* 0x3c0 */
        ApicReg reserved3d;              /* 0x3d0 */
        ApicReg divider_config;          /* 0x3e0 */
        ApicReg reserved3f;              /* 0x3f0 */
} ApicLocalUnit;

typedef struct IoApicData {
        uint8_t  apic_id;
        uint32_t addr;
        uint32_t base;
} IoApicData;

#define APIC_IRQ_OVERRIDE_ACTIVE_LOW 2
#define APIC_IRQ_OVERRIDE_LEVEL_TRIGGERED 8

typedef struct IrqOverrideData {
        uint8_t  bus;
        uint8_t  irq;
        uint32_t gsi;
        uint16_t flags;
} IrqOverrideData;

#define MAX_IOAPICS 16
#define MAX_IRQ_OVERRIDE 24

typedef struct ApicInfo {
        uint8_t   ncpus;
        uint8_t   nioapics;
        int       nirqoverride;
        uint16_t* cpu_lapic_list;
        struct    IoApicData ioapic_list[MAX_IOAPICS];
        struct    IrqOverrideData irq_override_list[MAX_IRQ_OVERRIDE];
} ApicInfo;

int apic_data_init(void);
void apic_add_cpu(uint16_t apic_id);
void apic_lapic_init(ApicLocalUnit* lapic_ptr);
void apic_add_ioapic(struct IoApicData);
void apic_add_irq_override(struct IrqOverrideData irq_over);
uint16_t apic_get_cpu_apic_id(int kernel_id);
volatile ApicLocalUnit* apic_get_lapic(void);
struct IoApicData apic_get_ioapic(int kernel_id);
uint8_t apic_get_numcpus(void);
uint8_t apic_get_num_ioapics(void);
uint16_t apic_get_current_cpu(void);
void apic_print_info(void);
int apic_refit_cpulist(void);

#endif

#define APIC_IO_UNIT_ID			0x00
#define APIC_IO_VERSION			0x01
#define APIC_IO_REDIR_LOW(int_pin)	(0x10+(int_pin)*2)
#define APIC_IO_REDIR_HIGH(int_pin)	(0x11+(int_pin)*2)

/* Set or clear a bit in a 255-bit APIC mask register.
   These registers are spread through eight 32-bit registers.  */
#define APIC_SET_MASK_BIT(reg, bit) \
        ((reg)[(bit) >> 5].r |= 1 << ((bit) & 0x1f))
#define APIC_CLEAR_MASK_BIT(reg, bit) \
        ((reg)[(bit) >> 5].r &= ~(1 << ((bit) & 0x1f)))

#endif /*_IMPS_APIC_*/

