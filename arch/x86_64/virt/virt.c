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
#include "cpu.h"
#include "debug.h"
#include "mm/malloc.h"
#include "utils/string.h"
#include "asm_string.h"
#include "acpi.h"
#include "debug.h"
#include "msr.h"
#include "utils/screen.h"
#include "utils/bits.h"
#include "mm/physical.h"
#include "virt/linux.h"

//#define VIRT_DEBUG

vm_struct_t *vm_structs;
uint16_t cpu_to_vm[MAX_CPUS];

static uint64_t vmxon_region[MAX_CPUS];
static uint64_t vmcs_mem_type = 0;
static uint32_t vmcs_rev = 0;

void virt_guest_dump(void)
{
  printf("RIP %llX, RSP %llX, RFLAGS %llX\n", vmread(VMCS_GUEST_RIP),
         vmread(VMCS_GUEST_RSP), vmread(VMCS_GUEST_RFLAGS));
  printf("Base: CS %llX, DS %llX\n", vmread(VMCS_GUEST_CS_BASE),
         vmread(VMCS_GUEST_DS_BASE));
  printf("Limit: CS %X, DS %X\n", vmread(VMCS_GUEST_CS_LMT),
         vmread(VMCS_GUEST_DS_LMT));
}

uint16_t virt_diagnose(void)
{
  uint64_t reason = vmread(VMCS_EXIT_REASON);
  uint64_t qual = vmread(VMCS_EXIT_QUAL);
  uint16_t basic = (uint16_t) reason & 0xFFFF;

  if (get_bit64(reason, 31)) {
    printf("VM-entry failure: reason %u qualification %llX\n",
           basic, qual);
  } else {
#if 1
//#ifdef VIRT_DEBUG
    printf("VM-exit: reason %u qualification %llX\n",
           basic, qual);
    printf("guest address %llX (linear) %llX (physical),\n"
           "interrupt %llX (info) %llX (code),\n"
           "IDT %llX (info) %llX (code),\n"
           "instruction %llX (length) %llX (info)\n",
           vmread(VMCS_LINEAR_ADDR), vmread(VMCS_PHY_ADDR),
           vmread(VMCS_EXIT_INT_INFO), vmread(VMCS_EXIT_INT_CODE),
           vmread(VMCS_EXIT_IDT_INFO), vmread(VMCS_EXIT_IDT_CODE),
           vmread(VMCS_INSTR_LENGTH), vmread(VMCS_INSTR_INFO));

    virt_guest_dump();
#endif
  }

  return basic;
}

static void virt_main(void)
{
  uint64_t flags;
  virt_diagnose();
  panic("virt_main");

  __asm__ volatile("vmresume" : : : "cc", "memory");

  virt_check_error(flags);
  virt_diagnose();
}

