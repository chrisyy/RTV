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

#endif
