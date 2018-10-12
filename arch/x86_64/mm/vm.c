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
#include "mm/physical.h"
#include "utils/bits.h"
#include "utils/screen.h"
#include "debug.h"
#include "asm_string.h"

#define VM_TABLE_ENTRIES (PG_SIZE / sizeof(uint64_t))
#define VM_TABLE_MASK ((1 << (PG_BITS - 3)) - 1)

/* canonical address, level 0-3: pml4t to pt */
static uint64_t *get_paging_struct_vaddr(uint64_t *addr, uint8_t level)
{
  uint64_t bits = (uint64_t) addr;

  switch (level) {
  case 0:
    return (uint64_t *) 0xFFFFFFFFFFFFF000;

  case 1:
    return (uint64_t *) (((bits >> 27) | 0xFFFFFFFFFFE00000) & PGT_MASK);

  case 2:
    return (uint64_t *) (((bits >> 18) | 0xFFFFFFFFC0000000) & PGT_MASK);

  case 3:
    return (uint64_t *) (((bits >> 9) | 0xFFFFFF8000000000) & PGT_MASK);

  default:
    panic("invalid paging level option");
  }

  return NULL;
}

void vm_check_mapping(uint64_t *addr)
{
  uint64_t *pml4t, *pdpt, *pdt, *pt;
  uint64_t entry;

  pml4t = get_paging_struct_vaddr(addr, 0);
  entry = ((uint64_t) addr & 0xFF8000000000) >> 39;
  printf("PML4T entry: %llX\n", pml4t[entry]);
  pdpt = get_paging_struct_vaddr(addr, 1);
  entry = ((uint64_t) addr & 0x7FC0000000) >> 30;
  printf("PDPT entry: %llX\n", pdpt[entry]);
  pdt = get_paging_struct_vaddr(addr, 2);
  entry = ((uint64_t) addr & 0x3FE00000) >> 21;
  printf("PDT entry: %llX\n", pdt[entry]);
  pt = get_paging_struct_vaddr(addr, 3);
  entry = ((uint64_t) addr & 0x1FF000) >> 12;
  printf("PT entry: %llX\n", pt[entry]);
}

void *vm_map_page(uint64_t frame, uint64_t flags)
{
  uint64_t i;
  void *va;
  uint64_t *pt = get_paging_struct_vaddr((uint64_t *) KERNEL_MAPPING_BASE, 3);

  for (i = 0; i < VM_TABLE_ENTRIES; i++) {
    if (!(pt[i] & PGT_P)) {
      pt[i] = frame | flags;
      va = (void *) (KERNEL_MAPPING_BASE + (i << PG_BITS));
      return va;
    }
  }
    
  return NULL;
}

