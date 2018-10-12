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
#include "asm_string.h"
#include "utils/screen.h"
#include "debug.h"
#include "vm.h"
#include "percpu.h"
#include "apic.h"
#include "smp.h"

uint16_t g_cpus = 1;

struct {
  uint8_t irq;
  uint16_t flags;
  uint32_t gsi;
} irq_map[MAX_IRQ_OVERRIDES];

uint8_t num_overrides = 0;

static uint16_t num_ioapic = 0;
static uint8_t lapic_ids[MAX_CPUS];

extern uint8_t ap_boot_start[], ap_boot_end[];

void acpi_sec_init(void)
{
  uint16_t i, count = 0;

  for (i = 1; i < g_cpus; i++) {
    if (!smp_boot_cpu(lapic_ids[i])) {
      printf("%s: Failed to boot cpu %u\n", __func__, lapic_ids[i]);
      count++;
    }
  }
  g_cpus -= count;
}

static bool acpi_add_cpu(apic_lapic_t *info)
{
  /* cpu disabled */
  if (!(info->flags & 1))
    return true;
  /* BSP */
  if (info->apic_id == lapic_get_phys_id(get_pcpu_id()))
    return false;

  lapic_ids[g_cpus++] = info->apic_id;
  if (g_cpus > MAX_CPUS)
    panic("Exceeds supported max CPUs");

  return true;
}

static void acpi_parse_apic(acpi_madt_t *madt)
{
  uint8_t *p, *end;
  extern uint8_t *lapic_addr;
  extern uint8_t *ioapic_addr;
  extern uint32_t gsi_base;
  
  lapic_addr = (uint8_t *) (uint64_t) madt->lapic_addr;

  p = (uint8_t *) (madt + 1);
  end = (uint8_t *) madt + madt->header.length;

  while (p < end) {
    apic_header_t *header = (apic_header_t *) p;
    uint8_t type = header->type;
    uint8_t length = header->length;

    if (type == APIC_TYPE_LAPIC) {
      apic_lapic_t *s = (apic_lapic_t *) p;
      acpi_add_cpu(s);
      //printf("Found CPU: %d %d %x\n", s->processor_id, s->apic_id, s->flags);
    } else if (type == APIC_TYPE_IOAPIC) {
      apic_ioapic_t *s = (apic_ioapic_t *) p;
      ioapic_addr = (uint8_t *) ((uint64_t) s->address);
      gsi_base = s->gsi_base;
      num_ioapic++;
      if (num_ioapic > 1)
        panic("Multiple IOAPIC not supported");
      //printf("Found I/O APIC: %d 0x%08x %d\n", s->id, s->address, s->gsi_base);
    } else if (type == APIC_TYPE_INTERRUPT_OVERRIDE) {
      apic_interruptoverride_t *s = (apic_interruptoverride_t *) p;
      if (s->bus != 0)
        panic("Only bus 0 is supported");
      irq_map[num_overrides].irq = s->irq;
      irq_map[num_overrides].gsi = s->gsi;
      irq_map[num_overrides].flags = s->flags;
      num_overrides++;
      if (num_overrides > MAX_IRQ_OVERRIDES)
        panic("Exceeds supported max overrides");
      //printf("Found Interrupt Override: %d %d %d 0x%04x\n", s->bus, s->irq, s->gsi, s->flags);
    } else {
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
  uint32_t page_start = rsdt & PGT_MASK;
  uint8_t *va = (uint8_t *) vm_map_pages((uint64_t) page_start, 2, PGT_P | PGT_XD);
  uint8_t *rsdt_p = va + rsdt - page_start;
  uint32_t *sdt = (uint32_t *) (rsdt_p + sizeof(acpi_header_t));
  uint32_t *end = (uint32_t *) (rsdt_p + ((acpi_header_t *) rsdt_p)->length);

  if ((uint8_t *) end > va + 2 * PG_SIZE)
    panic("RSDT is too large");

  while (sdt < end) {
    uint64_t address = (uint64_t) *sdt++;
    page_start = address & PGT_MASK;
    uint8_t *va2 = (uint8_t *) vm_map_pages((uint64_t) page_start, 2, PGT_P | PGT_XD);
    acpi_header_t *dt_p = (acpi_header_t *) (va2 + address - page_start);
    acpi_parse_dt(dt_p);
    vm_unmap_pages(va2, 2);
  }

  vm_unmap_pages(va, 2);
}

static void acpi_parse_xsdt(uint64_t xsdt)
{
  /* map two pages in case XSDT spans across page boundary */
  uint64_t page_start = xsdt & PGT_MASK;
  uint8_t *va = (uint8_t *) vm_map_pages((uint64_t) page_start, 2, PGT_P | PGT_XD);
  uint8_t *xsdt_p = va + xsdt - page_start;
  uint64_t *sdt = (uint64_t *) (xsdt_p + sizeof(acpi_header_t));
  uint64_t *end = (uint64_t *) (xsdt_p + ((acpi_header_t *) xsdt_p)->length);

  if ((uint8_t *) end > va + 2 * PG_SIZE)
    panic("XSDT is too large");

  while (sdt < end) {
    uint64_t address = *sdt++;
    page_start = (address >> PG_BITS) << PG_BITS;
    uint8_t *va2 = (uint8_t *) vm_map_pages((uint64_t) page_start, 2, PGT_P | PGT_XD);
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
    panic("Checksum failed");

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
    panic("Unsupported ACPI version");

  return true;
}

void acpi_init(void)
{
  uint8_t *p, *end;

  /* Search main BIOS area below 1MB, identity mapping */
  p = (uint8_t *) 0x000e0000;
  end = (uint8_t *) 0x000fffff;

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
    panic("Can't find RSDP");

  if (g_cpus > 1)
    memcpy((uint8_t *) SMP_BOOT_ADDR, ap_boot_start, ap_boot_end - ap_boot_start);

  //TODO - Search Extended BIOS Data Area
}

uint32_t acpi_irq_to_gsi(uint8_t irq, uint16_t *flags)
{
  uint8_t i;
  for (i = 0; i < num_overrides; i++) {
    if (irq_map[i].irq == irq) {
      if (flags)
        *flags = irq_map[i].flags;
      return irq_map[i].gsi;
    }
  }

  if (flags)
    *flags = 0;
  return (uint32_t) irq;
}
