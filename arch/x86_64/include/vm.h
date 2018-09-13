#ifndef _VM_H_
#define _VM_H_

#include "types.h"

#define PG_BITS 12
#define PG_SIZE (1 << PG_BITS)
#define PG_MASK (~((uint64_t) PG_SIZE - 1))

#define LARGE_PG_BITS 21
#define LARGE_PG_SIZE (1 << LARGE_PG_BITS)

/* flags of page table entry */
#define PGT_P   0x1
#define PGT_RW  0x2
#define PGT_US  0x4
#define PGT_PWT 0x8
#define PGT_PCD 0x10
#define PGT_A   0x20
#define PGT_D   0x40
#define PGT_PAT 0x80
#define PGT_G   0x100
#define PGT_XD  0x8000000000000000

/* flags of PDT entry */
#define PDT_P   0x1
#define PDT_RW  0x2
#define PDT_US  0x4
#define PDT_PWT 0x8
#define PDT_PCD 0x10
#define PDT_A   0x20
#define PDT_D   0x40
#define PDT_PS  0x80
#define PDT_G   0x100
#define PDT_PAT 0x1000
#define PDT_XD  0x8000000000000000

#define PGT_MASK ((uint64_t) 0xFFFFFFFFFFFFF000)

/* 2~6 MB is static mapping for kernel, this 2MB is for dynamic mapping */
#define KERNEL_MAPPING_BASE ((uint64_t) 0x600000)

extern void vm_init(void);
extern void *vm_map_page(uint64_t frame, uint64_t flags);
extern void *vm_map_pages(uint64_t frame, uint64_t num, uint64_t flags);
extern uint64_t vm_unmap_page(void *va);
extern uint64_t vm_unmap_pages(void *va, uint64_t num);
extern void vm_free_page(void *va);
extern void vm_map_page_unrestricted(uint64_t frame, uint64_t flags, uint64_t vaddr);
extern void vm_map_large_page_unrestricted(uint64_t frame, uint64_t flags, uint64_t vaddr);
extern void vm_check_mapping(uint64_t *addr);

static inline void invalidate_page(void *va)
{
  __asm__ volatile("invlpg (%0)": : "r" (va) : "memory");
}
#endif
