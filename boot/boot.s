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

  .set MAGIC, 0xE85250D6
  .set ARCH, 0x0

  .section .bss
  .align 0x1000
  .global PDT

  .comm PML4T, 0x1000
  .comm PDPT, 0x1000
  .comm PDT, 0x1000

  .section .rodata
GDT64:
  /* null descriptor as required */
  .quad 0
  /* code */
  .quad 0x00209A0000000000
  /* data */
  .quad 0x0000920000000000
GDT_ptr:
  .short . - GDT64 - 1
  .quad GDT64

MSG:
  .ascii "Failure: jumping to _start64"
MSG_LEN:
  .short . - MSG
  
  /* multiboot 2 */
  .section .multiboot
header_start:
  .align 8
  .long MAGIC
  .long ARCH
  .long header_end - header_start
  .long -(MAGIC + ARCH + header_end - header_start)
  .short 0
  .short 0
  .long 0x8
header_end:

  .section .text
  .code32

  .global _start

_start:
  /* disable paging */
  movl %cr0, %eax
  andl $0x7FFFFFFF, %eax
  movl %eax, %cr0

  /* set up first 4MB identity mapping */
  movl $PDPT, %eax
  orl $0x3, %eax  /* present+write bits */
  movl %eax, PML4T

  movl $PDT, %eax
  orl $0x3, %eax
  movl %eax, PDPT

  movl $0x83, %eax  /* 2MB page */
  movl %eax, PDT

  movl $0x200000, %eax
  orl $0x83, %eax   /* 2MB page */
  movl %eax, PDT+8

  movl $PML4T, %eax
  movl %eax, %cr3 

  /* enable PAE paging */
  movl %cr4, %eax
  orl $0x20, %eax
  movl %eax, %cr4

  /* set long mode */
  movl $0xC0000080, %ecx  /* IA32_EFER */
  rdmsr
  orl $0x100, %eax
  wrmsr

  /* enable paging */
  movl %cr0, %eax
  orl $0x80000000, %eax
  movl %eax, %cr0 

  lgdt GDT_ptr
  ljmp $0x8, $_start64
  
  /* error */
  cli

  movb $0x2F, %ah
  movl MSG_LEN, %ecx
  movl $MSG, %esi
  movl $0xB8000, %edi
1:
  lodsb
  stosw
  loop 1b
2:
  hlt
  jmp 2b


