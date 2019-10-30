#ifndef _PHYSICAL_H_
#define _PHYSICAL_H_

#include "types.h"
#include "vm.h"

#define MM_TABLE_MAX (((uint64_t) MM_PHYSICAL_MAX * 1024 * 1024 * 1024) / (8 * sizeof(uint64_t) * PG_SIZE))

extern void physical_free_range(uint64_t begin, uint64_t length);
extern void physical_take_range(uint64_t begin, uint64_t length);
extern bool physical_is_free(uint64_t addr);
extern void physical_set_limit(uint64_t limit);
extern void physical_check_table(void);
extern void physical_next_free_range(uint64_t current, uint64_t *start, uint64_t *size);
extern uint64_t alloc_phys_frame(void);
extern uint64_t alloc_phys_frames(uint64_t num);
extern uint64_t alloc_phys_frames_aligned(uint64_t num, uint64_t align);
extern uint64_t alloc_phys_frame_lowmem(void);
extern void free_phys_frame(uint64_t frame);
extern void free_phys_frames(uint64_t frame, uint64_t num);

#endif
