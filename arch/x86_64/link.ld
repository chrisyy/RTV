OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
  /* starts from 1MB */
  _boot_start = 1M;
  . = _boot_start;

  .physicaltext : ALIGN(4K)
  {
    arch/x86_64/boot/boot.o (.multiboot)
    arch/x86_64/boot/boot.o (.text)
  } 
  .physicalrodata : ALIGN(4K)
  {
    arch/x86_64/boot/boot.o (.rodata)
  } 
  .physicaldata : ALIGN(4K)
  {
    arch/x86_64/boot/boot.o (.data)
  } 
  .physicalbss : ALIGN(4K)
  {
    arch/x86_64/boot/boot.o (.bss COMMON)
  } 

  _kernel_start_pa = ALIGN(4K);
  _boot_pages = (_kernel_start_pa - _boot_start) / 0x1000;

  _kernel_start_va = _kernel_start_pa;
  . = _kernel_start_va;

  .text : AT(_kernel_start_pa) ALIGN(4K)
  {
    _text = .;
    *(EXCLUDE_FILE(*boot/boot.o) .text)
  }
  _text_end = ALIGN(4K);

  .rodata : AT(LOADADDR(.text) + _text_end - _text) ALIGN(4K)
  {
    _rodata = .;
    *(EXCLUDE_FILE(*boot/boot.o) .rodata)
  }
  _rodata_end = ALIGN(4K);

  .data : AT(LOADADDR(.rodata) + _rodata_end - _rodata) ALIGN(4K)
  {
    _data = .;
    *(EXCLUDE_FILE(*boot/boot.o) .data)
  }
  _data_end = ALIGN(4K);

  .bss : AT(LOADADDR(.data) + _data_end - _data) ALIGN(4K)
  {
    _bss = .;
    *(EXCLUDE_FILE(*boot/boot.o) .bss COMMON)
  }
  _bss_end = ALIGN(4K);

  _kernel_ro_pages = (_rodata_end - _kernel_start_va) / 0x1000;
  _kernel_rw_pages = (_bss_end - _data) / 0x1000;

  .eh_frame : AT(LOADADDR(.bss) + _bss_end - _bss) ALIGN(4K)
  {
    *(.eh_frame)
  }
}
