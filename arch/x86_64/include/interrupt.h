#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include "types.h"

typedef struct _idt_entry {
  uint16_t offset0;
  uint16_t selector;
  uint16_t type;
  uint16_t offset1;
  uint16_t offset2;
  uint32_t reserved;
} idt_entry;

#define IDT_ENTRY_NR 256

typedef struct _idt_desc {
  uint16_t limit;
  uint64_t base;
} idt_desc;

#define INTERRUPT_GATE_TYPE 0x8E00
#define TRAP_GATE_TYPE 0x8F00

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)

#define PIC1_BASE_IRQ 0x20
#define PIC2_BASE_IRQ 0x28

void interrupt_init(void);

#endif
