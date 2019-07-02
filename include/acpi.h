#ifndef _ACPI_H_
#define _ACPI_H_

#include "types.h"

typedef struct _acpi_header {
  uint32_t signature;
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem[6];
  char table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} PACKED acpi_header_t;

typedef struct _acpi_madt {
  acpi_header_t header;
  uint32_t lapic_addr;
  uint32_t flags;
} PACKED acpi_madt_t;

typedef struct _acpi_dmar {
  acpi_header_t header;
  uint8_t addr_width;
  uint8_t flags;
  uint8_t reserved[10];
} PACKED acpi_dmar_t;

typedef struct _dmar_header {
  uint16_t type;
  uint16_t length;
} PACKED dmar_header_t;

typedef struct _dmar_drhd {
  dmar_header_t header;
  uint8_t flags;
  uint8_t reserved;
  uint16_t segment;
  uint64_t base_addr;
} PACKED dmar_drhd_t;

/* DMAR remapping structure types */
#define DMAR_TYPE_DRHD    0
#define DMAR_TYPE_RMRR    1
#define DMAR_TYPE_ATSR    2
#define DMAR_TYPE_RHSA    3
#define DMAR_TYPE_ANDD    4

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
  uint8_t irq;
  uint32_t gsi;
  uint16_t flags;
} PACKED apic_interruptoverride_t;

#define MAX_IRQ_OVERRIDES 32

extern uint16_t g_cpus;

extern void acpi_init(uint8_t *rsdp);
extern void acpi_sec_init(void);
extern uint32_t acpi_irq_to_gsi(uint8_t irq, uint16_t *flags);

#endif
