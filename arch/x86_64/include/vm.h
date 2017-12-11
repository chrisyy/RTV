#ifndef _VM_H_
#define _VM_H_

#include "types.h"

/* bits of page table entry */
#define PGT_P   0x1
#define PGT_RW  0x2
#define PGT_US  0x4
#define PGT_PWT 0x8
#define PGT_PCD 0x10
#define PGT_A   0x20
#define PGT_D   0x40
#define PGT_PAT 0x80
#define PGT_G   0x100

#define PGT_MASK 0xFFFFFFFFFFFFF000

/* first 2MB is identity mapping for kernel, this 2MB is for dynamic mapping */
#define KERNEL_MAPPING_BASE 0x200000

extern void vm_init(void);
extern void *vm_map_page(uint64_t frame, uint64_t flags);
extern void *vm_map_pages(uint64_t frame, size_t num, uint64_t flags);
extern void vm_unmap_page(void *va);
extern void vm_unmap_pages(void *va, size_t num);

static inline void invalidate_page(void *va)
{
  __asm__ volatile("invlpg (%0)": : "b" (va) : "memory");
}

#endif
