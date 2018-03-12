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

#define IOAPIC_REGSEL     0
#define IOAPIC_WIN        0x10
#define IOAPIC_ID         0
#define IOAPIC_VER        0x1
#define IOAPIC_ARB        0x2
#define IOAPIC_REDTBL(n)  (0x10 + 2 * n)  //lower 32 bits (add +1 for upper 32-bits)

extern void apic_init(void);
extern uint8_t lapic_get_phys_id(uint32_t cpu);

#endif