static void virt_guest_setup(vm_struct_t *vm)
{
  uint64_t val;

  /* bit-1 reserved */
  vmwrite(VMCS_GUEST_RFLAGS, 0x2);
  vmwrite(VMCS_GUEST_CR3, 0);
  vmwrite(VMCS_GUEST_DR7, 0);

  /* PE, ET */
  val = 0x11;
  val = rdmsr(IA32_VMX_CR0_FIXED0);
  val &= rdmsr(IA32_VMX_CR0_FIXED1);
  /* no PG */
  val &= 0x7FFFFFFF;
  vmwrite(VMCS_GUEST_CR0, val);

  val = rdmsr(IA32_VMX_CR4_FIXED0);
  val &= rdmsr(IA32_VMX_CR4_FIXED1);
  vmwrite(VMCS_GUEST_CR4, val);

  /* accessed bit must be 1, 32-bit code, granularity 1 */
  vmwrite(VMCS_GUEST_CS_SEL, 0x10);
  vmwrite(VMCS_GUEST_CS_BASE, 0);
  vmwrite(VMCS_GUEST_CS_LMT, 0xFFFFFFFF);
  vmwrite(VMCS_GUEST_CS_ACC, 0xC09B);

  /* accessed bit must be 1, granularity 1 */
  vmwrite(VMCS_GUEST_DS_SEL, 0x18);
  vmwrite(VMCS_GUEST_DS_BASE, 0);
  vmwrite(VMCS_GUEST_DS_LMT, 0xFFFFFFFF);
  vmwrite(VMCS_GUEST_DS_ACC, 0xC093);

  /* accessed bit must be 1, granularity 1 */
  vmwrite(VMCS_GUEST_ES_SEL, 0x18);
  vmwrite(VMCS_GUEST_ES_BASE, 0);
  vmwrite(VMCS_GUEST_ES_LMT, 0xFFFFFFFF);
  vmwrite(VMCS_GUEST_ES_ACC, 0xC093);

  /* accessed bit must be 1, granularity 1 */
  vmwrite(VMCS_GUEST_SS_SEL, 0x18);
  vmwrite(VMCS_GUEST_SS_BASE, 0);
  vmwrite(VMCS_GUEST_SS_LMT, 0xFFFFFFFF);
  vmwrite(VMCS_GUEST_SS_ACC, 0xC093);

  /* accessed bit must be 1, granularity 1 */
  vmwrite(VMCS_GUEST_FS_SEL, 0x18);
  vmwrite(VMCS_GUEST_FS_BASE, 0);
  vmwrite(VMCS_GUEST_FS_LMT, 0xFFFFFFFF);
  vmwrite(VMCS_GUEST_FS_ACC, 0xC093);

  /* accessed bit must be 1, granularity 1 */
  vmwrite(VMCS_GUEST_GS_SEL, 0x18);
  vmwrite(VMCS_GUEST_GS_BASE, 0);
  vmwrite(VMCS_GUEST_GS_LMT, 0xFFFFFFFF);
  vmwrite(VMCS_GUEST_GS_ACC, 0xC093);

  /* set to unusable */
  vmwrite(VMCS_GUEST_LDTR_SEL, 0);
  vmwrite(VMCS_GUEST_LDTR_BASE, 0);
  vmwrite(VMCS_GUEST_LDTR_LMT, 0xFFFFF);
  vmwrite(VMCS_GUEST_LDTR_ACC, 0x10082);

  /* TI flag must be 0 */
  vmwrite(VMCS_GUEST_TR_SEL, 0);
  vmwrite(VMCS_GUEST_TR_BASE, 0);
  vmwrite(VMCS_GUEST_TR_LMT, 0xFFFFF);
  vmwrite(VMCS_GUEST_TR_ACC, 0x408B);

  vmwrite(VMCS_GUEST_GDTR_BASE, vm->gdtr);
  vmwrite(VMCS_GUEST_GDTR_LMT, 0xFFFF);
  vmwrite(VMCS_GUEST_IDTR_BASE, 0);
  vmwrite(VMCS_GUEST_IDTR_LMT, 0xFFFF);

  vmwrite(VMCS_GUEST_LP, UINT64_MAX);
  vmwrite(VMCS_GUEST_ACTIVE, 0);
  vmwrite(VMCS_GUEST_INT, 0);
  vmwrite(VMCS_GUEST_DEBUG, 0);
  vmwrite(VMCS_GUEST_SMBASE, 0);
  vmwrite(VMCS_GUEST_SYS_CS, 0);
  vmwrite(VMCS_GUEST_SYS_ESP, 0);
  vmwrite(VMCS_GUEST_SYS_EIP, 0);
  vmwrite(VMCS_GUEST_TIMER, 0);
  vmwrite(VMCS_GUEST_STATUS, 0);
  vmwrite(VMCS_GUEST_PML, 0);
  vmwrite(VMCS_GUEST_PDPTE0, 0);
  vmwrite(VMCS_GUEST_PDPTE1, 0);
  vmwrite(VMCS_GUEST_PDPTE2, 0);
  vmwrite(VMCS_GUEST_PDPTE3, 0);
  vmwrite(VMCS_GUEST_DEBUGCTL, 0);
  vmwrite(VMCS_GUEST_PERF, 0);
  vmwrite(VMCS_GUEST_BNDCFGS, 0);
  vmwrite(VMCS_GUEST_EFER, 0);
  vmwrite(VMCS_GUEST_PAT, rdmsr(IA32_PAT));
  vmwrite(VMCS_GUEST_RSP, 0);

  vmwrite(VMCS_GUEST_RIP, vm->entry_point);

#ifdef VIRT_DEBUG
  printf("Guest: link pointer %llX, PAT %llX, RIP %llX\n",
         vmread(VMCS_GUEST_LP), vmread(VMCS_GUEST_PAT),
         vmread(VMCS_GUEST_RIP));
  printf("Guest: CR0 %llX (fixed0 %llX, fixed1 %llX)\n",
         vmread(VMCS_GUEST_CR0), rdmsr(IA32_VMX_CR0_FIXED0),
         rdmsr(IA32_VMX_CR0_FIXED1));
  printf("Guest: CR4 %llX (fixed0 %llX, fixed1 %llX)\n",
         vmread(VMCS_GUEST_CR4), rdmsr(IA32_VMX_CR4_FIXED0),
         rdmsr(IA32_VMX_CR4_FIXED1));
#endif
}

