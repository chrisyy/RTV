#ifndef _ASM_STRING_H_
#define _ASM_STRING_H_

#include "types.h"

static inline void *memcpy(void *dest, const void *src, uint64_t cb)
{
  __asm__ volatile("cld; rep movsb"
                   : "=c" (cb), "=D" (dest), "=S" (src)
                   : "0" (cb), "1" (dest), "2" (src)
                   : "memory" , "flags");
  return dest;
}

static inline void *memset(void *s, uint8_t c, uint64_t n)
{
  __asm__ volatile("cld; rep stosb"
                   : "=D" (s), "=a" (c), "=c" (n)
                   : "0" (s), "1" (c), "2" (n)
                   : "memory" , "flags");
  return s;
}

#endif
