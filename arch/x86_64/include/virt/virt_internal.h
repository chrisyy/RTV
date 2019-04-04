#ifndef _VIRT_INTERNAL_H_
#define _VIRT_INTERNAL_H_

#include "types.h"
#include "boot_info.h"
#include "utils/spinlock.h"
#include "cpu.h"

#define VM_NONE UINT16_MAX

/* exit information 32-bit fields */
#define VMCS_INSTR_ERROR        0x4400
#define VMCS_EXIT_REASON        0x4402

/* exit information natural-width fields */
#define VMCS_EXIT_QUAL          0x6400

/* guest 64-bit fields */
#define VMCS_GUEST_LP           0x2800
#define VMCS_GUEST_EFER         0x2806
#define VMCS_GUEST_PAT          0x2804

/* guest 32-bit fields */
#define VMCS_GUEST_CS_LMT       0x4802
#define VMCS_GUEST_DS_LMT       0x4806
#define VMCS_GUEST_ES_LMT       0x4800
#define VMCS_GUEST_SS_LMT       0x4804
#define VMCS_GUEST_FS_LMT       0x4808
#define VMCS_GUEST_GS_LMT       0x480A
#define VMCS_GUEST_CS_ACC       0x4816
#define VMCS_GUEST_DS_ACC       0x481A
#define VMCS_GUEST_ES_ACC       0x4814
#define VMCS_GUEST_SS_ACC       0x4818
#define VMCS_GUEST_FS_ACC       0x481C
#define VMCS_GUEST_GS_ACC       0x481E
#define VMCS_GUEST_ACTIVE       0x4826
#define VMCS_GUEST_INT          0x4824

/* guest 16-bit fields */
#define VMCS_GUEST_CS_SEL       0x802
#define VMCS_GUEST_DS_SEL       0x806
#define VMCS_GUEST_ES_SEL       0x800
#define VMCS_GUEST_SS_SEL       0x804
#define VMCS_GUEST_FS_SEL       0x808
#define VMCS_GUEST_GS_SEL       0x80A

/* guest natural-width fields */
#define VMCS_GUEST_CS_BASE      0x6808
#define VMCS_GUEST_DS_BASE      0x680C
#define VMCS_GUEST_ES_BASE      0x6806
#define VMCS_GUEST_SS_BASE      0x680A
#define VMCS_GUEST_FS_BASE      0x680E
#define VMCS_GUEST_GS_BASE      0x6810
#define VMCS_GUEST_RFLAGS       0x6820
#define VMCS_GUEST_CR0          0x6800
#define VMCS_GUEST_CR3          0x6802
#define VMCS_GUEST_CR4          0x6804
#define VMCS_GUEST_DR7          0x681A
#define VMCS_GUEST_DEBUG        0x6822
#define VMCS_GUEST_RIP          0x681E
#define VMCS_GUEST_RSP          0x681C

/* host natural-width fields */
#define VMCS_HOST_CR0           0x6C00
#define VMCS_HOST_CR3           0x6C02
#define VMCS_HOST_CR4           0x6C04
#define VMCS_HOST_FS_BASE       0x6C06
#define VMCS_HOST_GS_BASE       0x6C08
#define VMCS_HOST_TR_BASE       0x6C0A
#define VMCS_HOST_GDTR_BASE     0x6C0C
#define VMCS_HOST_IDTR_BASE     0x6C0E
#define VMCS_HOST_RSP           0x6C14
#define VMCS_HOST_RIP           0x6C16

/* host 64-bit fields */
#define VMCS_HOST_PAT           0x2C00
#define VMCS_HOST_EFER          0x2C02

/* host 16-bit fields */
#define VMCS_HOST_CS_SEL        0xC02
#define VMCS_HOST_DS_SEL        0xC06
#define VMCS_HOST_ES_SEL        0xC00
#define VMCS_HOST_SS_SEL        0xC04
#define VMCS_HOST_FS_SEL        0xC08
#define VMCS_HOST_GS_SEL        0xC0A
#define VMCS_HOST_TR_SEL        0xC0C

/* control 32-bit fields */
#define VMCS_CTRL_PIN           0x4000
#define VMCS_CTRL_PPROC         0x4002
#define VMCS_CTRL_SPROC         0x401E
#define VMCS_CTRL_EXP_MAP       0x4004
#define VMCS_CTRL_EXIT          0x400C
#define VMCS_CTRL_EXIT_SCNT     0x400E
#define VMCS_CTRL_EXIT_LCNT     0x4010
#define VMCS_CTRL_ENTRY         0x4012
#define VMCS_CTRL_ENTRY_LCNT    0x4014
#define VMCS_CTRL_ENTRY_INT     0x4016
#define VMCS_CTRL_CR3_CNT       0x400A
#define VMCS_CTRL_PF_MASK       0x4006
#define VMCS_CTRL_PF_MATCH      0x4008

/* control 64-bit fields */
#define VMCS_CTRL_IOMAP_A       0x2000
#define VMCS_CTRL_IOMAP_B       0x2002
#define VMCS_CTRL_EPTP          0x201A

/* control natural-width fields */
#define VMCS_CTRL_CR0_MASK      0x6000
#define VMCS_CTRL_CR4_MASK      0x6002
#define VMCS_CTRL_CR0_RD        0x6004
#define VMCS_CTRL_CR4_RD        0x6006

/* EPT entry fields */
#define EPT_RD    0x1
#define EPT_WR    0x2
#define EPT_EX    0x4
#define EPT_TP(x) (x << 3)
#define EPT_IPAT  0x40
#define EPT_PG    0x80

typedef struct _vm_struct_t {
  uint16_t vm_id;
  char name[BOOT_STRING_MAX];
  uint64_t img_paddr;
  uint16_t num_cpus;
  uint32_t ram_size;  /* MB */
  uint64_t vmcs_paddr[MAX_CPUS];
  bool lauched[MAX_CPUS];
  spinlock_t lock;
} vm_struct_t;

extern vm_struct_t *vm_structs;
extern uint16_t cpu_to_vm[MAX_CPUS];

#define virt_check_error(flags)   \
do {                              \
  __asm__ volatile("pushfq\n"     \
                   "popq %0" : "=r" (flags));   \
                                                \
  if (flags & FLAGS_CF)                         \
    panic("Invalid VMCS pointer");              \
  else if (flags & FLAGS_ZF) {                  \
    printf("Error number: %u\n", vmread(VMCS_INSTR_ERROR)); \
    panic("VMX error");                                     \
  }                                                         \
} while (0)

uint64_t virt_pg_table_setup(uint32_t size);

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
