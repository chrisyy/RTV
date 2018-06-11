#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#include "types.h"

/* GCC built-in, __ATOMIC_SEQ_CST ensures inter-thread synchronization */

/* type *dst, type val */
#define atomic_store(dst, val) __atomic_store_n((dst), (val), __ATOMIC_SEQ_CST)
/* type *dst, type val */
#define atomic_fetch_add(dst, val) __atomic_fetch_add((dst), (val), __ATOMIC_SEQ_CST)
/* type *dst */
#define atomic_increment(dst) __atomic_fetch_add((dst), 1, __ATOMIC_SEQ_CST)
/* type *dst, *expected, type newval */
#define atomic_cmp_xchg(dst, expected, newval) \
__atomic_compare_exchange_n((dst), (expected), (newval), false, \
                          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#endif
