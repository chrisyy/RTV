#ifndef _VIRT_INTERNAL_H_
#define _VIRT_INTERNAL_H_

#include "types.h"
#include "boot_info.h"
#include "utils/spinlock.h"
#include "cpu.h"

#define VM_NONE UINT16_MAX
#define VCPU_NONE UINT16_MAX

#define VMCS_INSTR_ERROR    0x4400

typedef struct _vm_struct_t {
  uint16_t vm_id;
  char name[BOOT_STRING_MAX];
  uint64_t img_paddr;
  uint16_t vcpu_map[MAX_CPUS];
  uint16_t num_cpus;
  uint64_t vmcs_paddr[MAX_CPUS];
  spinlock_t lock;
} vm_struct_t;

extern vm_struct_t *vm_structs;
extern uint16_t cpu_to_vm[MAX_CPUS];

void virt_check_error(void);

static inline uint64_t vmread(uint64_t encoding)
{
  uint64_t value;
  __asm__ volatile("vmread %1, %0" : "=r" (value) : "r" (encoding) : "cc");
  return value;
}

static inline void vmwrite(uint64_t val, uint64_t encoding)
{
  __asm__ volatile("vmwrite %0, %1" : : "r" (val), "r" (encoding) : "cc");
}

#endif
