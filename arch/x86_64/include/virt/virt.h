#ifndef _VIRT_H_
#define _VIRT_H_

#include "boot_info.h"

void virt_init(boot_info_t *info);
void virt_percpu_init(void);

#endif
