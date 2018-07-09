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

#include "msr.h"
#include "cpu.h"
#include "smp.h"

  .section .text
  .code16

  .global ap_boot_start
  .global ap_boot_end
  .global status_code
  .global ap_stack_ptr
ap_boot_start:
  jmp 1f

status_code:    
  .quad 0       /* status indicator */
ap_stack_ptr:
  .quad 0       /* C stack for AP */

1:
  movw %cs, %dx
  movw %dx, %ds

  /* using ds segment */
  lgdtl ap_gdt_ptr - ap_boot_start

  xorw %dx, %dx
  movw %dx, %ds 

  movl $pml4t, %eax
  movl %eax, %cr3

  /* set PSE, PAE */
  movl %cr4, %eax
  orl $0x30, %eax
  movl %eax, %cr4

  /* set long mode */
  movl $IA32_EFER, %ecx
  rdmsr
  orl $0x100, %eax
  wrmsr

  /* enable paging, protected mode */
  movl %cr0, %eax
  orl $0x80000001, %eax
  movl %eax, %cr0
  
  ljmpl $0x8, $ap_boot64 - ap_boot_start + SMP_BOOT_ADDR

  .code64
ap_boot64:
  /* set segment registers to GDT data descriptor */
  movw $16, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  movw %ax, %ss

  /* set up ISR */
  lidt idtr
  
  /* set up stack */
  movq $ap_stack_ptr - ap_boot_start + SMP_BOOT_ADDR, %rax
  movq (%rax), %rsp

  movq $status_code - ap_boot_start + SMP_BOOT_ADDR, %rax
  movq $1, (%rax)

  pushq $0  /* end of stack trace */
  mov $ap_main, %rax
  call *%rax

  /* error */
  cli
2:
  hlt
  jmp 2b

ap_gdt_ptr:  
  .short (GDT_ENTRY_NR * 8) - 1
  .quad _kernel_gdt
ap_boot_end:
