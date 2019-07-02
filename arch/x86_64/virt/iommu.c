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

#include "virt/iommu.h"
#include "vm.h"
#include "utils/screen.h"
#include "debug.h"

static uint8_t *reg_base = NULL;

static inline uint32_t iommu_read32(uint16_t reg)
{
  uint32_t *ptr = (uint32_t *) (reg_base + reg);
  return *ptr;
}

static inline uint64_t iommu_read64(uint16_t reg)
{
  uint64_t *ptr = (uint64_t *) (reg_base + reg);
  return *ptr;
}

void iommu_init(uint64_t base)
{
  reg_base = (uint8_t *) vm_map_page(base, PGT_P | PGT_RW | PGT_PCD | PGT_PWT | PGT_XD);
  if (reg_base == NULL)
    panic("vm_map_page failed for IOMMU base");

  printf("ver %X, cap %llX, ext cap %llX\n",
         iommu_read32(IOMMU_VERSION), iommu_read64(IOMMU_CAP),
         iommu_read64(IOMMU_EXT_CAP));

  //TODO
}
