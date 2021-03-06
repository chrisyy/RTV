#ifndef _IO_H_
#define _IO_H_

#include "types.h"

#define PIT_CHANNEL0 0x40
#define PIT_CMD 0x43

#define PIC1 0x20		/* IO base address for master PIC */
#define PIC2 0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2+1)

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void mmio_write32(void *addr, uint32_t data)
{
  *((volatile uint32_t *) addr) = data;
}

static inline uint32_t mmio_read32(void *addr)
{
  return *((volatile uint32_t *) addr);
}

#endif
