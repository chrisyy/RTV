#ifndef _CPU_H_
#define _CPU_H_

/* 4KB GDT/IDT */
#define GDT_ENTRY_NR 512
#define IDT_ENTRY_NR 256

/* skip null + code + data */
#define GDT_START 3

#define FLAGS_CF  0x1
#define FLAGS_ZF  0x40

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

#define gdt_desc idt_desc

uint16_t alloc_tss_desc(tss_t *tss_p);
uint64_t get_gdt_tss_base(uint16_t sel);
void mtrr_config(void);

static inline seg_desc *get_gdt(void)
{
  extern uint64_t _kernel_gdt;
  return (seg_desc *) &_kernel_gdt;
}

static inline idt_entry *get_idt(void)
{
  extern uint64_t _kernel_idt;
  return (idt_entry *) (&_kernel_idt);
}

static inline void halt(void)
{
  __asm__ volatile("hlt");
}

static inline void pause(void)
{
  __asm__ volatile("pause" : : : "memory");
}

static inline void load_tr(uint16_t selector)
{
  __asm__ volatile("ltr %0" : : "r" (selector));
}

static inline void cpuid(uint32_t in_eax, uint32_t in_ecx, uint32_t *out_eax, 
                         uint32_t *out_ebx, uint32_t *out_ecx, uint32_t *out_edx)
{
  uint32_t eax, ebx, ecx, edx;
  __asm__ volatile("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                   : "a" (in_eax), "c" (in_ecx));
  if (out_eax)
    *out_eax = eax;
  if (out_ebx)
    *out_ebx = ebx;
  if (out_ecx)
    *out_ecx = ecx;
  if (out_edx)
    *out_edx = edx;
}

static inline uint64_t rdmsr(uint32_t ecx)
{
  uint32_t edx, eax;

  __asm__ volatile("rdmsr" : "=d" (edx), "=a" (eax) : "c" (ecx));
  return (((uint64_t) edx) << 32) | ((uint64_t) eax);
}

static inline void wrmsr(uint32_t ecx, uint64_t val)
{
  uint32_t edx, eax;

  edx = (uint32_t) (val >> 32);
  eax = (uint32_t) val;

  __asm__ volatile("wrmsr" : : "d" (edx), "a" (eax), "c" (ecx));
}

static inline void compiler_barrier(void)
{
  __asm__ volatile("" : : : "memory");
}

static inline void mem_barrier(void)
{
  __asm__ volatile("mfence" : : : "memory");
}

static inline void tlb_flush(void)
{
  uint64_t tmp;
  __asm__ volatile("movq %%cr3, %0" : "=r" (tmp));
  __asm__ volatile("movq %0, %%cr3" : : "r" (tmp));
  mem_barrier();
}

static inline uint64_t rdtsc(void)
{
  uint32_t low, high;
  uint64_t time;
  __asm__ volatile("lfence\n"
                   "rdtsc" : "=a" (low), "=d" (high) : : "memory");
  time = high;
  time <<= 32;
  time |= low;
  return time;
}

#endif /* __ASSEMBLER__ */
#endif
