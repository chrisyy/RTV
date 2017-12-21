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

#include "multiboot2.h"
#include "utils/screen.h"
#include "interrupt.h"
#include "cpu.h"
#include "acpi.h"
#include "vm.h"
#include "apic.h"
#include "mm/physical.h"

/* percpu */
tss_t cpu_tss;

/* percpu */
extern uint8_t kernel_stack[PG_SIZE];

void kernel_main(uint64_t magic, uint64_t mbi)
{
  struct multiboot_tag *tag;
  uint16_t selector;

  /* multiboot2 */
  if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
    printf("Invalid magic number: %X\n", (uint32_t)magic);
    return;
  }

  for (tag = (struct multiboot_tag *) (mbi + 8); 
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
                                            + ((tag->size + 7) & ~7))) {
    switch (tag->type) {
      case MULTIBOOT_TAG_TYPE_CMDLINE:
        //printf("CMD: %s\n", ((struct multiboot_tag_string *) tag)->string);
        break;

      case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
        /*printf("mem_lower = %uKB, mem_upper = %uKB\n",
                ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
                ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);*/
        break;

      case MULTIBOOT_TAG_TYPE_MMAP: {
        multiboot_memory_map_t *mmap;
     
        for (mmap = ((struct multiboot_tag_mmap *) tag)->entries;
             (multiboot_uint8_t *) mmap < (multiboot_uint8_t *) tag + tag->size;
             mmap = (multiboot_memory_map_t *) ((unsigned long) mmap 
             + ((struct multiboot_tag_mmap *) tag)->entry_size)) {
          //if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE)
            //physical_free_range(mmap->addr, mmap->len);
          printf("base_addr = 0x%llx, length = 0x%llx, type = 0x%x\n",
                 mmap->addr, mmap->len, mmap->type);
        }
        break;
      }
      case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
        struct multiboot_tag_framebuffer *tagfb
          = (struct multiboot_tag_framebuffer *) tag;
        frameBuf = (uint8_t *) tagfb->common.framebuffer_addr;
     
        switch (tagfb->common.framebuffer_type) {
          case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED: 
            break;
     
          case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
            break;
     
          case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
            break;
        }
        break;
      }
    }
  }

  interrupt_init();

  vm_init();

  acpi_init();

  lapic_init();

  cpu_tss.rsp[0] = ((uint64_t) kernel_stack) + PG_SIZE;
  selector = alloc_tss_desc(&cpu_tss);
  load_tr(selector);

  interrupt_enable();

  while(1);
}