/* map contiguous physical frames to contiguous virtual memory */
void *vm_map_pages(uint64_t frame, uint64_t num, uint64_t flags)
{
  uint64_t i, j;
  void *va;
  uint64_t count = 0;
  uint64_t *pt = get_paging_struct_vaddr((uint64_t *) KERNEL_MAPPING_BASE, 3);

  if (num < 1) return NULL;

  for (i = 0; i < VM_TABLE_ENTRIES - num + 1; i++) {
    if (!(pt[i] & PGT_P)) {
      count++;

      for (j = i + 1; j < i + num; j++) {
        if (!(pt[j] & PGT_P))
          count++;
        else
          break;
      }

      if (count == num) {
        for (j = i; j < i + num; j++) {
          pt[j] = frame | flags;
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

uint64_t vm_unmap_page(void *va)
{
  uint64_t frame;
  uint64_t *pt = get_paging_struct_vaddr((uint64_t *) va, 3);
  uint64_t i = ((uint64_t) va) >> PG_BITS;

  i &= VM_TABLE_MASK;
  frame = pt[i] & PG_MASK;
  pt[i] = 0;
  invalidate_page(va);
  return frame;
}

uint64_t vm_unmap_pages(void *va, uint64_t num)
{
  uint64_t frame = 0;
  uint64_t *pt = get_paging_struct_vaddr((uint64_t *) va, 3);
  uint8_t *pg = (uint8_t *) va;
  uint64_t i = ((uint64_t) va) >> PG_BITS;

  i &= VM_TABLE_MASK;
  while (num > 0) {
    if (frame == 0)
      frame = pt[i] & PG_MASK;
    pt[i] = 0;
    invalidate_page(pg);

    num--;
    i++;
    pg += PG_SIZE;
  }
  return frame;
}

/* unmap and free the physical memory */
void vm_free_page(void *va)
{
  uint64_t frame;

  /* clear data for security */
  memset(va, 0, PG_SIZE);
  frame = vm_unmap_page(va);
  free_phys_frame(frame);
}

/* 
 * flags: use PGT_{x} flags
 * vaddr: page-aligned
 */
void vm_map_page_unrestricted(uint64_t frame, uint64_t flags, uint64_t vaddr)
{
  uint64_t new;
  uint64_t entry;
  uint64_t *pml4t, *pdpt, *pdt, *pt;
  
  pml4t = get_paging_struct_vaddr((uint64_t *) vaddr, 0);
  entry = (vaddr & 0xFF8000000000) >> 39;
  if ((pml4t[entry] & PGT_P) == 0) { 
    new = alloc_phys_frame();
    if (new == 0)
      panic("page allocation for PDPT failed");
    pml4t[entry] = new | PGT_P | PGT_RW;
  }

  pdpt = get_paging_struct_vaddr((uint64_t *) vaddr, 1);
  entry = (vaddr & 0x7FC0000000) >> 30;
  if ((pdpt[entry] & PGT_P) == 0) {
    new = alloc_phys_frame();
    if (new == 0)
      panic("page allocation for PDT failed");
    pdpt[entry] = new | PGT_P | PGT_RW;
  }

  pdt = get_paging_struct_vaddr((uint64_t *) vaddr, 2);
  entry = (vaddr & 0x3FE00000) >> 21;
  if ((pdt[entry] & PGT_P) == 0) {
    new = alloc_phys_frame();
    if (new == 0)
      panic("page allocation for PT failed");
    pdt[entry] = new | PGT_P | PGT_RW;
  }

  pt = get_paging_struct_vaddr((uint64_t *) vaddr, 3);
  entry = (vaddr & 0x1FF000) >> 12;
  if (pt[entry] & PGT_P)
    panic("page table entry not available");
  pt[entry] = frame | flags;
}

/* 
 * flags: use LG_PGT_{x} flags
 * vaddr: large-page-aligned
 */
void vm_map_large_page_unrestricted(uint64_t frame, uint64_t flags, uint64_t vaddr)
{
  uint64_t new;
  uint64_t entry;
  uint64_t *pml4t, *pdpt, *pdt;
  
  pml4t = get_paging_struct_vaddr((uint64_t *) vaddr, 0);
  entry = (vaddr & 0xFF8000000000) >> 39;
  if ((pml4t[entry] & PGT_P) == 0) { 
    new = alloc_phys_frame();
    if (new == 0)
      panic("page allocation for PDPT failed");
    pml4t[entry] = new | PGT_P | PGT_RW;
  }

  pdpt = get_paging_struct_vaddr((uint64_t *) vaddr, 1);
  entry = (vaddr & 0x7FC0000000) >> 30;
  if ((pdpt[entry] & PGT_P) == 0) {
    new = alloc_phys_frame();
    if (new == 0)
      panic("page allocation for PDT failed");
    pdpt[entry] = new | PGT_P | PGT_RW;
  }

  pdt = get_paging_struct_vaddr((uint64_t *) vaddr, 2);
  entry = (vaddr & 0x3FE00000) >> 21;
  if (pdt[entry] & PGT_P)
    panic("page table entry not available");
  pdt[entry] = frame | flags;
}

void vm_init(void)
{
  uint16_t i;
  uint64_t entry;
  uint64_t new;
  uint64_t *pml4t, *pdpt, *pdt, *pt;

  /* set up 2MB virtual memory for dynamic kernel mapping */
  pml4t = get_paging_struct_vaddr((uint64_t *) KERNEL_MAPPING_BASE, 0);
  entry = (KERNEL_MAPPING_BASE & 0xFF8000000000) >> 39;
  if ((pml4t[entry] & PGT_P) == 0) { 
    new = alloc_phys_frame();
    if (new == 0)
      panic("page allocation for PDPT failed");
    pml4t[entry] = new | PGT_P | PGT_RW;
  }

  pdpt = get_paging_struct_vaddr((uint64_t *) KERNEL_MAPPING_BASE, 1);
  entry = (KERNEL_MAPPING_BASE & 0x7FC0000000) >> 30;
  if ((pdpt[entry] & PGT_P) == 0) {
    new = alloc_phys_frame();
    if (new == 0)
      panic("page allocation for PDT failed");
    pdpt[entry] = new | PGT_P | PGT_RW;
  }

  pdt = get_paging_struct_vaddr((uint64_t *) KERNEL_MAPPING_BASE, 2);
  entry = (KERNEL_MAPPING_BASE & 0x3FE00000) >> 21;
  if ((pdt[entry] & PGT_P) == 0) {
    new = alloc_phys_frame();
    if (new == 0)
      panic("page allocation for PT failed");
    pdt[entry] = new | PGT_P | PGT_RW;
  }

  pt = get_paging_struct_vaddr((uint64_t *) KERNEL_MAPPING_BASE, 3);
  for (i = 0; i < VM_TABLE_ENTRIES; i++) 
    pt[i] = 0;
}
