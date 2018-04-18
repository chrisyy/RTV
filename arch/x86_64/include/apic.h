#ifndef _APIC_H_
#define _APIC_H_

#include "types.h"

/* Local APIC registers */
#define LAPIC_ID    0x20
#define LAPIC_VER   0x30
#define LAPIC_TPR   0x80
#define LAPIC_APR   0x90
#define LAPIC_PPR   0xA0
#define LAPIC_EOI   0xB0
#define LAPIC_RRD   0xC0
#define LAPIC_LDR   0xD0
#define LAPIC_DFR   0xE0
#define LAPIC_SPIV  0xF0
#define LAPIC_ISR   0x100
#define LAPIC_TMR   0x180
#define LAPIC_IRR   0x200
#define LAPIC_ESR   0x280
#define LAPIC_CMCI  0x2F0
#define LAPIC_ICR   0x300
#define LAPIC_ICRH  0x310
#define LAPIC_LVTT  0x320
#define LAPIC_TSR   0x330
#define LAPIC_LVTPC 0x340
#define LAPIC_LVT0  0x350
#define LAPIC_LVT1  0x360
#define LAPIC_LVTE  0x370
#define LAPIC_TICR  0x380
#define LAPIC_TCCR  0x390
#define LAPIC_TDCR  0x3E0

#define LAPIC_ID_OFFSET   24

/* Only INIT IPI uses this: */
#define         LAPIC_ICR_TM_LEVEL              0x8000  /* level vs edge mode */
/* Only INIT de-assert would NOT use this: */
#define         LAPIC_ICR_LEVELASSERT           0x4000  /* level assert */
#define         LAPIC_ICR_STATUS_PEND           0x1000  /* status check, readonly */
#define         LAPIC_ICR_DM_LOGICAL            0x800   /* logical destination mode */

/* Delivery mode: */
#define LAPIC_ICR_DM_FIXED  0x000   /* FIXED delivery */
#define LAPIC_ICR_DM_LOWPRI 0x100   /* send to lowest-priority CPU */
#define LAPIC_ICR_DM_SMI    0x200   /* send SMI */
#define LAPIC_ICR_DM_NMI    0x400   /* send NMI */
#define LAPIC_ICR_DM_INIT   0x500   /* send INIT */
#define LAPIC_ICR_DM_SIPI   0x600   /* send Startup IPI */

#define IOAPIC_REGSEL     0
#define IOAPIC_WIN        0x10
#define IOAPIC_ID         0
#define IOAPIC_VER        0x1
#define IOAPIC_ARB        0x2
#define IOAPIC_REDTBL(n)  (0x10 + 2 * n)  //lower 32 bits (add +1 for upper 32-bits)

extern void apic_init(void);
extern uint8_t lapic_get_phys_id(uint32_t cpu);
extern void lapic_eoi(void);
extern void lapic_send_ipi(uint32_t dest, uint32_t vector);

#endif
