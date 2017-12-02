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
#include "io.h"

uint8_t *lapic_addr;

static inline void lapic_write32(uint16_t offset, uint32_t data)
{
  mmio_write32(lapic_addr + offset, data);
}

void lapic_init(void) 
{
  lapic_write32(LAPIC_TPR, 0x00);      /* task priority = 0x0 */
  lapic_write32(LAPIC_LVTT, 0x10000);  /* disable timer interrupt */
  lapic_write32(LAPIC_LVTPC, 0x10000); /* disable PMC interrupt */
  lapic_write32(LAPIC_LVT0, 0x08700);  /* enable normal external interrupts */
  lapic_write32(LAPIC_LVT1, 0x00400);  /* enable NMI */
  lapic_write32(LAPIC_LVTE, 0x10000);  /* disable error interrupts */
  lapic_write32(LAPIC_SPIV, 0x0010F);  /* enable APIC: spurious vector = 0xF */
}
