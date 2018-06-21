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

  .set MAGIC, 0xE85250D6
  .set ARCH, 0x0

  .section .bss
  .align 0x1000

  .comm pml4t, 0x1000
  .comm pdpt, 0x1000
  .comm pdt, 0x1000
  .comm kernel_page_tables, 0x2000

  .global kernel_stack
kernel_stack:
  .skip 0x1000
stack_top:

  .section .data
  .global gdt64
gdt64:
  /* null descriptor as required */
  .quad 0
  /* code */
  .quad 0x00209A0000000000
  /* data */
  .quad 0x0000920000000000
  /* 4KB GDT */
  .rept GDT_ENTRY_NR - 3
  .quad 0
  .endr
gdt_ptr:
  .short . - gdt64 - 1
  .quad gdt64

  .section .rodata
msg:
  .ascii "Failure: jumping to _start64"
msg_len:
  .long . - msg

  /* multiboot 2 */
  .section .multiboot
header_start:
  .align 8
  .long MAGIC
  .long ARCH
  .long header_end - header_start
  .long -(MAGIC + ARCH + header_end - header_start)
  /* ending tag */
  .short 0
  .short 0
  .long 0x8
header_end:

  .section .text
  .code32

  .global _start
_start:
  movl $stack_top, %esp
  pushl %eax  /* multiboot_magic */
  pushl %ebx  /* multiboot_info */

  /* disable paging */
  movl %cr0, %eax
  andl $0x7FFFFFFF, %eax
  movl %eax, %cr0

  /* set up first 2MB identity mapping */
  movl $pdpt + 3, %eax  /* present+write bits */
  movl %eax, pml4t

  movl $pdt + 3, %eax
  movl %eax, pdpt

  movl $0x83, %eax  /* 2MB page */
  movl %eax, pdt

#if 0
  movl $0x200083, %eax
  movl %eax, pdt + 8

  movl $0x400083, %eax
  movl %eax, pdt + 16
#endif

#if 1
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
  movl $0, %eax
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
  movl $0, %eax
  stosl
  movl %ebx, %eax
  loop 1b
#endif

  /* install paging struct */
  movl $pml4t, %eax
  movl %eax, %cr3 

  /* enable PAE paging and PSE */
  movl %cr4, %eax
  orl $0x30, %eax
  movl %eax, %cr4

  /* set long mode */
  movl $IA32_EFER, %ecx
  rdmsr
  orl $0x100, %eax
  wrmsr

  /* enable paging */
  movl %cr0, %eax
  orl $0x80000000, %eax
  movl %eax, %cr0 

  lgdt gdt_ptr
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

