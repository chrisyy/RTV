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
#include "percpu.h"
#include "mm/physical.h"
#include "helper.h"
#include "asm/asm-string.h"

uint8_t *percpu_virt[MAX_CPUS];

static uint16_t pcpu_counter = 0;

extern seg_desc gdt64[GDT_ENTRY_NR];
extern void (*_percpu_ctors)();
extern uint64_t _percpu_pages_plus_one;

DEF_PER_CPU(uint16_t, pcpu_id);
INIT_PER_CPU(pcpu_id) {
  percpu_write(pcpu_id, pcpu_counter++);
}

DEF_PER_CPU(tss_t, cpu_tss);

void percpu_init(void)
{
  uint16_t i;
  uint64_t pages = (uint64_t) &_percpu_pages_plus_one;
  /* legal range (inclusive): base -> base + limit */
  uint64_t limit;
  uint64_t start_frame;

  /* workaround: GCC will eliminate this if-statement when the check
   * is (pages == 0) because it thinks it knows the address of a
   * symbol can never be zero. So, check (pages + 1 == 1) and then
   * subtract one. */
  if (pages == 1) {
    /* no per-CPU data, fall back to the usual data segment */
    __asm__ volatile("movw %%ds, %%ax\n"
                     "movw %%ax, %%"PER_CPU_SEG_STR"\n" : : : "ax");
    return;
  }

  pages--;
  limit = pages * PG_SIZE - 1;

  for (i = GDT_START; i < GDT_ENTRY_NR; i++) {
    if (!gdt64[i].p)
      break;
  }
  if (i == GDT_ENTRY_NR) 
    panic(__func__, "out of GDT entries");

  start_frame = alloc_phys_frames(pages);
  if (start_frame == 0) 
    panic(__func__, "out of physical RAM");
  uint64_t start_virt = (uint64_t) vm_map_pages(start_frame, pages, PGT_P | PGT_RW);
  if (start_virt == 0) 
    panic(__func__, "out of virtual addresses");

  percpu_virt[pcpu_counter] = (uint8_t *) start_virt;

  seg_desc seg = {
    .base0 = start_virt & 0xFFFFFF,
    .base1 = (start_virt >> 24) & 0xFF,
    .limit0 = limit & 0xFFFF,
    .limit1 = (limit >> 16) & 0xF,
    .s = 1,
    .type = 2,
    .dpl = 0,
    .p = 1,
    .avl = 0,
    .l = 0,
    .db = 1,
    .g = 0
  };

  memcpy(&gdt64[i], &seg, sizeof(seg_desc));

  i <<= 3;
  asm volatile("movw %0, %%"PER_CPU_SEG_STR"\n" : : "r" (i));

  /* invoke initialization functions */
  void (**ctor)();
  for (ctor = &_percpu_ctors; *ctor; ctor++) {
    (*ctor)();
  }
}
