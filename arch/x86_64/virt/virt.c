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
#include "msr.h"
#include "utils/screen.h"
#include "mm/physical.h"

vm_struct_t *vm_structs;
uint16_t cpu_to_vm[MAX_CPUS];

static uint64_t vmxon_region[MAX_CPUS];

void virt_init(boot_info_t *info)
{
  uint16_t i, j, cur_cpu = 0;

  for (i = 0; i < MAX_CPUS; i++)
    cpu_to_vm[i] = VM_NONE;

  vm_structs = (vm_struct_t *) malloc(info->num_mod * sizeof(vm_struct_t));
  if (vm_structs == NULL)
    panic("Failed malloc for vm_structs");

  for (i = 0; i < info->num_mod; i++) {
    memcpy(vm_structs[i].name, info->mod_str[i], strlen(info->mod_str[i]) + 1);
    vm_structs[i].vm_id = i;
    vm_structs[i].img_paddr = info->mod_paddr[i];
    for (j = 0; j < MAX_CPUS; j++)
      vm_structs[i].vcpu_map[j] = VCPU_NONE;
    vm_structs[i].num_cpus = 0;
    spin_lock_init(&vm_structs[i].lock);

    if (cur_cpu + info->num_cpus[i] > g_cpus)
      panic("Number of VM CPUs exceeds the available amount");
    for (j = 0; j < info->num_cpus[i]; j++)
      cpu_to_vm[cur_cpu++] = i;
  }
}

void virt_check_error(void)
{
  uint64_t flags;

  __asm__ volatile("pushfq\n"
                   "pop %0" : "=r" (flags));

  if (flags & FLAGS_CF)
    panic("Invalid VMCS pointer");
  else if (flags & FLAGS_ZF) {
    printf("Error number: %u\n", vmread(VMCS_INSTR_ERROR));
    panic("VMX error");
  }
}

void virt_percpu_init(void)
{
  uint16_t cpu = get_pcpu_id();
  uint16_t vm_id = cpu_to_vm[cpu];
  vm_struct_t *vm;
  uint64_t cr0, cr4, msr;
  uint32_t *vmxon_addr;

  if (vm_id == VM_NONE)
    return;

  vm = &vm_structs[vm_id];

  /* VMX fixed bits in CR0 */
  __asm__ volatile("movq %%cr0, %0\n" : "=r" (cr0));
  cr0 |= rdmsr(IA32_VMX_CR0_FIXED0);
  cr0 &= rdmsr(IA32_VMX_CR0_FIXED1);
  __asm__ volatile("movq %0, %%cr0\n" : : "r" (cr0));
  //printf("cr0: %llX\n", cr0);

  /* VMX fixed bits in CR4 */
  __asm__ volatile("movq %%cr4, %0\n" : "=r" (cr4));
  cr4 |= rdmsr(IA32_VMX_CR4_FIXED0);
  cr4 &= rdmsr(IA32_VMX_CR4_FIXED1);
  __asm__ volatile("movq %0, %%cr4\n" : : "r" (cr4));
  //printf("cr4: %llX\n", cr4);

  /* set lock bit and enable VMX outside SMX operation */
  msr = rdmsr(IA32_FEATURE_CONTROL);
  msr |= 1 | (1 << 2);
  wrmsr(IA32_FEATURE_CONTROL, msr);
  //printf("feature: %llX\n", rdmsr(IA32_FEATURE_CONTROL));

  vmxon_region[cpu] = alloc_phys_frame();
  if (vmxon_region[cpu] == 0)
    panic("VMXON region allocation failed");
  vmxon_addr = (uint32_t *) vm_map_page(vmxon_region[cpu], PGT_P | PGT_RW);
  if (vmxon_addr == NULL)
    panic("VMXON region mapping failed");
  *vmxon_addr = (uint32_t) rdmsr(IA32_VMX_BASIC);
  vm_unmap_page(vmxon_addr);

  __asm__ volatile("vmxon %0" : : "m" (vmxon_region[cpu]) : "cc");

  virt_check_error();

  spin_lock(&vm->lock);

  vm->vcpu_map[cpu] = vm->num_cpus++;

  spin_unlock(&vm->lock);
}
