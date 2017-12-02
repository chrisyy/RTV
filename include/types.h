#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PACKED __attribute__ ((packed))
#define ALIGNED(x) __attribute__ ((aligned (x)))

#define PG_BITS 12
#define PG_SIZE (1 << PG_BITS)

#endif
