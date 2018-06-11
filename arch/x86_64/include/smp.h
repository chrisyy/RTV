#ifndef _SMP_H_
#define _SMP_H_

#define SMP_BOOT_ADDR 0x70000

#ifndef __ASSEMBLER__
#include "types.h"

#define compiler_barrier() __asm__ volatile("" : : : "memory")

extern bool smp_boot_cpu(uint8_t lapic);

#endif /* __ASSEMBLER__ */
#endif
