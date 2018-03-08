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
#include "percpu.h"
#include "cpu.h"
#include "utils/screen.h"

/* Default address: 0xFEE00000 */
uint8_t *lapic_addr = (uint8_t *) 0xFEE00000;

static uint32_t tsc_freq = 0;

static inline void lapic_write32(uint16_t offset, uint32_t data)
{
  mmio_write32(lapic_addr + offset, data);
}

void lapic_init(void) 
{
  uint32_t eax, ebx, ecx;

  lapic_write32(LAPIC_TPR, 0x00);      /* task priority = 0x0 */
  lapic_write32(LAPIC_LVTT, 0x10000);  /* disable timer interrupt */
  lapic_write32(LAPIC_LVTPC, 0x10000); /* disable PMC interrupt */
  lapic_write32(LAPIC_LVT0, 0x08700);  /* enable normal external interrupts */
  lapic_write32(LAPIC_LVT1, 0x00400);  /* enable NMI */
  lapic_write32(LAPIC_LVTE, 0x10000);  /* disable error interrupts */
  lapic_write32(LAPIC_SPIV, 0x0010F);  /* enable APIC: spurious vector = 0xF */

  /* set logical destination */
  lapic_write32(LAPIC_LDR, get_pcpu_id() << LAPIC_LDR_OFFSET);
  lapic_write32(LAPIC_DFR, (uint32_t) -1);

  /* get timer frequency: (ebx/eax)*ecx  */
  cpuid(0x15, 0, &eax, &ebx, &ecx, NULL);
  if (ecx == 0 || ebx == 0)
    panic(__func__, "No TSC frequency info");
  tsc_freq = (ecx * ebx) / eax;
  printf("TSC freq: %u\n", tsc_freq);
}
