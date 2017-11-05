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

  .section .text
  .code64

  .global _start64
_start64:
  /* Set segment registers to GDT data descriptor */
  movw $16, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  movw %ax, %ss

  /* x86_64 calling convention */
  movq $0xFFFFFFFF, %rax
  popq %rbx
  movq %rbx, %rcx
  andq %rax, %rbx  /* multiboot_info */
  movq %rbx, %rsi
  shrq $32, %rcx  /* multiboot_magic */
  movq %rcx, %rdi

  pushq $0  /* end of stack trace */

  call kernel_main

  /* error */
  cli
1:
  hlt
  jmp 1b
