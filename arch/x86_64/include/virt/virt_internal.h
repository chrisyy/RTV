#ifndef _VIRT_INTERNAL_H_
#define _VIRT_INTERNAL_H_

#include "types.h"
#include "boot_info.h"
#include "utils/spinlock.h"
#include "cpu.h"

#define VM_NONE UINT16_MAX

#define VMCS_INSTR_ERROR    0x4400

#define VMCS_GUEST_RFLAGS   0x6820
#define VMCS_GUEST_CR0      0x6800
#define VMCS_GUEST_CR3      0x6802
#define VMCS_GUEST_CR4      0x6804
#define VMCS_GUEST_DR7      0x681A

typedef struct _vm_struct_t {
  uint16_t vm_id;
  char name[BOOT_STRING_MAX];
  uint64_t img_paddr;
  uint16_t num_cpus;
  uint64_t vmcs_paddr[MAX_CPUS];
  bool lauched[MAX_CPUS];
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

static inline void vmwrite(uint64_t encoding, uint64_t val)
{
  __asm__ volatile("vmwrite %0, %1" : : "r" (val), "r" (encoding) : "cc");
}

static inline void vmxon(uint64_t paddr)
{
  __asm__ volatile("vmxon %0" : : "m" (paddr) : "cc");
}

static inline void vmclear(uint64_t paddr)
{
  __asm__ volatile("vmclear %0" : : "m" (paddr) : "cc");
}

static inline void vmptrld(uint64_t paddr)
{
  __asm__ volatile("vmptrld %0" : : "m" (paddr) : "cc");
}

#endif
