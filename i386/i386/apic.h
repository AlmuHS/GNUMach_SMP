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
        uint32_t r;	/* the actual register */
        uint32_t p[3];	/* pad to the next 128-bit boundary */
} ApicReg;

typedef struct ApicIoUnit {
        ApicReg select;
        ApicReg window;
        ApicReg unused[2];
        ApicReg eoi; /* write the vector you wish to EOI to this reg */
} ApicIoUnit;

struct ioapic_route_entry {
    uint32_t vector      : 8,
            delvmode    : 3, /* 000=fixed 001=lowest 111=ExtInt */
            destmode    : 1, /* 0=physical 1=logical */
            delvstatus  : 1,
            polarity    : 1, /* 0=activehigh 1=activelow */
            irr         : 1,
            trigger     : 1, /* 0=edge 1=level */
            mask        : 1, /* 0=enabled 1=disabled */
            reserved1   : 15;
    uint32_t reserved2   : 24,
            dest        : 8;
} __attribute__ ((packed));

union ioapic_route_entry_union {
    struct {
       uint32_t lo;
       uint32_t hi;
    };
    struct ioapic_route_entry both;
};


/* Grateful to trasterlabs for this snippet */

typedef union u_icr_low
{
    uint32_t value[4];
    struct
    {
        uint32_t r;    // FEE0 0300H - 4 bytes
        unsigned :32;  // FEE0 0304H
        unsigned :32;  // FEE0 0308H
        unsigned :32;  // FEE0 030CH
    };
    struct
    {
        unsigned vector: 8; /* Vector of interrupt. Lowest 8 bits of routine address */
        unsigned delivery_mode : 3;
        unsigned destination_mode: 1;
        unsigned delivery_status: 1;
        unsigned :1;
        unsigned level: 1;
        unsigned trigger_mode: 1;
        unsigned remote_read_status: 2;	/* Read-only field */
        unsigned destination_shorthand: 2;
        unsigned :12;
    };
} IcrLReg;

typedef union u_icr_high
{
    uint32_t value[4];
    struct
    {
        uint32_t r; // FEE0 0310H - 4 bytes
        unsigned :32;  // FEE0 0314H
        unsigned :32;  // FEE0 0318H
        unsigned :32;  // FEE0 031CH
    };
    struct
    {
        unsigned :24; // FEE0 0310H - 4 bytes
        unsigned destination_field :8; /* APIC ID (in physical mode) or MDA (in logical) of destination processor */
    };
} IcrHReg;


typedef enum e_icr_dest_shorthand
{
        NO_SHORTHAND = 0,
        SELF = 1,
        ALL_INCLUDING_SELF = 2,
        ALL_EXCLUDING_SELF = 3
} icr_dest_shorthand;

typedef enum e_icr_deliv_mode
{
        FIXED = 0,
        LOWEST_PRIORITY = 1,
        SMI = 2,
        NMI = 4,
        INIT = 5,
        STARTUP = 6,
} icr_deliv_mode;

typedef enum e_icr_dest_mode
{
        PHYSICAL = 0,
        LOGICAL = 1
} icr_dest_mode;

typedef enum e_icr_deliv_status
{
        IDLE = 0,
        SEND_PENDING = 1
} icr_deliv_status;

typedef enum e_icr_level
{
        DE_ASSERT = 0,
        ASSERT = 1
} icr_level;

typedef enum e_irc_trigger_mode
{
        EDGE = 0,
        LEVEL = 1
} irc_trigger_mode;


typedef struct ApicLocalUnit {
        ApicReg reserved0;               /* 0x000 */
        ApicReg reserved1;               /* 0x010 */
        ApicReg apic_id;                 /* 0x020. Hardware ID of current processor */
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
        IcrLReg icr_low;                 /* 0x300. Store the information to send an IPI (Inter-processor Interrupt) */
        IcrHReg icr_high;                /* 0x310. Store the IPI destination  */
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
        uint8_t  ngsis;
        uint32_t addr;
        uint32_t gsi_base;
        ApicIoUnit *ioapic;
} IoApicData;

