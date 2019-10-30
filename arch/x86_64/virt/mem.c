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
#include "utils/math.h"


/*
 * most memory regions are identity-mapped with large pages;
 * kernel is not identity-mapped and is mapped with 4KB pages
 */
static void virt_setup_e820(boot_params_t *zero_page, uint64_t kernel_size)
{
  uint64_t mmap_start = 0, mmap_size = 0;
  uint8_t mmap_num = 0;
  uint64_t start_paddr;
  struct boot_e820_entry *table = zero_page->e820_table;
  bool merge_ramdisk = true;
  uint64_t ramdisk_end = zero_page->hdr.ramdisk_image
                         + zero_page->hdr.ramdisk_size;
  uint64_t kernel_end = LINUX_ENTRY_POINT + kernel_size;

  while (true) {
    start_paddr = mmap_start + mmap_size;
    physical_next_free_range(start_paddr, &mmap_start, &mmap_size);

    /* add ramdisk to linux memory */
    if (mmap_start >= ramdisk_end && merge_ramdisk) {
      table[mmap_num].addr = zero_page->hdr.ramdisk_image;
      table[mmap_num].size = ceiling64(zero_page->hdr.ramdisk_size,
                                       LARGE_PG_SIZE);
      table[mmap_num].type = E820_RAM;
      printf("mmap: %llX, %llX\n", table[mmap_num].addr, table[mmap_num].size);
      mmap_num++;
      merge_ramdisk = false;
    }

    if (mmap_size == 0)
      break;

    if (mmap_start >= kernel_end) {
      uint64_t old_addr, tmp;

      /* align to large page size */
      old_addr = mmap_start;
      mmap_start = ceiling64(mmap_start, LARGE_PG_SIZE);
      /* skip memory range smaller than large page size */
      tmp = mmap_start - old_addr;
      if (tmp >= mmap_size) {
        mmap_size = 0;
        continue;
      }
      mmap_size -= tmp;
      tmp = mmap_size & LARGE_PG_MASK;
      if (tmp < LARGE_PG_SIZE)
        continue;
      mmap_size = tmp;
    } else if (mmap_start >= LINUX_ENTRY_POINT && mmap_start < kernel_end) {
      /* for kernel mapping */
      continue;
    } else {
      /* below LINUX_ENTRY_POINT */
      if (start_paddr == 0) {
        /*
         * add zero_frame and cmd_frame (from
         * virt_setup_linux) to linux memory
         */
        mmap_start -= 2 << PG_BITS;
        mmap_size += 2 << PG_BITS;
      }
    }

    table[mmap_num].addr = mmap_start;
    table[mmap_num].size = mmap_size;
    table[mmap_num].type = E820_RAM;
    mmap_num++;

    /* mark unavailable to hypervisor */
    physical_take_range(mmap_start, mmap_size);
    printf("mmap: %llX, %llX\n", mmap_start, mmap_size);
  }

  //TODO: add ACPI memory ranges to table
}

