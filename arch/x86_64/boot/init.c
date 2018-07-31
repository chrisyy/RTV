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
#include "percpu.h"
#include "smp.h"
#include "utils/spinlock.h"

uint8_t kernel_stack[PG_SIZE];

extern uint64_t _boot_start, _boot_pages; 
extern uint64_t _kernel_code_pages, _kernel_ro_pages, _kernel_rw_pages;
extern uint8_t ap_boot_start[], ap_boot_end[];


void kernel_main(uint64_t magic, uint64_t mbi)
{
  struct multiboot_tag *tag;
  uint16_t selector;
  uint64_t mem_end, mem_limit = 0;
  tss_t *tss_ptr;

  /* multiboot2 */
  if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
    printf("Invalid magic number: %X\n", (uint32_t)magic);
    return;
  }

  for (tag = (struct multiboot_tag *) (mbi + 8); 
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
                                            + ((tag->size + 7) & ~7))) {
    switch(tag->type) {
    case MULTIBOOT_TAG_TYPE_CMDLINE:
      //printf("CMD: %s\n", ((struct multiboot_tag_string *) tag)->string);
      break;
    case MULTIBOOT_TAG_TYPE_MMAP: {
      multiboot_memory_map_t *mmap;
    
      for (mmap = ((struct multiboot_tag_mmap *) tag)->entries;
           (multiboot_uint8_t *) mmap < (multiboot_uint8_t *) tag + tag->size;
           mmap = (multiboot_memory_map_t *) ((unsigned long) mmap 
           + ((struct multiboot_tag_mmap *) tag)->entry_size)) {
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
          physical_free_range(mmap->addr, mmap->len);
          mem_end = mmap->addr + mmap->len;
          if (mem_end > mem_limit)
            mem_limit = mem_end;
        }
        //printf("base_addr = 0x%llx, length = 0x%llx, type = 0x%x\n",
        //       mmap->addr, mmap->len, mmap->type);
      }

      break;
    }
    case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
      struct multiboot_tag_framebuffer *tagfb
        = (struct multiboot_tag_framebuffer *) tag;
      frameBuf = (uint8_t *) tagfb->common.framebuffer_addr;
      break;
    }
    }
  }

  //TODO: check if SMP_BOOT_ADDR is free memory

  physical_take_range((uint64_t) &_boot_start, ((uint64_t) &_boot_pages
                      + (uint64_t) &_kernel_code_pages
                      + (uint64_t) &_kernel_ro_pages
                      + (uint64_t) &_kernel_rw_pages) * PG_SIZE);
  physical_set_limit(mem_limit);

  interrupt_init();


  vm_init(); 

  percpu_init();

  acpi_init();

  lapic_init();

  ioapic_init();

  /* remap video memory */
  frameBuf = (uint8_t *) vm_map_page((uint64_t) frameBuf, PGT_P | PGT_RW | PGT_PCD | PGT_PWT | PGT_XD);

  acpi_sec_init();

  /* remove the first 2MB identity mapping (Recursive Mapping) */
  *((uint64_t *) 0xFFFFFFFFC0000000) = 0;
  tlb_flush();

  printf("BSP %u: %u cores\n", get_pcpu_id(), g_cpus);

  tss_ptr = percpu_pointer(get_pcpu_id(), cpu_tss);
  tss_ptr->rsp[0] = ((uint64_t) kernel_stack) + PG_SIZE;
  selector = alloc_tss_desc(tss_ptr);
  load_tr(selector);

  interrupt_enable();

  while(1);
}