#define APIC_IRQ_OVERRIDE_POLARITY_MASK 1
#define APIC_IRQ_OVERRIDE_ACTIVE_LOW 2
#define APIC_IRQ_OVERRIDE_TRIGGER_MASK 4
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
void apic_send_ipi(unsigned dest_shorthand, unsigned deliv_mode, unsigned dest_mode, unsigned level, unsigned trig_mode, unsigned vector, unsigned dest_id);
IrqOverrideData *acpi_get_irq_override(uint8_t gsi);
int apic_get_cpu_apic_id(int kernel_id);
int apic_get_cpu_kernel_id(uint16_t apic_id);
volatile ApicLocalUnit* apic_get_lapic(void);
struct IoApicData *apic_get_ioapic(int kernel_id);
uint8_t apic_get_numcpus(void);
uint8_t apic_get_num_ioapics(void);
int apic_get_current_cpu(void);
void apic_print_info(void);
int apic_refit_cpulist(void);
void apic_generate_cpu_id_lut(void);
int apic_get_total_gsis(void);
void picdisable(void);
void lapic_eoi(void);
void ioapic_irq_eoi(int pin);
void lapic_setup(void);
void lapic_disable(void);
void lapic_enable(void);
void lapic_enable_timer(void);
void calibrate_lapic_timer(void);
void ioapic_toggle(int pin, int mask);
void ioapic_configure(void);

void hpet_init(void);
void hpet_udelay(uint32_t us);
void hpet_mdelay(uint32_t ms);

extern int timer_pin;
extern void intnull(int unit);
extern volatile ApicLocalUnit* lapic;
extern int cpu_id_lut[];
extern uint32_t *hpet_addr;

#endif

#define APIC_IO_UNIT_ID			0x00
#define APIC_IO_VERSION			0x01
# define APIC_IO_VERSION_SHIFT		0
# define APIC_IO_ENTRIES_SHIFT		16
#define APIC_IO_REDIR_LOW(int_pin)	(0x10+(int_pin)*2)
#define APIC_IO_REDIR_HIGH(int_pin)	(0x11+(int_pin)*2)

#define IMCR_SELECT    0x22
#define IMCR_DATA      0x23
#define MODE_IMCR      0x70
# define IMCR_USE_PIC  0
# define IMCR_USE_APIC 1

#define LAPIC_LOW_PRIO                 0x100
#define LAPIC_NMI                      0x400
#define LAPIC_EXTINT                   0x700
#define LAPIC_LEVEL_TRIGGERED          0x8000

#define LAPIC_ENABLE                   0x100
#define LAPIC_FOCUS                    0x200
#define LAPIC_ENABLE_DIRECTED_EOI      0x1000
#define LAPIC_DISABLE                  0x10000
#define LAPIC_TIMER_PERIODIC           0x20000
#define LAPIC_TIMER_DIVIDE_2           0
#define LAPIC_TIMER_DIVIDE_4           1
#define LAPIC_TIMER_DIVIDE_8           2
#define LAPIC_TIMER_DIVIDE_16          3
#define LAPIC_TIMER_BASEDIV            0x100000
#define LAPIC_HAS_DIRECTED_EOI         0x1000000

#define NINTR                          64 /* Max 32 GSIs on each of two IOAPICs */
#define IOAPIC_FIXED                   0
#define IOAPIC_PHYSICAL                0
#define IOAPIC_LOGICAL                 1
#define IOAPIC_NMI                     4
#define IOAPIC_EXTINT                  7
#define IOAPIC_ACTIVE_HIGH             0
#define IOAPIC_ACTIVE_LOW              1
#define IOAPIC_EDGE_TRIGGERED          0
#define IOAPIC_LEVEL_TRIGGERED         1
#define IOAPIC_MASK_ENABLED            0
#define IOAPIC_MASK_DISABLED           1

#define APIC_MSR                       0x1b
#define APIC_MSR_BSP                   0x100 /* Processor is a BSP */
#define APIC_MSR_X2APIC                0x400 /* LAPIC is in x2APIC mode */
#define APIC_MSR_ENABLE                0x800 /* LAPIC is enabled */

/* Set or clear a bit in a 255-bit APIC mask register.
   These registers are spread through eight 32-bit registers.  */
#define APIC_SET_MASK_BIT(reg, bit) \
        ((reg)[(bit) >> 5].r |= 1 << ((bit) & 0x1f))
#define APIC_CLEAR_MASK_BIT(reg, bit) \
        ((reg)[(bit) >> 5].r &= ~(1 << ((bit) & 0x1f)))

#ifndef __ASSEMBLER__

#ifdef APIC
static inline void mask_irq (unsigned int irq_nr) {
    ioapic_toggle(irq_nr, IOAPIC_MASK_DISABLED);
}

static inline void unmask_irq (unsigned int irq_nr) {
    ioapic_toggle(irq_nr, IOAPIC_MASK_ENABLED);
}
#endif

#endif  /* !__ASSEMBLER__ */

#endif /*_IMPS_APIC_*/

