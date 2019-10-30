#ifndef _MATH_H_
#define _MATH_H_

#include "types.h"

static inline uint64_t ceiling64(uint64_t data, uint64_t base)
{
  return ((data + base - 1) / base) * base;
}

#endif
