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

#include "cpu.h"
#include "debug.h"
#include "utils/spinlock.h"

spinlock_t gdt_lock = SPINLOCK_UNLOCKED;

uint16_t alloc_tss_desc(tss_t *tss_p)
{
  uint16_t i;
  uint32_t limit = sizeof(tss_t) - 1;
  seg_desc *gdt_ptr = get_gdt();
  spin_lock(&gdt_lock);

  /* TSS desc takes 16B */
  for (i = GDT_START; i < GDT_ENTRY_NR - 1; i++) {
    if (!gdt_ptr[i].p && !gdt_ptr[i + 1].p) {
      tss_desc *entry = (tss_desc *) &gdt_ptr[i];
      /* legal range: base -> base + limit */
      entry->limit0 = limit & 0xFFFF;
      entry->limit1 = (limit >> 16) & 0xF;
      entry->base0 = ((uint64_t) tss_p) & 0xFFFFFF;
      entry->base1 = ((uint64_t) tss_p) >> 24;
      entry->p = 1;
      entry->type = 9;
      entry->zero0 = 0;
      entry->dpl = 0;
      entry->avl = 0;
      entry->zero1 = 0;
      entry->g = 0;
      entry->reserved0 = 0;

      /* mark the second 8B used */
      gdt_ptr[i + 1].p = 1;
      spin_unlock(&gdt_lock);
      return i << 3;
    }
  }

  spin_unlock(&gdt_lock);
  panic("out of GDT entries");
  return 0;
}

uint64_t get_gdt_tss_base(uint16_t sel)
{
  seg_desc *gdt_ptr = get_gdt();
  tss_desc *entry = (tss_desc *) &gdt_ptr[sel >> 3];
  uint64_t base = entry->base1;
  base = (base << 24) | entry->base0;
  return base;
}
