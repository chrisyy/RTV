#ifndef _VIRT_INTERNAL_H_
#define _VIRT_INTERNAL_H_

#include "types.h"
#include "boot_info.h"

typedef struct _vm_struct_t {
  uint16_t vm_id;
  char name[BOOT_STRING_MAX];
  uint64_t img_paddr;
} vm_struct_t;

extern vm_struct_t *vm_structs;
extern uint16_t cpu_to_vm[MAX_CPUS];

#endif
