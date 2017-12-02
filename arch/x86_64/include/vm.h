#ifndef _VM_H_
#define _VM_H_

#include "types.h"

/* bits of page table entry */
#define PGT_P   0
#define PGT_RW  1
#define PGT_US  2
#define PGT_PWT 3
#define PGT_PCD 4
#define PGT_A   5
#define PGT_D   6
#define PGT_PAT 7
#define PGT_G   8

#define KERNEL_MAPPING_BASE 0x200000

extern void vm_init(void);
extern void *vm_map_page(uint64_t frame);

static inline void invalidate_page(void *va)
{
  __asm__ volatile("invlpg (%0)": : "b" (va) : "memory");
}

#endif
