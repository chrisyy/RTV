#ifndef _PHYSICAL_H_
#define _PHYSICAL_H_

#include "types.h"
#include "vm.h"

/* maximum supported physical memory size (GB) */
#define MM_PHYSICAL_MAX 64
#define MM_TABLE_MAX (((uint64_t) MM_PHYSICAL_MAX * 1024 * 1024 * 1024) / (8 * sizeof(uint64_t) * PG_SIZE))

extern void physical_free_range(uint64_t begin, uint64_t length);
extern void physical_take_range(uint64_t begin, uint64_t length);
extern void physical_set_limit(uint64_t limit);

#endif
