#ifndef _SMP_H_
#define _SMP_H_

#define SMP_BOOT_ADDR 0x70000

#ifndef __ASSEMBLER__
#include "types.h"

extern bool smp_boot_cpu(uint8_t lapic);

#endif /* __ASSEMBLER__ */
#endif
