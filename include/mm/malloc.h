#ifndef _MALLOC_H_
#define _MALLOC_H_

#include "types.h"
#include "utils/list.h"

/* 2~6 MB is static mapping for kernel, malloc pool follows */
#define MALLOC_BASE ((uint64_t) 0x600000)

#define BUDDY_MIN_ORDER 5
#define BUDDY_MAX_ORDER 22

#define MALLOC_END (MALLOC_BASE + ((uint64_t) 1 << BUDDY_MAX_ORDER))

#define BUDDY_MIN_BLK (1 << BUDDY_MIN_ORDER)
#define BUDDY_MAX_BLK (1 << BUDDY_MAX_ORDER)

#define USED 0xFF

/*
 * Metadata about this particular block,
 * stored at the beginning of this block.
 * One per free block of memory.
 * There is also 1B meta isFree at the
 * end of the block.
 */
typedef struct _buddy_list
{
  struct list_head list;
} buddy_list_t;

/*
 * Bucket of 2^order sized free memory blocks.
 */
typedef struct _buddy_bucket
{
    struct list_head ptr;
    uint32_t count;
    uint32_t size;
} buddy_bucket_t;

#define BUDDY_ENTRIES (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1)

extern void malloc_init(void);
extern void *malloc(uint32_t size);
extern void free(void *ptr);

#endif
