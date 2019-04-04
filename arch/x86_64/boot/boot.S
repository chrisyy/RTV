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
#include "multiboot2.h"

  .section .bss
  .align 0x1000
  .global pml4t
pml4t:
  .skip 0x1000
pdpt:
  .skip 0x1000
pdt:
  .skip 0x1000
kernel_page_tables:
  .skip 0x2000
new_multiboot:
  .skip 0x1000

tmp_stack:
  .skip 0x1000
stack_top:

  .global my_cr0
  .global my_cr3
  .global my_cr4
  .global my_cs
  .global my_ds
my_cr0:
  .skip 0x4
my_cr3:
  .skip 0x4
my_cr4:
  .skip 0x4
my_cs:
  .skip 0x2
my_ds:
  .skip 0x2

  .section .data
  .align 0x1000
gdt64:
  /* null descriptor as required */
  .quad 0
  /* code */
  .quad 0x00209A0000000000
  /* data */
  .quad 0x0000920000000000
  .rept GDT_ENTRY_NR - 3
  .quad 0
  .endr
idt64:
  .rept IDT_ENTRY_NR
  .quad 0
  .quad 0
  .endr
gdt_ptr:
  .short (GDT_ENTRY_NR * 8) - 1
  .quad _kernel_gdt

  .section .rodata
msg:
  .ascii "Failure: jumping to _start64"
msg_len:
  .long . - msg

  /* multiboot 2 */
  .section .multiboot
header_start:
  .align 8
  .long MULTIBOOT2_HEADER_MAGIC
  .long MULTIBOOT_ARCHITECTURE_I386
  .long header_end - header_start
  .long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + header_end - header_start)
  /* ending tag */
  .short MULTIBOOT_HEADER_TAG_END
  .short 0
  .long 0x8
header_end:

  .section .text
  .code32

  .global _start
_start:
  movl $stack_top, %esp
  pushl %eax  /* multiboot_magic */

  /* relocate multiboot_info from ebx */
  cld
  movl (%ebx), %ecx
  movl %ebx, %esi
  movl $new_multiboot, %edi
  rep movsb

  movl %cr0, %eax
  movl %eax, my_cr0
  movl %cr3, %eax
  movl %eax, my_cr3
  movl %cr4, %eax
  movl %eax, my_cr4
  movw %cs, %ax
  movw %ax, my_cs
  movw %ds, %ax
  movw %ax, my_ds

  /* set up first 2MB identity mapping */
  movl $pdpt + 3, %eax  /* present+write bits */
  movl %eax, pml4t

  movl $pdt + 3, %eax
  movl %eax, pdpt

  movl $0x83, %eax  /* 2MB page */
  movl %eax, pdt

  /* set up kernel mapping from address 2MB, size limited to 4MB */
  movl $kernel_page_tables + 3, %eax
  movl %eax, pdt + 8

  addl $0x1000, %eax
  movl %eax, pdt + 16

  movl $kernel_page_tables, %edi

  /* code */
  movl $_kernel_start_pa + 1, %eax  /* present bit */
  movl $_kernel_code_pages, %ecx
1:
  stosl
  addl $0x1000, %eax
  movl %eax, %ebx
  movl $0, %eax
  stosl
  movl %ebx, %eax
  loop 1b

  /* ro data */
  movl $_kernel_ro_pages, %ecx
1:
  stosl
  addl $0x1000, %eax
  movl %eax, %ebx
  movl $0x10000000, %eax  /* execute disable */
  stosl
  movl %ebx, %eax
  loop 1b

  /* rw data */
  orl $0x3, %eax  /* present+write bits */
  movl $_kernel_rw_pages, %ecx
1:
  stosl
  addl $0x1000, %eax
  movl %eax, %ebx
  movl $0x10000000, %eax
  stosl
  movl %ebx, %eax
  loop 1b

  /* map GDT */
  movl $gdt64 + 3, %eax
  stosl
  movl $0x10000000, %eax
  stosl

  /* map IDT */
  movl $idt64 + 3, %eax
  stosl
  movl $0x10000000, %eax
  stosl

  /* set up Recursive Mapping in the last entry */
  movl $pml4t + 3, %eax
  movl %eax, pml4t + 4088
  movl $0x10000000, %eax
  movl %eax, pml4t + 4092

  /* install paging struct */
  movl $pml4t, %eax
  movl %eax, %cr3 

  /* enable PAE paging */
  movl %cr4, %eax
  orl $0x20, %eax
  movl %eax, %cr4

  /* enable long mode + execute disable */
  movl $IA32_EFER, %ecx
  rdmsr
  orl $0x900, %eax
  wrmsr

  /* enable paging/cache */
  movl %cr0, %eax
  orl $0x80000000, %eax
  andl $0x9FFFFFFF, %eax
  movl %eax, %cr0 

  lgdt gdt_ptr

  /* switch to stack with virtual address */
  movl $new_multiboot, %ebx
  popl %eax
  movl $kernel_stack + 0x1000, %esp
  pushl %eax
  pushl %ebx

  ljmp $0x8, $start64
  
  /* error */
  cli

  movb $0x2F, %ah
  movl msg_len, %ecx
  movl $msg, %esi
  movl $0xB8000, %edi
1:
  lodsb
  stosw
  loop 1b

2:
  hlt
  jmp 2b

