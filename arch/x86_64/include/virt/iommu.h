#ifndef _IOMMU_H_
#define _IOMMU_H_

#include "types.h"

#define IOMMU_VERSION       0x0
#define IOMMU_CAP           0x8
#define IOMMU_EXT_CAP       0x10

void iommu_init(uint64_t base);

#endif