static void virt_host_setup(void)
{
  uint64_t reg;
  uint16_t sel;
  uint64_t base;
  uint64_t stack;
  gdt_desc gdtr;
  idt_desc idtr;

  __asm__ volatile("movq %%cr0, %0" : "=r" (reg));
  vmwrite(VMCS_HOST_CR0, reg);
  __asm__ volatile("movq %%cr3, %0" : "=r" (reg));
  vmwrite(VMCS_HOST_CR3, reg);
  __asm__ volatile("movq %%cr4, %0" : "=r" (reg));
  vmwrite(VMCS_HOST_CR4, reg);

  __asm__ volatile("movw %%cs, %%ax\n"
                   "movw %%ax, %0" : "=r" (sel));
  vmwrite(VMCS_HOST_CS_SEL, sel);
  __asm__ volatile("movw %%ds, %%ax\n"
                   "movw %%ax, %0" : "=r" (sel));
  vmwrite(VMCS_HOST_DS_SEL, sel);
  __asm__ volatile("movw %%es, %%ax\n"
                   "movw %%ax, %0" : "=r" (sel));
  vmwrite(VMCS_HOST_ES_SEL, sel);
  __asm__ volatile("movw %%ss, %%ax\n"
                   "movw %%ax, %0" : "=r" (sel));
  vmwrite(VMCS_HOST_SS_SEL, sel);

  __asm__ volatile("movw %%fs, %%ax\n"
                   "movw %%ax, %0" : "=r" (sel));
  vmwrite(VMCS_HOST_FS_SEL, sel);
  base = rdmsr(IA32_FS_BASE);
  vmwrite(VMCS_HOST_FS_BASE, base);

  __asm__ volatile("movw %%gs, %%ax\n"
                   "movw %%ax, %0" : "=r" (sel));
  vmwrite(VMCS_HOST_GS_SEL, sel);
  base = rdmsr(IA32_GS_BASE);
  vmwrite(VMCS_HOST_GS_BASE, base);

  __asm__ volatile("str %0" : "=r" (sel));
  vmwrite(VMCS_HOST_TR_SEL, sel);
  base = get_gdt_tss_base(sel);
  vmwrite(VMCS_HOST_TR_BASE, base);

  __asm__ volatile("sgdt %0" : "=m" (gdtr));
  vmwrite(VMCS_HOST_GDTR_BASE, gdtr.base);

  __asm__ volatile("sidt %0" : "=m" (idtr));
  vmwrite(VMCS_HOST_IDTR_BASE, idtr.base);

  vmwrite(VMCS_HOST_EFER, rdmsr(IA32_EFER));
  vmwrite(VMCS_HOST_PAT, rdmsr(IA32_PAT));

  __asm__ volatile("movq %%rsp, %0" : "=r" (stack));
  stack = (stack & PG_MASK) + PG_SIZE;
  vmwrite(VMCS_HOST_RSP, stack);
  vmwrite(VMCS_HOST_RIP, (uint64_t) virt_main);

#ifdef VIRT_DEBUG
  printf("Host: CR0 %llX, CR3 %llX, CR4 %llX\n", 
         vmread(VMCS_HOST_CR0), vmread(VMCS_HOST_CR3), 
         vmread(VMCS_HOST_CR4));
  printf("Host: FS base %llX, GS base %llX, TR base %llX\n", 
         vmread(VMCS_HOST_FS_BASE), vmread(VMCS_HOST_GS_BASE),
         vmread(VMCS_HOST_TR_BASE));
  printf("Host: EFER %llX, PAT %llX\n", vmread(VMCS_HOST_EFER),
         vmread(VMCS_HOST_PAT));
  printf("Host: RSP %llX, RIP %llX\n", vmread(VMCS_HOST_RSP), 
         vmread(VMCS_HOST_RIP));
#endif
}

