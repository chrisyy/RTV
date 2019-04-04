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
#include "utils/string.h"
#include "utils/bits.h"
#include "virt/virt.h"
#include "boot_info.h"
#include "mm/malloc.h"
#include "utils/spinlock.h"

uint8_t kernel_stack[PG_SIZE] ALIGNED(PG_SIZE);
boot_info_t vm_config = {.config_size = 0};
spinlock_t mtrr_lock = SPINLOCK_UNLOCKED;
volatile uint8_t mtrr_sync = 0;
volatile bool virt_start = false;

extern uint64_t _boot_start, _boot_pages; 
extern uint64_t _kernel_code_pages, _kernel_ro_pages, _kernel_rw_pages;


void kernel_main(uint64_t magic, uint64_t mbi)
{
  struct multiboot_tag *tag;
  uint16_t selector;
  uint64_t mem_end, mem_limit = 0;
  tss_t *tss_ptr;
  uint32_t i;
  uint64_t base, tmp, end;
  uint8_t *config;

  extern uint16_t my_cs, my_ds;
  extern uint32_t my_cr0, my_cr3, my_cr4;

  printf("cr0 %X, cr3 %X, cr4 %X, cs %X, ds %X\n", my_cr0, my_cr3, my_cr4, my_cs, my_ds);

  /* multiboot2 */
  if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
    printf("Invalid magic number: %X\n", (uint32_t) magic);
    return;
  }
  if (*(uint32_t *) mbi > PG_SIZE) {
    printf("Multiboot struct is too big: %u\n", *(uint32_t *) mbi);
    return;
  }

  for (tag = (struct multiboot_tag *) (mbi + 8); 
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
             + ((tag->size + 7) & ~7))) {
    switch(tag->type) {
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

  physical_set_limit(mem_limit);
  /* mark kernel image */
  physical_take_range((uint64_t) &_boot_start, ((uint64_t) &_boot_pages
                      + (uint64_t) &_kernel_code_pages
                      + (uint64_t) &_kernel_ro_pages
                      + (uint64_t) &_kernel_rw_pages) * PG_SIZE);

  /* mark modules */
  for (tag = (struct multiboot_tag *) (mbi + 8); 
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
             + ((tag->size + 7) & ~7))) {
    if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
      struct multiboot_tag_module *mod = (struct multiboot_tag_module *) tag;

      if (!strncmp(mod->cmdline, "config", sizeof("config"))) {
        vm_config.config_paddr = mod->mod_start;
        vm_config.config_size = mod->mod_end - mod->mod_start;
      } else
        vm_config.num_mod++;

      physical_take_range(mod->mod_start, mod->mod_end - mod->mod_start);
    }
  }

  if (vm_config.config_size == 0)
    panic("Missing config module");

  interrupt_init();

  vm_init(); 

  percpu_init();

  acpi_init();

  lapic_init();

  ioapic_init();

  /* remap video memory */
  frameBuf = (uint8_t *) vm_map_page((uint64_t) frameBuf, PGT_P | PGT_RW | PGT_PCD | PGT_PWT | PGT_XD);

  tss_ptr = percpu_pointer(get_pcpu_id(), cpu_tss);
  tss_ptr->rsp[0] = ((uint64_t) kernel_stack) + PG_SIZE;
  selector = alloc_tss_desc(tss_ptr);
  load_tr(selector);

  malloc_init();

  /* parse config file */
  base = vm_config.config_paddr & PG_MASK;
  end = vm_config.config_paddr + vm_config.config_size;
  tmp = base;
  for (i = 0; tmp < end; i++) 
    tmp += PG_SIZE;
  config = (uint8_t *) vm_map_pages(base, i, PGT_P | PGT_XD);
  if (config == NULL)
    panic("Failed to map config module");
  config += vm_config.config_paddr - base;

  if (vm_config.num_mod == 0)
    panic("Missing VM images");

  parse_boot_info(config, &vm_config);

  /* access mbi before removing identity mapping */
  for (tag = (struct multiboot_tag *) (mbi + 8); 
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
             + ((tag->size + 7) & ~7))) {
    if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
      struct multiboot_tag_module *mod = (struct multiboot_tag_module *) tag;
      uint16_t mod_index;
      for (mod_index = 0; mod_index < vm_config.num_mod; mod_index++) {
        if (strncmp(vm_config.mod_str[mod_index], mod->cmdline, 
            BOOT_STRING_MAX) == 0) {
          vm_config.mod_paddr[mod_index] = mod->mod_start;
          vm_config.mod_size[mod_index] = mod->mod_end - mod->mod_start;
          printf("mod %u: %llX %llX\n", mod_index, vm_config.mod_paddr[mod_index], vm_config.mod_size[mod_index]);
        }
      }
    }
  }

  /* need synchronization after this */
  acpi_sec_init();

  /* remove the first 2MB identity mapping, after smp boot */
  *((uint64_t *) 0xFFFFFFFFC0000000) = 0;
  tlb_flush();

  spin_lock(&mtrr_lock);
  mtrr_sync++;
  spin_unlock(&mtrr_lock);
  /* required synchronization for mtrr update */
  while (mtrr_sync != g_cpus) ;

  mtrr_config();

  spin_lock(&mtrr_lock);
  mtrr_sync--;
  spin_unlock(&mtrr_lock);
  while (mtrr_sync != 0) ;

  //TODO: flush cache

  virt_init(&vm_config);
  virt_start = true;
  virt_percpu_init();

  printf("BSP %u: %u cores\n", get_pcpu_id(), g_cpus);

  interrupt_enable();

  while(1);
}
