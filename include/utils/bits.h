#ifndef _BITS_H_
#define _BITS_H_

#include "types.h"

static inline uint8_t get_bit64(uint64_t data, uint8_t offset)
{
  uint64_t mask = 1 << offset;
  return (data & mask) ? 1 : 0;
}

static inline void set_bit64(uint64_t *data, uint8_t offset)
{
  uint64_t mask = 1 << offset;
  *data |= mask;
}

static inline void clear_bit64(uint64_t *data, uint8_t offset)
{
  uint64_t mask = ~(1 << offset);
  *data &= mask;
}

static inline void bitmap64_set(uint64_t *map, uint64_t index)
{
  map[index >> 6] |= (1 << (index & 63));
}

static inline void bitmap64_clear(uint64_t *map, uint64_t index)
{
  map[index >> 6] &= ~(1 << (index & 63));
}

extern void bitmap64_set_range(uint64_t *map, uint64_t begin, uint64_t length);

extern void bitmap64_clear_range(uint64_t *map, uint64_t begin, uint64_t length);

#endif
