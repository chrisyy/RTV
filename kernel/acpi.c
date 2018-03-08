/*                       RTV Real-Time Hypervisor
 * Copyright (C) 2017  Ying Ye
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "acpi.h"
#include "asm/asm-string.h"
#include "utils/screen.h"
#include "helper.h"
#include "vm.h"

uint16_t g_cpus = 0;

static acpi_madt_t *acpi_madt;

static void acpi_parse_apic(acpi_madt_t *madt)
{
  uint8_t *p, *end;
  extern uint8_t *lapic_addr;
  
  acpi_madt = madt;
  lapic_addr = (uint8_t *) (uint64_t) madt->lapic_addr;

  p = (uint8_t *) (madt + 1);
  end = (uint8_t *) madt + madt->header.length;

  while (p < end) {
    apic_header_t *header = (apic_header_t *) p;
    uint8_t type = header->type;
    uint8_t length = header->length;

    if (type == APIC_TYPE_LAPIC) {
      //apic_lapic_t *s = (apic_lapic_t *) p;
      //printf("Found CPU: %d %d %x\n", s->processor_id, s->apic_id, s->flags);
      ++g_cpus;
    }
    else if (type == APIC_TYPE_IOAPIC) {
      //apic_ioapic_t *s = (apic_ioapic_t *) p;

      //printf("Found I/O APIC: %d 0x%08x %d\n", s->id, s->address, s->gsi_base);
      //g_ioApicAddr = (uint8_t *)s->ioApicAddress;
    }
    else if (type == APIC_TYPE_INTERRUPT_OVERRIDE) {
      //apic_interruptoverride_t *s = (apic_interruptoverride_t *) p;
      //printf("Found Interrupt Override: %d %d %d 0x%04x\n", s->bus, s->source, s->interrupt, s->flags);
    }
    else {
      //printf("Unsupported APIC structure %d\n", type);
    }

    p += length;
  }
}

static void acpi_parse_dt(acpi_header_t *header)
{ 
  //char sigstr[5];
  //memcpy(sigstr, &signature, 4);
  //sigstr[4] = 0;
  //printf("%s 0x%x\n", sigstr, signature);

  /* "APIC" */
  if (header->signature == 0x43495041) 
    acpi_parse_apic((acpi_madt_t *) header);
}

static void acpi_parse_rsdt(uint32_t rsdt)
{
  /* map two pages in case RSDT spans across page boundary */
  uint32_t page_start = (rsdt >> PG_BITS) << PG_BITS;
  uint8_t *va = (uint8_t *) vm_map_pages((uint64_t) page_start, 2, PGT_P);
  uint8_t *rsdt_p = va + rsdt - page_start;
  uint32_t *sdt = (uint32_t *) (rsdt_p + sizeof(acpi_header_t));
  uint32_t *end = (uint32_t *) (rsdt_p + ((acpi_header_t *) rsdt_p)->length);

  if ((uint8_t *) end > va + 2 * PG_SIZE)
    panic(__func__, "RSDT is too large");

  while (sdt < end) {
    uint64_t address = (uint64_t) *sdt++;
    page_start = (address >> PG_BITS) << PG_BITS;
    uint8_t *va2 = (uint8_t *) vm_map_pages((uint64_t) page_start, 2, PGT_P);
    acpi_header_t *dt_p = (acpi_header_t *) (va2 + address - page_start); 
    acpi_parse_dt(dt_p);
    vm_unmap_pages(va2, 2);
  }

  vm_unmap_pages(va, 2);
}

static void acpi_parse_xsdt(uint64_t xsdt)
{
  /* map two pages in case XSDT spans across page boundary */
  uint64_t page_start = (xsdt >> PG_BITS) << PG_BITS;
  uint8_t *va = (uint8_t *) vm_map_pages((uint64_t) page_start, 2, PGT_P);
  uint8_t *xsdt_p = va + xsdt - page_start;
  uint64_t *sdt = (uint64_t *) (xsdt_p + sizeof(acpi_header_t));
  uint64_t *end = (uint64_t *) (xsdt_p + ((acpi_header_t *) xsdt_p)->length);

  if ((uint8_t *) end > va + 2 * PG_SIZE)
    panic(__func__, "XSDT is too large");

  while (sdt < end) {
    uint64_t address = *sdt++;
    page_start = (address >> PG_BITS) << PG_BITS;
    uint8_t *va2 = (uint8_t *) vm_map_pages((uint64_t) page_start, 2, PGT_P);
    acpi_header_t *dt_p = (acpi_header_t *) (va2 + address - page_start);
    acpi_parse_dt(dt_p);
    vm_unmap_pages(va2, 2);
  }

  vm_unmap_pages(va, 2);
}

static bool acpi_parse_rsdp(uint8_t *p)
{
  // Verify checksum
  uint8_t sum = 0;
  uint8_t i;
  uint8_t revision;

  for (i = 0; i < 20; ++i) 
    sum += p[i];
  if (sum) 
    panic(__func__, "Checksum failed");

  // Check version
  revision = p[15];
  if (revision == 0) {
    //printf("Version 1\n");

    uint32_t rsdt_addr = *(uint32_t *) (p + 16); 
    acpi_parse_rsdt(rsdt_addr);
  } else if (revision == 2) {
    //printf("Version 2\n");

    uint32_t rsdt_addr = *(uint32_t *) (p + 16);
    uint64_t xsdt_addr = *(uint64_t *) (p + 24);

    if (xsdt_addr)
      acpi_parse_xsdt(xsdt_addr);
    else
      acpi_parse_rsdt(rsdt_addr);
  } else
    panic(__func__, "Unsupported ACPI version");

  return true;
}

void acpi_init(void)
{
  // TODO - Search Extended BIOS Data Area

  /* Search main BIOS area below 1MB */
  uint8_t *p = (uint8_t *) 0x000e0000;
  uint8_t *end = (uint8_t *) 0x000fffff;

  while (p < end) {
    uint64_t signature = *(uint64_t *)p;

    /* "RSD PTR" */
    if (signature == 0x2052545020445352) {
      if (acpi_parse_rsdp(p)) 
          break;
    }

    p += 16;
  }

  if (p >= end)
    panic(__func__, "Can't find RSDP");
}

uint8_t acpi_irq_map(uint8_t irq)
{
  acpi_madt_t *madt = acpi_madt;

  uint8_t *p = (uint8_t *) (madt + 1);
  uint8_t *end = (uint8_t *) madt + madt->header.length;

  while (p < end) {
    apic_header_t *header = (apic_header_t *) p;
    uint8_t type = header->type;
    uint8_t length = header->length;

    if (type == APIC_TYPE_INTERRUPT_OVERRIDE) {
      apic_interruptoverride_t *s = (apic_interruptoverride_t *) p;

      if (s->source == irq)
        return s->interrupt;
    }

    p += length;
  }

  return irq;
}
