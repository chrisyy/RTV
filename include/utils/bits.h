#ifndef _BITS_H_
#define _BITS_H_

#include "types.h"

static inline uint8_t get_bit64(uint64_t data, uint8_t index)
{
  /* type cast to prevent overflow */
  uint64_t mask = (uint64_t) 1 << index;
  return (data & mask) > 0 ? 1 : 0;
}

static inline void set_bit64(uint64_t *data, uint8_t index)
{
  uint64_t mask = (uint64_t) 1 << index;
  *data |= mask;
}

static inline void clear_bit64(uint64_t *data, uint8_t index)
{
  uint64_t mask = ~((uint64_t) 1 << index);
  *data &= mask;
}

static inline uint8_t bitmap64_get(uint64_t *map, uint64_t index)
{
  return (map[index >> 6] & ((uint64_t) 1 << (index & 63))) > 0 ? 1 : 0;
}

static inline void bitmap64_set(uint64_t *map, uint64_t index)
{
  map[index >> 6] |= ((uint64_t) 1 << (index & 63));
}

static inline void bitmap64_clear(uint64_t *map, uint64_t index)
{
  map[index >> 6] &= ~((uint64_t) 1 << (index & 63));
}

extern void bitmap64_set_range(uint64_t *map, uint64_t begin, uint64_t length);
extern void bitmap64_clear_range(uint64_t *map, uint64_t begin, uint64_t length);
extern bool bitmap64_check_bit(uint64_t *map, uint64_t index);

#endif
