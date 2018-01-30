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
#include "helper.h"

extern seg_desc gdt64[GDT_ENTRY_NR];

uint16_t alloc_tss_desc(tss_t *tss_p)
{
  uint16_t i;
  uint32_t limit = sizeof(tss_t) - 1;

  for (i = GDT_START; i < GDT_ENTRY_NR; i++) {
    if (!gdt64[i].p) {
      tss_desc *entry = (tss_desc *) &gdt64[i];
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
      return i << 3;
    }
  }

  panic(__func__, "out of GDT entries");
  return 0;
}
