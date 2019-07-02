#ifndef _LINUX_H_
#define _LINUX_H_

#include "types.h"

/*
 * start machine physical address for linux memory: 4GB
 * first 1MB is skipped
 */
#define LINUX_PHY_START ((uint64_t) 0x100000000)

#endif
