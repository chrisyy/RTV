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
#include "asm_string.h"
#include "msr.h"

uint8_t *percpu_virt[MAX_CPUS];

static uint16_t pcpu_counter = 0;

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
  seg_desc *gdt_ptr = get_gdt();
  uint64_t start_virt;

  /* 
   * workaround: GCC will eliminate this if-statement when the check
   * is (pages == 0) because it thinks it knows the address of a
   * symbol can never be zero. So, check (pages + 1 == 1) and then
   * subtract one. 
   */
  if (pages == 1) {
    /* no per-CPU data, fall back to the usual data segment */
    __asm__ volatile("movw %%ds, %%ax\n"
                     "movw %%ax, %%fs\n" : : : "ax");
    return;
  }

  pages--;
  limit = pages * PG_SIZE - 1;

  for (i = GDT_START; i < GDT_ENTRY_NR; i++) {
    if (!gdt_ptr[i].p)
      break;
  }
  if (i == GDT_ENTRY_NR) 
    panic("out of GDT entries");

  start_frame = alloc_phys_frames(pages);
  if (start_frame == 0) 
    panic("out of physical memory");

  start_virt = (uint64_t) vm_map_pages(start_frame, pages, PGT_P | PGT_RW | PGT_XD);
  if (start_virt == 0) 
    panic("out of virtual addresses");
  percpu_virt[pcpu_counter] = (uint8_t *) start_virt;

  gdt_ptr[i].limit0 = limit & 0xFFFF;
  gdt_ptr[i].limit1 = (limit >> 16) & 0xF;
  gdt_ptr[i].s = 1;
  gdt_ptr[i].type = 2;
  gdt_ptr[i].dpl = 0;
  gdt_ptr[i].p = 1;
  gdt_ptr[i].avl = 0;
  gdt_ptr[i].l = 0;
  gdt_ptr[i].db = 1;
  gdt_ptr[i].g = 0;

  i <<= 3;
  __asm__ volatile("movw %0, %%fs\n" : : "r" (i));

  /* can only writes 32-bit address to gdt entry, better writes to the MSR */
  wrmsr(IA32_FS_BASE, start_virt);

  /* invoke initialization functions */
  void (**ctor)();
  for (ctor = &_percpu_ctors; *ctor; ctor++) 
    (*ctor)();
}