static void virt_control_setup(uint64_t ept_p)
{
  uint64_t pin_msr, proc_msr, proc2_msr, exit_msr, entry_msr;

  if (get_bit64(rdmsr(IA32_VMX_BASIC), 55)) {
    pin_msr = rdmsr(IA32_VMX_TRUE_PINBASED_CTLS);
    proc_msr = rdmsr(IA32_VMX_TRUE_PROCBASED_CTLS);

    exit_msr = rdmsr(IA32_VMX_TRUE_EXIT_CTLS);
    entry_msr = rdmsr(IA32_VMX_TRUE_ENTRY_CTLS);
  } else {
    pin_msr = rdmsr(IA32_VMX_PINBASED_CTLS);
    proc_msr = rdmsr(IA32_VMX_PROCBASED_CTLS);

    exit_msr = rdmsr(IA32_VMX_EXIT_CTLS);
    entry_msr = rdmsr(IA32_VMX_ENTRY_CTLS);
  }

  /* load/save IA32_EFER, host address-space size */
  if (!get_bit64(exit_msr, 52) || !get_bit64(exit_msr, 53))
    panic("Load/save IA32_EFER on VM exit not supported");
  if (!get_bit64(exit_msr, 41))
    panic("Host address-space size on VM exit not supported");
  vmwrite(VMCS_CTRL_EXIT, exit_msr | 0x300200);
  vmwrite(VMCS_CTRL_EXIT_SCNT, 0);
  vmwrite(VMCS_CTRL_EXIT_LCNT, 0);

  /* first time entry in 32-bit mode, load IA32_EFER/IA32_PAT */
  if (get_bit64(entry_msr, 9))
    panic("32-bit VM entry not supported");
  if (!get_bit64(entry_msr, 47))
    panic("Load IA32_EFER not supported");
  if (!get_bit64(entry_msr, 46))
    panic("Load IA32_PAT not supported");
  vmwrite(VMCS_CTRL_ENTRY, entry_msr | 0xC000);
  vmwrite(VMCS_CTRL_ENTRY_LCNT, 0);
  vmwrite(VMCS_CTRL_ENTRY_INT, 0);

  vmwrite(VMCS_CTRL_PIN, pin_msr);

  if (!get_bit64(proc_msr, 63))
    panic("Secondary controls is not supported");
  proc_msr |= ((uint64_t) 1) << 31;
  vmwrite(VMCS_CTRL_PPROC, proc_msr);

  /* enable EPT, unrestricted guest */
  proc2_msr = rdmsr(IA32_VMX_PROCBASED_CTLS2);
  if (!get_bit64(proc2_msr, 33))
    panic("EPT not supported");
  if (!get_bit64(proc2_msr, 39))
    panic("Unrestricted guest not supported");
  vmwrite(VMCS_CTRL_SPROC, proc2_msr | 0x82);

  vmwrite(VMCS_CTRL_EXP_MAP, 0);
  vmwrite(VMCS_CTRL_IOMAP_A, 0);
  vmwrite(VMCS_CTRL_IOMAP_B, 0);
  vmwrite(VMCS_CTRL_CR0_MASK, 0);
  vmwrite(VMCS_CTRL_CR0_RD, 0);
  vmwrite(VMCS_CTRL_CR3_CNT, 0);
  vmwrite(VMCS_CTRL_PF_MASK, 0);
  vmwrite(VMCS_CTRL_PF_MATCH, 0);

  /* host controlled VMXE */
  vmwrite(VMCS_CTRL_CR4_MASK, 0x2000);
  vmwrite(VMCS_CTRL_CR4_RD, 0);

  /* write-back */
  vmwrite(VMCS_CTRL_EPTP, ept_p | 0x6 | (3 << 3));

  //TODO enable VPID for performance

#ifdef VIRT_DEBUG
  printf("Control: pin %llX, proc %llX, proc2 %llX, exit %llX, entry %llX\n",
         vmread(VMCS_CTRL_PIN), vmread(VMCS_CTRL_PPROC),
         vmread(VMCS_CTRL_SPROC), vmread(VMCS_CTRL_EXIT), 
         vmread(VMCS_CTRL_ENTRY));
#endif
}

