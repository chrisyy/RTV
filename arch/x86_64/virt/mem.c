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
#include "virt/linux.h"

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
  /* number of 4KB pages */
  uint32_t count = 0, bound = pdt_entries * PG_TABLE_ENTRIES;
  uint32_t i, j;

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

  /* mark memory allocated to linux */
  physical_take_range(LINUX_PHY_START, size << 20);

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
      uint64_t frame_offset = PG_SIZE * PG_TABLE_ENTRIES * PG_TABLE_ENTRIES * i
                              + PG_SIZE * PG_TABLE_ENTRIES * j;

      /* first 2MB */
      if (i == 0 && j == 0) {
        uint16_t k;
        uint64_t *pt_virt;
        uint64_t pt_frame = alloc_phys_frame();
        if (pt_frame == 0)
          panic("Failed to allocate a PT frame");
        pdt_virt[j] = pt_frame | EPT_RD | EPT_WR | EPT_EX;

        pt_virt = (uint64_t *) vm_map_page(pt_frame, PGT_P | PGT_RW);
        if (pt_virt == NULL)
          panic("Failed to map PT");

        frame_offset -= PG_SIZE;
        for (k = 0; k < PG_TABLE_ENTRIES; k++) {
          frame_offset += PG_SIZE;
          /* identity mapping for the first MB */
          if (k == 0xB8)
            pt_virt[k] = frame_offset | EPT_RD | EPT_WR | EPT_TP(EPT_TYPE_UC);
          else if (k < PG_TABLE_ENTRIES / 2)
            pt_virt[k] = frame_offset | EPT_RD | EPT_WR | EPT_EX
                         | EPT_TP(EPT_TYPE_WB);
          else
            pt_virt[k] = (frame_offset + LINUX_PHY_START) | EPT_RD
                         | EPT_WR | EPT_EX | EPT_TP(EPT_TYPE_WB);

          //TODO: 32-bit test starts at 1MB
          if (k == PG_TABLE_ENTRIES / 2) {
            uint16_t *test_addr = (uint16_t *) vm_map_page(frame_offset
                                  + LINUX_PHY_START, PGT_P | PGT_RW);

            //print OK: movl imm32, 0xB8000
            test_addr[0] = 0x05c7;
            test_addr[1] = 0x8000;
            test_addr[2] = 0x000b;
            test_addr[3] = 0x2f4f;//(green)O
            test_addr[4] = 0x2f4b;//(green)K
            test_addr[5] = 0xf4f4;//hlt

            vm_unmap_page(test_addr);
          }
        }

        vm_unmap_page(pt_virt);
      } else {
        pdt_virt[j] = (frame_offset + LINUX_PHY_START) | EPT_RD | EPT_WR
                      | EPT_EX | EPT_PG | EPT_TP(EPT_TYPE_WB);
      }

      count += PG_TABLE_ENTRIES;
      if (count >= bound) break;
    }

    vm_unmap_page(pdt_virt);
  }

  vm_unmap_page(pml4t_virt);
  vm_unmap_page(pdpt_virt);

  return pml4t_phy;
}
