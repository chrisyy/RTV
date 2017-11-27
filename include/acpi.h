#ifndef _ACPI_H_
#define _ACPI_H_

#include "types.h"

typedef struct _ACPI_header {
  uint32_t signature;
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  uint8_t oem[6];
  uint8_t table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} PACKED ACPI_header;

typedef struct _ACPI_MADT {
  ACPI_header header;
  uint32_t LAPIC_addr;
  uint32_t flags;
} PACKED ACPI_MADT;

typedef struct _APIC_header {
  uint8_t type;
  uint8_t length;
} PACKED APIC_header;

/* APIC structure types */
#define APIC_TYPE_LAPIC                 0
#define APIC_TYPE_IOAPIC                1
#define APIC_TYPE_INTERRUPT_OVERRIDE    2

typedef struct _APIC_LAPIC {
  APIC_header header;
  uint8_t processor_id;
  uint8_t apic_id;
  uint32_t flags;
} PACKED APIC_LAPIC;

typedef struct APIC_IOAPIC {
  APIC_header header;
  uint8_t id;
  uint8_t reserved;
  uint32_t address;
  uint32_t GSI_base;
} PACKED APIC_IOAPIC;

typedef struct _APIC_InterruptOverride {
  APIC_header header;
  uint8_t bus;
  uint8_t source;
  uint32_t interrupt;
  uint16_t flags;
} PACKED APIC_InterruptOverride;

extern void ACPI_init(void);

#endif