static void virt_setup_linux(vm_struct_t *vm)
{
  uint64_t zero_frame, new_frames, cmd_frame;
  uint8_t *linux_virt, *cmd_virt;
  uint64_t *gdt;
  boot_params_t *zero_virt;
  setup_header_t *setup;
  uint64_t start_paddr, frame;
  uint8_t *src_vaddr, *dst_vaddr;
  uint64_t kernel_size;
  uint8_t src_pages, dst_pages;

  /* initialize zeropage below 1MB for identity mapping */
  zero_frame = alloc_phys_frame_lowmem();
  if (zero_frame == 0)
    panic("page allocation for zero_frame failed");
  zero_virt = (boot_params_t *) vm_map_page(zero_frame, PGT_P | PGT_RW);
  if (zero_virt == NULL)
    panic("page mapping for zero_frame failed");
  memset(zero_virt, 0, PG_SIZE);

  /* load linux setup header */
  if (vm->img_paddr & (~PG_MASK))
    panic("Image address not aligned to 4KB");
  linux_virt = (uint8_t *) vm_map_page(vm->img_paddr, PGT_P | PGT_RW);
  if (linux_virt == NULL)
    panic("page mapping for header failed");
  setup = (setup_header_t *) (linux_virt + LINUX_HEADER_OFFSET);
  if (setup->header != LINUX_HEADER_MAGIC)
    panic("Image magic number does not match");
  memcpy(&(zero_virt->hdr), setup, sizeof(setup_header_t));
  vm_unmap_page(linux_virt);

  /*
   * relocate kernel to make sure it's 4KB page-aligned
   * and located before ramdisk in memory
   */
  start_paddr = vm->img_paddr + (zero_virt->hdr.setup_sects + 1)
                * LINUX_SECTOR_SZ;
  frame = start_paddr & LARGE_PG_MASK;
  kernel_size = vm->img_size - (zero_virt->hdr.setup_sects + 1)
                * LINUX_SECTOR_SZ;
  src_pages = ceiling64(start_paddr - frame + kernel_size, LARGE_PG_SIZE)
              >> LARGE_PG_BITS;
  src_vaddr = (uint8_t *) vm_map_large_pages(frame, src_pages, PGT_P | PDT_PS);
  if (src_vaddr == NULL)
    panic("page mapping for src kernel failed");
  src_vaddr += start_paddr - frame;

  dst_pages = ceiling64(kernel_size, LARGE_PG_SIZE) >> LARGE_PG_BITS;
  new_frames = alloc_phys_frames_aligned(dst_pages << (LARGE_PG_BITS - PG_BITS),
                                         LARGE_PG_SIZE);
  if (new_frames == 0)
    panic("page allocation for dst kernel failed");
  dst_vaddr = (uint8_t *) vm_map_large_pages(new_frames, dst_pages,
                                             PGT_P | PGT_RW | PDT_PS);
  if (dst_vaddr == NULL)
    panic("page mapping for dst kernel failed");

  //TODO: why no need for RW?
  vm_check_mapping(dst_vaddr);

  memcpy(dst_vaddr, src_vaddr, kernel_size);

  /* free the original kernel */
  vm_unmap_large_pages(src_vaddr - (start_paddr - frame), src_pages);
  physical_free_range(vm->img_paddr, vm->img_size);

  vm_unmap_large_pages(dst_vaddr, dst_pages);

  vm->img_paddr = new_frames;
  vm->img_size = kernel_size;

  /*
   * first half page is for gdt, second half is for
   * linux boot argument, identity mapping
   */
  cmd_frame = alloc_phys_frame_lowmem();
  gdt = (uint64_t *) vm_map_page(cmd_frame, PGT_P | PGT_RW);
  cmd_virt = (uint8_t *) gdt;
  gdt[0] = 0;
  gdt[1] = 0;
  gdt[2] = 0xCF9A000000FFFF;  /* 32-bit, 4GB, exec+read CS */
  gdt[3] = 0xCF92000000FFFF;  /* 32-bit, 4GB, r/w DS */
  vm->gdtr = cmd_frame;

  //TODO move cmdline arguments to grub.cfg
  cmd_virt += PG_SIZE >> 1;
  memcpy(cmd_virt, LINUX_CMD_LINE, sizeof(LINUX_CMD_LINE));
  vm_unmap_page(gdt);
  zero_virt->hdr.cmd_line_ptr = cmd_frame + (PG_SIZE >> 1);

  zero_virt->hdr.type_of_loader = 0xFF;
  /* print early msg, reload segment registers, no heap */
  zero_virt->hdr.loadflags &= ~0xE0;
  zero_virt->hdr.code32_start = vm->entry_point;

  /* relocate ramdisk to make large page alignment */
  frame = vm->extra_paddr & LARGE_PG_MASK;
  src_pages = ceiling64(vm->extra_size + vm->extra_paddr - frame,
                        LARGE_PG_SIZE) >> LARGE_PG_BITS;
  src_vaddr = (uint8_t *) vm_map_large_pages(frame, src_pages, PGT_P | PDT_PS);
  if (src_vaddr == NULL)
    panic("page mapping for src ramdisk failed");
  src_vaddr += vm->extra_paddr - frame;

  dst_pages = ceiling64(vm->extra_size, LARGE_PG_SIZE) >> LARGE_PG_BITS;
  new_frames = alloc_phys_frames_aligned(dst_pages << (LARGE_PG_BITS - PG_BITS),
                                         LARGE_PG_SIZE);
  if (new_frames == 0)
    panic("page allocation for dst ramdisk failed");
  dst_vaddr = (uint8_t *) vm_map_large_pages(new_frames, dst_pages, PGT_P | PGT_RW | PDT_PS);
  if (dst_vaddr == NULL)
    panic("page mapping for dst ramdisk failed");

  memcpy(dst_vaddr, src_vaddr, vm->extra_size);

  /* free the original ramdisk */
  vm_unmap_large_pages(src_vaddr - (vm->extra_paddr - frame), src_pages);
  physical_free_range(vm->extra_paddr, vm->extra_size);

  vm->extra_paddr = new_frames;

  /* ramdisk is identity-mapped to linux */
  zero_virt->hdr.ramdisk_image = vm->extra_paddr;
  zero_virt->hdr.ramdisk_size = vm->extra_size;

  virt_setup_e820(zero_virt, kernel_size);

  vm_unmap_page(zero_virt);
  vm->input = zero_frame;
}

/*
 * TODO: rewrite this
 * returns the physical address of the EPT base
 */
uint64_t virt_pg_table_setup(vm_struct_t *vm)
{
  uint64_t pml4t_phy, pdpt_phy;
  uint64_t *pml4t_virt, *pdpt_virt;
  uint32_t pdt_entries = vm->ram_size >> 1;
  uint32_t pdpt_entries = (pdt_entries + (PG_TABLE_ENTRIES - 1)) >> PG_TABLE_BITS;
  uint32_t count = 0, bound = pdt_entries * PG_TABLE_ENTRIES;
  uint32_t i, j;
  //TODO fix this, start from img_paddr
  uint64_t kernel_phy = vm->img_paddr + 0x100000;
  uint64_t ramdisk_phy = vm->extra_paddr;

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

  virt_setup_linux(vm);

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
            pt_virt[k] = (frame_offset + 0x100000000) | EPT_RD
                         | EPT_WR | EPT_EX | EPT_TP(EPT_TYPE_WB);

          //TODO: 32-bit test starts at 1MB
          if (k == PG_TABLE_ENTRIES / 2) {
            uint16_t *test_addr = (uint16_t *) vm_map_page(frame_offset
                                  + 0x100000000, PGT_P | PGT_RW);

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
      } else if (kernel_phy < (vm->img_size + vm->img_paddr)) {
        /* map kernel */
        pdt_virt[j] = kernel_phy | EPT_RD | EPT_WR | EPT_EX | EPT_PG
                      | EPT_TP(EPT_TYPE_WB);
        kernel_phy += LARGE_PG_SIZE;
      } else if (ramdisk_phy < (vm->extra_size + vm->extra_paddr)) {
        /* map ramdisk right after kernel */
        pdt_virt[j] = ramdisk_phy | EPT_RD | EPT_WR | EPT_EX | EPT_PG
                      | EPT_TP(EPT_TYPE_WB);
        ramdisk_phy += LARGE_PG_SIZE;
      } else {
        pdt_virt[j] = (frame_offset + 0x100000000) | EPT_RD | EPT_WR
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
