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

#include "apic.h"
#include "acpi.h"
#include "io.h"
#include "vm.h"
#include "percpu.h"
#include "cpu.h"
#include "interrupt.h"
#include "utils/screen.h"

/* Default LAPIC address: 0xFEE00000 */
uint8_t *lapic_addr = (uint8_t *) 0xFEE00000;
/* Default IOAPIC address: 0xFEC00000 */
uint8_t *ioapic_addr = (uint8_t *) 0xFEC00000;
uint32_t gsi_base = 0;

static uint32_t num_gsi = 0;
static uint32_t tsc_freq = 0;
static uint8_t lapic_phys_ids[MAX_CPUS];

static inline void lapic_write32(uint16_t offset, uint32_t data)
{
  mmio_write32(lapic_addr + offset, data);
}

static inline uint32_t lapic_read32(uint16_t offset)
{
  return mmio_read32(lapic_addr + offset);
}

static inline void ioapic_write32_raw(uint8_t offset, uint32_t data)
{
  mmio_write32(ioapic_addr + offset, data);
}

static inline uint32_t ioapic_read32_raw(uint8_t offset)
{
  return mmio_read32(ioapic_addr + offset);
}

static inline void ioapic_write32(uint8_t reg, uint32_t data)
{
  ioapic_write32_raw(IOAPIC_REGSEL, reg);
  ioapic_write32_raw(IOAPIC_WIN, data);
}

static inline uint32_t ioapic_read32(uint8_t reg)
{
  ioapic_write32_raw(IOAPIC_REGSEL, reg);
  return ioapic_read32_raw(IOAPIC_WIN);
}

static inline void ioapic_write64(uint8_t reg, uint64_t data)
{
  /* First, disable the entry by setting the mask bit */
  ioapic_write32_raw(IOAPIC_REGSEL, reg);
  ioapic_write32_raw(IOAPIC_WIN, 0x10000);
  /* Write to the upper half */
  ioapic_write32_raw(IOAPIC_REGSEL, reg + 1);
  ioapic_write32_raw(IOAPIC_WIN, (uint32_t) (data >> 32));
  /* Write to the lower half */
  ioapic_write32_raw(IOAPIC_REGSEL, reg);
  ioapic_write32_raw(IOAPIC_WIN, (uint32_t) (data & 0xFFFFFFFF));
}

void ioapic_map_gsi(uint32_t gsi, uint8_t vector, uint64_t flags)
{
  if (gsi < gsi_base || gsi >= (gsi_base + num_gsi))
    panic(__func__, "Unsupported GSI number");

  gsi -= gsi_base;
  ioapic_write64(IOAPIC_REDTBL(gsi), flags | vector);
}

static inline uint8_t lapic_get_phys_id_raw(void)
{
  return (uint8_t) ((lapic_read32(LAPIC_ID) >> LAPIC_ID_OFFSET) & 0xF);
}

/* return LAPIC ID */
uint8_t lapic_get_phys_id(uint32_t cpu)
{
  return lapic_phys_ids[cpu];
}

void lapic_eoi(void)
{
  lapic_write32(LAPIC_EOI, 0);
}

/* dest: LAPIC ID */
void lapic_send_ipi(uint32_t dest, uint32_t vector)
{
  uint64_t flag;

  /* 
   * It is a bad idea to have interrupts enabled while 
   * twiddling the LAPIC. 
   */
  interrupt_disable_save(&flag);

  lapic_write32(LAPIC_ICRH, dest << LAPIC_ID_OFFSET);
  lapic_write32(LAPIC_ICR, vector);

  interrupt_enable_restore(flag);
}

void lapic_init(void) 
{
  uint32_t cpu;
  uint32_t eax, ebx, ecx;
  cpu = get_pcpu_id();

  if (cpu == 0) {
    /* identity mapping for LAPIC */
    vm_map_page_unrestricted((uint64_t) lapic_addr, PGT_P | PGT_RW | PGT_PCD | PGT_PWT, (uint64_t) lapic_addr);

    /* get timer frequency: (ebx/eax)*ecx */
    cpuid(0x15, 0, &eax, &ebx, &ecx, NULL);
    if (ecx == 0 || ebx == 0)
      panic(__func__, "No TSC frequency info");
    tsc_freq = (ecx * ebx) / eax;
    printf("TSC freq: %u\n", tsc_freq);
  }

  //printf("%s: %u\n", __func__, lapic_read32(LAPIC_VER));

  lapic_write32(LAPIC_TPR, 0x00);      /* task priority = 0x0 */
  lapic_write32(LAPIC_LVTT, 0x10000);  /* disable timer interrupt */
  lapic_write32(LAPIC_LVTPC, 0x10000); /* disable PMC interrupt */
  lapic_write32(LAPIC_LVT0, 0x08700);  /* enable normal external interrupts */
  lapic_write32(LAPIC_LVT1, 0x00400);  /* enable NMI */
  lapic_write32(LAPIC_LVTE, 0x10000);  /* disable error interrupts */
  lapic_write32(LAPIC_SPIV, 0x0010F);  /* enable APIC: spurious vector = 0xF */

  lapic_phys_ids[cpu] = lapic_get_phys_id_raw();
}

void ioapic_init(void)
{
  uint32_t i, gsi;
  uint16_t flags;
  uint64_t entry;

  /* disable PIC before using IOAPIC */
  outb(PIC1_DATA, 0xFF);
  outb(PIC2_DATA, 0xFF);

  /* identity mapping for IOAPIC */
  vm_map_page_unrestricted((uint64_t) ioapic_addr, PGT_P | PGT_RW | PGT_PCD | PGT_PWT, (uint64_t) ioapic_addr);

  //printf("%s: %u\n", __func__, ioapic_read32(IOAPIC_VER));

  num_gsi = ((ioapic_read32(IOAPIC_VER) >> 16) & 0xFF) + 1;
  for (i = 0; i < num_gsi; i++)
    ioapic_write64(IOAPIC_REDTBL(i), 0x0000000000010000LL);

  gsi = acpi_irq_to_gsi(PIT_IRQ, &flags);
  /* physical mode, broadcast */
  entry = 0xFF00000000000000LL;
  /* if active low */
  if (flags & 0x2)
    entry |= 0x2000;
  /* if level-triggered */
  if (flags & 0x8)
    entry |= 0x8000;
  /* map PIT timer IRQ to vector 0x20 */
  ioapic_map_gsi(gsi, 0x20, entry);
}
