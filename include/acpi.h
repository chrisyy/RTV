#ifndef _ACPI_H_
#define _ACPI_H_

#include "types.h"

typedef struct _acpi_header {
  uint32_t signature;
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  uint8_t oem[6];
  uint8_t table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} PACKED acpi_header_t;

typedef struct _acpi_madt {
  acpi_header_t header;
  uint32_t lapic_addr;
  uint32_t flags;
} PACKED acpi_madt_t;

typedef struct _apic_header {
  uint8_t type;
  uint8_t length;
} PACKED apic_header_t;

/* APIC structure types */
#define APIC_TYPE_LAPIC                 0
#define APIC_TYPE_IOAPIC                1
#define APIC_TYPE_INTERRUPT_OVERRIDE    2

typedef struct _apic_lapic {
  apic_header_t header;
  uint8_t processor_id;
  uint8_t apic_id;
  uint32_t flags;
} PACKED apic_lapic_t;

typedef struct _apic_ioapic {
  apic_header_t header;
  uint8_t id;
  uint8_t reserved;
  uint32_t address;
  uint32_t gsi_base;
} PACKED apic_ioapic_t;

typedef struct _apic_interruptoverride {
  apic_header_t header;
  uint8_t bus;
  uint8_t source;
  uint32_t interrupt;
  uint16_t flags;
} PACKED apic_interruptoverride_t;

extern void acpi_init(void);
extern uint8_t acpi_irq_map(uint8_t irq);

#endif
