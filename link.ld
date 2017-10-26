OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
  /* starts from 1MB */
  . = 1M;

  .physicaltext : ALIGN(4K)
  {
    boot/boot.o (.multiboot)
    boot/boot.o (.text)
  } 
  .physicalrodata : ALIGN(4K)
  {
    boot/boot.o (.rodata)
  } 
  .physicaldata : ALIGN(4K)
  {
    boot/boot.o (.data)
  } 
  .physicalbss : ALIGN(4K)
  {
    boot/boot.o (.bss COMMON)
  } 

  _kernel_start_pa = ALIGN(4K);

  _kernel_start_va = _kernel_start_pa;
  . = _kernel_start_va;

  .text : AT(_kernel_start_pa) ALIGN(4K)
  {
    *(EXCLUDE_FILE(*boot/boot.o) .text)
  } 

  .rodata : AT(LOADADDR(.text) + SIZEOF(.text)) ALIGN(4K)
  {
    *(EXCLUDE_FILE(*boot/boot.o) .rodata)
  } 

  .data : AT(LOADADDR(.rodata) + SIZEOF(.rodata)) ALIGN(4K)
  {
    *(EXCLUDE_FILE(*boot/boot.o) .data)
  } 

  .bss : AT(LOADADDR(.data) + SIZEOF(.data)) ALIGN(4K)
  {
    *(EXCLUDE_FILE(*boot/boot.o) .bss COMMON)
  } 
}
