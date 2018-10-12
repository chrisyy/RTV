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
#include "percpu.h"
#include "debug.h"
#include "mm/malloc.h"
#include "utils/string.h"
#include "asm_string.h"
#include "acpi.h"
#include "debug.h"

vm_struct_t *vm_structs;
uint16_t cpu_to_vm[MAX_CPUS];

void virt_init(boot_info_t *info)
{
  uint16_t i, j, cur_cpu = 0;

  vm_structs = (vm_struct_t *) malloc(info->num_mod * sizeof(vm_struct_t));
  if (vm_structs == NULL)
    panic("Failed malloc for vm_structs");

  for (i = 0; i < info->num_mod; i++) {
    memcpy(vm_structs[i].name, info->mod_str[i], strlen(info->mod_str[i]) + 1);
    vm_structs[i].vm_id = i;
    vm_structs[i].img_paddr = info->mod_paddr[i];

    if (cur_cpu + info->num_cpus[i] > g_cpus)
      panic("number of VM CPUs exceeds the available amount");
    for (j = 0; j < info->num_cpus[i]; j++)
      cpu_to_vm[cur_cpu++] = i;
  }
}
