#ifndef _MALLOC_H_
#define _MALLOC_H_

#include "types.h"

/* follows the 2MB dynamic mapping */
#define MALLOC_PHY_BASE (KERNEL_MAPPING_BASE + 0x200000)

extern void malloc_init(void);
extern void *malloc(uint32_t size);
extern void free(void *ptr);

#endif
