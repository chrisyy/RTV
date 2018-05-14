#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include "types.h"

#define INTERRUPT_GATE_TYPE 0xE
#define TRAP_GATE_TYPE 0xF

#define PIC1_BASE_IRQ 0x20
#define PIC2_BASE_IRQ 0x28

#define PIT_FREQ 1193182
#define HZ 100

#define PIT_IRQ 0

#define EXCEPTION_PG_FAULT 14

void interrupt_init(void);

static inline void interrupt_enable(void)
{
  __asm__ volatile("sti");
}

static inline void interrupt_disable(void)
{
  __asm__ volatile("cli");
}

static inline void interrupt_disable_save(uint64_t *flag)
{
  __asm__ volatile("pushfq\n"
                   "cli\n"
                   "popq %0\n" : "=a" (*flag) : : );
}

static inline void interrupt_enable_restore(uint64_t flag)
{
  __asm__ volatile("pushq %0\n"
                   "popfq\n" : : "a" (flag) : );
}



#endif
