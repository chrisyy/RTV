OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
PHDRS {
  physical PT_LOAD FILEHDR PHDRS;
  virtual PT_LOAD;
}

SECTIONS
{
  /* starts from 1MB */
  . = 1M;
  . += SIZEOF_HEADERS;

  .physicaltext : ALIGN(4K)
  {
    boot/boot.o (.multiboot)
    boot/boot.o (.text)
  } : physical
  .physicalrodata : ALIGN(4K)
  {
    boot/boot.o (.rodata)
  } : physical
  .physicaldata : ALIGN(4K)
  {
    boot/boot.o (.data)
  } : physical
  .physicalbss : ALIGN(4K)
  {
    boot/boot.o (.bss COMMON)
  } : physical

  _kernel_start_pa = ALIGN(4K);

  _kernel_start_va = 0x80000000;
  . = _kernel_start_va;

  .text : AT(_kernel_start_pa) ALIGN(4K)
  {
    *(.text)
    /**(EXCLUDE_FILE(*boot/boot.o) .text)*/
  } : virtual

  .rodata : AT(LOADADDR(.text) + SIZEOF(.text)) ALIGN(4K)
  {
    *(.rodata)
    /**(EXCLUDE_FILE(*boot/boot.o) .rodata)*/
  } : virtual

  .data : AT(LOADADDR(.rodata) + SIZEOF(.rodata)) ALIGN(4K)
  {
    *(.data)
    /**(EXCLUDE_FILE(*boot/boot.o) .data)*/
  } : virtual

  .bss : AT(LOADADDR(.data) + SIZEOF(.data)) ALIGN(4K)
  {
    *(.bss COMMON)
    /**(EXCLUDE_FILE(*boot/boot.o) .bss COMMON)*/
  } : virtual
}