void virt_init(boot_info_t *info)
{
  uint16_t i, j, cur_cpu = 0;
  uint64_t msr;
  uint32_t ecx;

  /* check VMX capability */
  cpuid(1, 0, NULL, NULL, &ecx, NULL);
  if (get_bit64((uint64_t) ecx, 5) == 0)
    panic("VMX not supported");

  msr = rdmsr(IA32_VMX_BASIC);
  vmcs_rev = (uint32_t) msr;
  if (get_bit64(msr, 44) == 0)
    panic("VMXON/VMCS size not 4KB");
  vmcs_mem_type = (msr >> 50) & 0xF;
  if (vmcs_mem_type == 0)
    vmcs_mem_type = PGT_PCD;
  else
    vmcs_mem_type = 0;

  for (i = 0; i < MAX_CPUS; i++)
    cpu_to_vm[i] = VM_NONE;

  //TODO find vmlinuz and initrd
  //hardcoded 1 vmlinuz and 1 initrd for now
  info->num_mod--;

  vm_structs = (vm_struct_t *) malloc(info->num_mod * sizeof(vm_struct_t));
  if (vm_structs == NULL)
    panic("Failed malloc for vm_structs");

  for (i = 0; i < info->num_mod; i++) {
    memcpy(vm_structs[i].name, info->mod_str[i], strlen(info->mod_str[i]) + 1);
    vm_structs[i].vm_id = i;
    vm_structs[i].img_paddr = info->mod_paddr[i];
    vm_structs[i].img_size = info->mod_size[i];
    vm_structs[i].num_cpus = 0;
    vm_structs[i].ram_size = info->ram_size[i];
    spin_lock_init(&vm_structs[i].lock);

    if (cur_cpu + info->num_cpus[i] > g_cpus)
      panic("Number of VM CPUs exceeds the available amount");
    for (j = 0; j < info->num_cpus[i]; j++)
      cpu_to_vm[cur_cpu++] = i;

    //TODO find vmlinuz and initrd
    //hardcoded 1 vmlinuz and 1 initrd for now
    vm_structs[i].entry_point = LINUX_ENTRY_POINT;
    i++;
    vm_structs[i - 1].extra_paddr = info->mod_paddr[i];
    vm_structs[i - 1].extra_size = info->mod_size[i];
  }
}

void virt_percpu_init(void)
{
  uint16_t cpu = get_pcpu_id();
  uint16_t vm_id = cpu_to_vm[cpu];
  vm_struct_t *vm;
  uint16_t vcpu_id;
  uint64_t cr0, cr4, msr;
  uint32_t *vmxon_addr;
  uint32_t *vmcs_addr;
  uint64_t ept_p;
  uint64_t flags;

  if (vm_id == VM_NONE)
    return;

  vm = &vm_structs[vm_id];

  /* VMX fixed bits in CR0 */
  __asm__ volatile("movq %%cr0, %0\n" : "=r" (cr0));
  cr0 |= rdmsr(IA32_VMX_CR0_FIXED0);
  cr0 &= rdmsr(IA32_VMX_CR0_FIXED1);
  __asm__ volatile("movq %0, %%cr0\n" : : "r" (cr0));

  /* VMX fixed bits in CR4 */
  __asm__ volatile("movq %%cr4, %0\n" : "=r" (cr4));
  cr4 |= rdmsr(IA32_VMX_CR4_FIXED0);
  cr4 &= rdmsr(IA32_VMX_CR4_FIXED1);
  __asm__ volatile("movq %0, %%cr4\n" : : "r" (cr4));

  /* 
   * per-CPU MSR
   * set lock bit and enable VMX outside SMX operation 
   */
  msr = rdmsr(IA32_FEATURE_CONTROL);
  if (get_bit64(msr, 0) && !get_bit64(msr, 2))
    panic("VMX is disabled");
  msr |= 1 | (1 << 2);
  wrmsr(IA32_FEATURE_CONTROL, msr);

  /* VMXON region */
  vmxon_region[cpu] = alloc_phys_frame();
  if (vmxon_region[cpu] == 0)
    panic("VMXON region allocation failed");
  vmxon_addr = (uint32_t *) vm_map_page(vmxon_region[cpu], 
                                        PGT_P | PGT_RW | vmcs_mem_type);
  if (vmxon_addr == NULL)
    panic("VMXON region mapping failed");
  *vmxon_addr = vmcs_rev;
  vm_unmap_page(vmxon_addr);

  vmxon(vmxon_region[cpu]);
  virt_check_error(flags);

  spin_lock(&vm->lock);
  vcpu_id = vm->num_cpus++;
  spin_unlock(&vm->lock);

  /* create VMCS */
  vm->vmcs_paddr[vcpu_id] = alloc_phys_frame();
  if (vm->vmcs_paddr[vcpu_id] == 0)
    panic("VMCS allocation failed");
  vmcs_addr = (uint32_t *) vm_map_page(vm->vmcs_paddr[vcpu_id], 
                                       PGT_P | PGT_RW | vmcs_mem_type);
  if (vmcs_addr == NULL)
    panic("VMCS mapping failed");
  vmcs_addr[0] = vmcs_rev;
  vmcs_addr[1] = 0;
  vm_unmap_page(vmcs_addr);

  vmclear(vm->vmcs_paddr[vcpu_id]);
  vmptrld(vm->vmcs_paddr[vcpu_id]);
  virt_check_error(flags);

  ept_p = virt_pg_table_setup(vm);

  virt_guest_setup(vm);

  virt_control_setup(ept_p);

  virt_host_setup();

  __asm__ volatile("vmlaunch" : : : "cc", "memory");

  virt_check_error(flags);
  virt_diagnose();
}
