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

#include "types.h"
#include "multiboot2.h"
#include "utils/screen.h"


void kernel_main(uint64_t magic, uint64_t mbi)
{
  uint32_t size;
  struct multiboot_tag *tag;
  
  if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
    printf("Invalid magic number: %X\n", (uint32_t)magic);
    return;
  }

  if (mbi & 0x7) {
    printf("mbi not aligned");
    return;
  }

  size = *(uint32_t *) mbi;
  printf("Size: %d", size);
  while(1);

  for (tag = (struct multiboot_tag *) (mbi + 8); 
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
                                            + ((tag->size + 7) & ~7))) {
                                            
  }
}
