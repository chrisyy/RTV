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
#include "helper.h"

#define VM_TABLE_ENTRIES (PG_SIZE / 64)

#define VM_TABLE_MASK ((1 << (PG_BITS - 3)) - 1)

uint64_t kernel_pgt[VM_TABLE_ENTRIES] ALIGNED(PG_SIZE);

void *vm_map_page(uint64_t frame, uint64_t flags)
{
  uint64_t i;
  void *va;

  for (i = 0; i < VM_TABLE_ENTRIES; i++) {
    if (!get_bit64(kernel_pgt[i], PGT_P)) {
      kernel_pgt[i] = frame | flags;
      va = (void *) (KERNEL_MAPPING_BASE + (i << PG_BITS));

      return va;
    } 
  }
    
  return NULL;
}

/* map contiguous physical frames to contiguous virtual memory */
void *vm_map_pages(uint64_t frame, size_t num, uint64_t flags)
{
  uint64_t i, j;
  void *va;
  size_t count = 0;

  if (num < 1) return NULL;

  for (i = 0; i < VM_TABLE_ENTRIES - num + 1; i++) {
    if (!get_bit64(kernel_pgt[i], PGT_P)) {
      count++;

      for (j = i + 1; j < i + num; j++) {
        if (!get_bit64(kernel_pgt[j], PGT_P))
          count++;
        else
          break;
      }

      if (count == num) {
        for (j = i; j < i + num; j++) {
          kernel_pgt[j] = frame | flags;
          frame += PG_SIZE;
        }

        va = (void *) (KERNEL_MAPPING_BASE + (i << PG_BITS));
        return va;
      } else {
        i = j;
        count = 0;
      }
    }
  }

  return NULL;
}

void vm_unmap_page(void *va)
{
  uint64_t i = ((uint64_t) va) >> PG_BITS;
  i &= VM_TABLE_MASK;

  kernel_pgt[i] = 0;
  invalidate_page(va);
}

void vm_unmap_pages(void *va, size_t num)
{
  uint8_t *pg = (uint8_t *) va;
  uint64_t i = ((uint64_t) va) >> PG_BITS;
  i &= VM_TABLE_MASK;

  while (num > 0) {
    kernel_pgt[i] = 0;
    invalidate_page(pg);

    num--;
    i++;
    pg += PG_SIZE;
  }
}

void vm_init(void)
{
  uint16_t i;
  uint64_t entry;
  uint64_t *pdpt, *pdt;
  extern uint64_t pml4t[];

  for (i = 0; i < VM_TABLE_ENTRIES; i++)
    kernel_pgt[i] = 0;

  /* set up 2MB virtual memory for kernel mapping */
  entry = (KERNEL_MAPPING_BASE & 0xFF8000000000) >> 39;
  if ((pml4t[entry] & PGT_P) == 0) 
    panic(__func__, "need to allocate page for PDPT");
  pdpt = (uint64_t *) (pml4t[entry] & PGT_MASK);

  entry = (KERNEL_MAPPING_BASE & 0x7FC0000000) >> 30;
  if ((pdpt[entry] & PGT_P) == 0) 
    panic(__func__, "need to allocate page for PDT");
  pdt = (uint64_t *) (pdpt[entry] & PGT_MASK);

  entry = (KERNEL_MAPPING_BASE & 0x3FE00000) >> 21;
  pdt[entry] = (uint64_t) kernel_pgt | PGT_P | PGT_RW;
}
