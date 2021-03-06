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
#include "utils/spinlock.h"

static spinlock_t pg_lock = SPINLOCK_UNLOCKED;

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

  spin_lock(&pg_lock);

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

  spin_unlock(&pg_lock);
}

void *vm_map_page(uint64_t frame, uint64_t flags)
{
  uint64_t i;
  void *va;
  uint64_t *pt;

  spin_lock(&pg_lock);

  pt = get_paging_struct_vaddr((uint64_t *) KERNEL_MAPPING_BASE, 3);

  for (i = 0; i < PG_TABLE_ENTRIES; i++) {
    if (!(pt[i] & PGT_P)) {
      pt[i] = frame | flags;
      va = (void *) (KERNEL_MAPPING_BASE + (i << PG_BITS));
      spin_unlock(&pg_lock);
      return va;
    }
  }

  spin_unlock(&pg_lock);
  return NULL;
}

/* map contiguous physical frames to contiguous virtual memory */
void *vm_map_pages(uint64_t frame, uint64_t num, uint64_t flags)
{
  uint64_t i, j;
  void *va;
  uint64_t count = 0;
  uint64_t *pt;

  if (num < 1) return NULL;

  spin_lock(&pg_lock);

  pt = get_paging_struct_vaddr((uint64_t *) KERNEL_MAPPING_BASE, 3);

  for (i = 0; i < PG_TABLE_ENTRIES - num + 1; i++) {
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
        spin_unlock(&pg_lock);
        return va;
      } else {
        i = j + 1;
        count = 0;
      }
    }
  }

  spin_unlock(&pg_lock);
  return NULL;
}

uint64_t vm_unmap_page(void *va)
{
  uint64_t frame;
  uint64_t *pt;
  uint64_t i = ((uint64_t) va) >> PG_BITS;

  spin_lock(&pg_lock);

  pt = get_paging_struct_vaddr((uint64_t *) va, 3);
  i &= PG_TABLE_MASK;
  frame = pt[i] & PG_MASK;
  pt[i] = 0;
  invalidate_page(va);

  spin_unlock(&pg_lock);
  return frame;
}

uint64_t vm_unmap_pages(void *va, uint64_t num)
{
  uint64_t frame = 0;
  uint64_t *pt;
  uint8_t *pg = (uint8_t *) va;
  uint64_t i = ((uint64_t) va) >> PG_BITS;

  spin_lock(&pg_lock);

  pt = get_paging_struct_vaddr((uint64_t *) va, 3);
  i &= PG_TABLE_MASK;
  while (num > 0) {
    if (frame == 0)
      frame = pt[i] & PG_MASK;
    pt[i] = 0;
    invalidate_page(pg);

    num--;
    i++;
    pg += PG_SIZE;
  }

  spin_unlock(&pg_lock);
  return frame;
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

  spin_lock(&pg_lock);

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

  spin_unlock(&pg_lock);
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

  spin_lock(&pg_lock);

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

  spin_unlock(&pg_lock);
}

void *vm_map_large_page(uint64_t frame, uint64_t flags)
{
  uint64_t i;
  void *va;
  uint64_t *pdt;

  spin_lock(&pg_lock);

  pdt = get_paging_struct_vaddr((uint64_t *) LG_PG_BASE, 2);

  for (i = (LG_PG_BASE >> LARGE_PG_BITS);
       i < (LG_PG_LIMIT >> LARGE_PG_BITS); i++) {
    if (!(pdt[i] & PGT_P)) {
      pdt[i] = frame | flags;
      va = (void *) (i << LARGE_PG_BITS);
      spin_unlock(&pg_lock);
      return va;
    }
  }

  spin_unlock(&pg_lock);
  return NULL;
}

uint64_t vm_unmap_large_page(void *va)
{
  uint64_t frame;
  uint64_t *pdt;
  uint64_t i = ((uint64_t) va) >> LARGE_PG_BITS;

  spin_lock(&pg_lock);

  pdt = get_paging_struct_vaddr((uint64_t *) va, 2);
  i &= PG_TABLE_MASK;
  frame = pdt[i] & LARGE_PG_MASK;
  pdt[i] = 0;
  invalidate_page(va);

  spin_unlock(&pg_lock);
  return frame;
}

void *vm_map_large_pages(uint64_t frame, uint64_t num, uint64_t flags)
{
  uint64_t i, j;
  void *va;
  uint64_t count = 0;
  uint64_t *pdt;

  if (num < 1) return NULL;

  spin_lock(&pg_lock);

  pdt = get_paging_struct_vaddr((uint64_t *) LG_PG_BASE, 2);

  for (i = (LG_PG_BASE >> LARGE_PG_BITS);
       i < (LG_PG_LIMIT >> LARGE_PG_BITS) - num + 1; i++) {
    if (!(pdt[i] & PGT_P)) {
      count++;

      for (j = i + 1; j < i + num; j++) {
        if (!(pdt[j] & PGT_P))
          count++;
        else
          break;
      }

      if (count == num) {
        for (j = i; j < i + num; j++) {
          pdt[j] = frame | flags;
          frame += LARGE_PG_SIZE;
        }

        va = (void *) (i << LARGE_PG_BITS);
        spin_unlock(&pg_lock);
        return va;
      } else {
        i = j + 1;
        count = 0;
      }
    }
  }

  spin_unlock(&pg_lock);
  return NULL;
}

uint64_t vm_unmap_large_pages(void *va, uint64_t num)
{
  uint64_t frame = 0;
  uint64_t *pdt;
  uint8_t *pg = (uint8_t *) va;
  uint64_t i = ((uint64_t) va) >> LARGE_PG_BITS;

  spin_lock(&pg_lock);

  pdt = get_paging_struct_vaddr((uint64_t *) va, 2);
  i &= PG_TABLE_MASK;
  while (num > 0) {
    if (frame == 0)
      frame = pdt[i] & LARGE_PG_MASK;
    pdt[i] = 0;
    invalidate_page(pg);

    num--;
    i++;
    pg += LARGE_PG_SIZE;
  }

  spin_unlock(&pg_lock);
  return frame;
}

void vm_init(void)
{
  uint16_t i;
  uint64_t entry;
  uint64_t new;
  uint64_t *pml4t, *pdpt, *pdt, *pt;

  /* set up 2MB virtual memory for dynamic kernel mapping of 4KB pages */
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
  for (i = 0; i < PG_TABLE_ENTRIES; i++)
    pt[i] = 0;

  /* initialize large page mapping entries */
  for (i = (LG_PG_BASE >> LARGE_PG_BITS);
       i < (LG_PG_LIMIT >> LARGE_PG_BITS); i++)
    pdt[i] = 0;
}
