#ifndef _ASM_STRING_H_
#define _ASM_STRING_H_

static inline void *memcpy(void *pDest, const void *pSrc, uint64_t cb)
{
  __asm__ volatile("cld; rep movsb"
                   : "=c" (cb), "=D" (pDest), "=S" (pSrc)
                   : "0" (cb), "1" (pDest), "2" (pSrc)
                   : "memory" , "flags");
  return pDest;
}

#endif
