#ifndef _CPU_H_
#define _CPU_H_

/* 4KB GDT */
#define GDT_ENTRY_NR 512
#define IDT_ENTRY_NR 256

#ifndef __ASSEMBLER__
#include "types.h"

typedef struct _tss {
  uint32_t reserved0;
  uint64_t rsp[3];
  uint64_t reserved1;
  uint64_t ist[7];
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t iomap_base;
} PACKED tss_t;

typedef struct _tss_desc {
  uint64_t limit0 : 16; /* limit 15:0 */
  uint64_t base0 : 24;  /* base address 23:0 */
  uint64_t type : 4;
  uint64_t zero0 : 1;
  uint64_t dpl : 2;
  uint64_t p : 1;       /* present */
  uint64_t limit1 : 4;  /* limit 19:16 */
  uint64_t avl : 1;
  uint64_t zero1 : 2;
  uint64_t g : 1;
  uint64_t base1 : 40;  /* base address 63:24 */
  uint64_t reserved0 : 32;
} PACKED tss_desc;

typedef struct _seg_desc {
  uint64_t limit0 : 16;  /* limit 15:0 */
  uint64_t base0 : 24;  /* base address 23:0 */
  uint64_t type : 4;
  uint64_t s : 1;
  uint64_t dpl : 2;
  uint64_t p : 1;       /* present */
  uint64_t limit1 : 4;  /* limit 19:16 */
  uint64_t avl : 1;
  uint64_t l : 1;
  uint64_t db : 1;
  uint64_t g : 1;
  uint64_t base1 : 8;   /* base address 32:24 */
} PACKED seg_desc;

typedef struct _idt_entry {
  uint16_t offset0;     /* offset 15:0 */
  uint16_t selector;
  uint16_t ist : 3;
  uint16_t zero0 : 5;
  uint16_t type : 4;
  uint16_t zero1 : 1;
  uint16_t dpl : 2;
  uint16_t p : 1;
  uint16_t offset1;     /* offset 31:16 */
  uint32_t offset2;     /* offset 63:32 */
  uint32_t reserved;
} PACKED idt_entry;

typedef struct _idt_desc {
  uint16_t limit;
  uint64_t base;
} PACKED idt_desc;

uint16_t alloc_tss_desc(tss_t *tss_p);

static inline void halt(void)
{
  __asm__ volatile("hlt");
}

static inline void load_tr(uint16_t selector)
{
  __asm__ volatile("ltr %0" : : "r" (selector));
}

#endif /* __ASSEMBLER__ */
#endif
