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

#include "vm.h"
#include "utils/bits.h"

#define VM_ENTRIES (PG_SIZE / sizeof(uint64_t))

uint64_t kernel_pgt[VM_ENTRIES] ALIGNED(PG_SIZE);

void *vm_map_page(uint64_t frame)
{
  uint64_t i;
  void *va;

  for (i = 0; i < VM_ENTRIES; i++) {
    if (!get_bit64(kernel_pgt[i], PGT_P)) {
      kernel_pgt[i] = frame;
      va = (void *) (KERNEL_MAPPING_BASE + (i << PG_BITS));
      invalidate_page(va);

      return va;
    } 
  }
    
  return NULL;
}

void vm_init(void)
{
  uint16_t i;
  uint64_t entry;
  uint64_t *pdpt, *pdt;
  extern uint64_t pml4t[];

  for (i = 0; i < VM_ENTRIES; i++)
    kernel_pgt[i] = 0;

  /* set up 2MB virtual memory for kernel mapping */
  entry = (KERNEL_MAPPING_BASE & 0xFF8000000000) >> 39;
  pdpt = (uint64_t *) pml4t[entry];
  entry = (KERNEL_MAPPING_BASE & 0x7FC0000000) >> 30;
  pdt = (uint64_t *) pdpt[entry];
  entry = (KERNEL_MAPPING_BASE & 0x3FE00000) >> 21;
  pdt[entry] = (uint64_t) kernel_pgt;
  invalidate_page((void *) KERNEL_MAPPING_BASE);
}
