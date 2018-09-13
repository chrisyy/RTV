#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PACKED __attribute__ ((packed))
#define ALIGNED(x) __attribute__ ((aligned (x)))

#define container_of(ptr, type, member) ({                    \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);  \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#endif
