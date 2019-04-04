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

#include "virt/virt_internal.h"
#include "vm.h"
#include "debug.h"
#include "mm/physical.h"
#include "asm_string.h"
#include "utils/screen.h"

/* 
 * size: ram size in MB, even number only
 * returns the physical address of the EPT base 
 */
uint64_t virt_pg_table_setup(uint32_t size)
{
  uint64_t pml4t_phy, pdpt_phy;
  uint64_t *pml4t_virt, *pdpt_virt;
  uint32_t pdt_entries = size >> 1;
  uint32_t pdpt_entries = (pdt_entries + (PG_TABLE_ENTRIES - 1)) >> PG_TABLE_BITS;
  uint32_t count = 0;
  uint32_t i, j;
  uint32_t pages = 1 << (LARGE_PG_BITS - PG_BITS);

  pml4t_phy = alloc_phys_frame();
  if (pml4t_phy == 0)
    panic("Failed to allocate a PML4T frame");
  pml4t_virt = (uint64_t *) vm_map_page(pml4t_phy, PGT_P | PGT_RW);
  if (pml4t_virt == NULL)
    panic("Failed to map PML4T");

  pdpt_phy = alloc_phys_frame();
  if (pdpt_phy == 0)
    panic("Failed to allocate a PDPT frame");
  pdpt_virt = (uint64_t *) vm_map_page(pdpt_phy, PGT_P | PGT_RW);
  if (pdpt_virt == NULL)
    panic("Failed to map PDPT");

  memset(pml4t_virt, 0, PG_SIZE);
  memset(pdpt_virt, 0, PG_SIZE);

  /* max memory size supported: 512GB */
  pml4t_virt[0] = pdpt_phy | EPT_RD | EPT_WR | EPT_EX;

  for (i = 0; i < pdpt_entries; i++) {
    uint64_t *pdt_virt;
    uint64_t frame;
    
    frame = alloc_phys_frame();
    if (frame == 0) 
      panic("Failed to allocate a PDT frame");
    pdpt_virt[i] = frame | EPT_RD | EPT_WR | EPT_EX;

    pdt_virt = (uint64_t *) vm_map_page(frame, PGT_P | PGT_RW);
    if (pdt_virt == NULL)
      panic("Failed to map PDT");

    for (j = 0; j < PG_TABLE_ENTRIES; j++) {
      frame = alloc_phys_frames_aligned(pages, LARGE_PG_SIZE);
      if (frame == 0)
        panic("Failed to allocate a 2MB frame");
      pdt_virt[j] = frame | EPT_RD | EPT_WR | EPT_EX | EPT_PG 
                    | EPT_TP(6) | EPT_IPAT;
      
      count++;
      if (count >= pdt_entries) break;

      // XXX: 32-bit test starts at 2MB
      if (i == 0 && j == 1) {
        uint16_t *test_addr = (uint16_t *) vm_map_page(frame, PGT_P | PGT_RW);
        test_addr[0] = 0x2fb4;
        test_addr[1] = 0x02b9;
        test_addr[2] = 0x0000;
        test_addr[3] = 0xbe00;
        test_addr[4] = 0x0018;
        test_addr[5] = 0x0020;
        test_addr[6] = 0x00bf;
        test_addr[7] = 0x0b80;
        test_addr[8] = 0xac00;
        test_addr[9] = 0xab66;
        test_addr[10] = 0xfbe2;
        test_addr[11] = 0x00f4;
        test_addr[12] = 0x4b4f;//string 'OK' 0x200018
        vm_unmap_page(test_addr);
      }
    }

    vm_unmap_page(pdt_virt);
  }

  vm_unmap_page(pml4t_virt);
  vm_unmap_page(pdpt_virt);

  return pml4t_phy;
}
