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

#include "boot_info.h"
#include "mm/malloc.h"
#include "debug.h"
#include "vm.h"
#include "asm_string.h"
#include "utils/string.h"

//TODO: add string processing support
void parse_boot_info(boot_info_t *info)
{
  uint8_t *addr;
  uint64_t len;
  uint8_t pages;

  if (info->config_paddr & (~PG_MASK))
    panic("Config module not aligned to 4KB");
  len = info->config_size;
  if (len % PG_SIZE == 0)
    pages = len >> PG_BITS;
  else
    pages = (len >> PG_BITS) + 1;

  addr = (uint8_t *) vm_map_pages(info->config_paddr, pages, PGT_P | PGT_XD);
  if (addr == NULL)
    panic("Failed to map config module");

  //TODO parse config here

  if (info->num_mod == 0)
    panic("Missing VM images");

  info->mod_str = (char **) malloc(sizeof(char *) * info->num_mod);
  if (info->mod_str == NULL)
    panic("Failed malloc for mod_str");

  info->mod_paddr = (uint64_t *) malloc(sizeof(uint64_t) * info->num_mod);
  if (info->mod_paddr == NULL)
    panic("Failed malloc for mod_paddr");

  info->mod_size = (uint64_t *) malloc(sizeof(uint64_t) * info->num_mod);
  if (info->mod_size == NULL)
    panic("Failed malloc for mod_size");

  info->num_cpus = (uint16_t *) malloc(sizeof(uint16_t) * info->num_mod);
  if (info->num_cpus == NULL)
    panic("Failed malloc for num_cpus");

  info->ram_size = (uint32_t *) malloc(sizeof(uint32_t) * info->num_mod);
  if (info->ram_size == NULL)
    panic("Failed malloc for ram_size");

  //TODO info->num_cpus[i], info->mod_str[i] (check max length)
  //check if match num_mod, if not, panic
  //hardcoded 1 vmlinuz and 1 initrd for now
  info->mod_str[0] = (char *) malloc(sizeof(char) * BOOT_STRING_MAX);
  memcpy(info->mod_str[0], "linux", sizeof("linux"));
  //TODO: hardcoded 1 cpu, 2MB ram for test
  info->num_cpus[0] = 1;
  info->ram_size[0] = 2;

  info->mod_str[1] = (char *) malloc(sizeof(char) * BOOT_STRING_MAX);
  memcpy(info->mod_str[1], "initrd", sizeof("initrd"));

  vm_unmap_pages(addr, pages);
}
