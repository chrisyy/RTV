/*                       RTV Real-Time Hypervisor
 * Copyright (C) 2017  Ying Ye
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "interrupt.h"
#include "cpu.h"
#include "io.h"
#include "utils/screen.h"
#include "vm.h"
#include "apic.h"

idt_entry idt64[IDT_ENTRY_NR] ALIGNED(PG_SIZE);
idt_desc idtr;

extern void *int_table[IDT_ENTRY_NR];

static inline void idt_set_entry(uint16_t index, uint8_t type, void *handler(), idt_entry *idt)
{
  uint64_t base = (uint64_t) handler;           
  idt[index].offset0 = (uint16_t) base;             
  idt[index].selector = 0x8;  /* code */            
  idt[index].ist = 0;                      
  idt[index].zero0 = 0;                      
  idt[index].type = type;                      
  idt[index].zero1 = 0;                      
  idt[index].dpl = 0;                      
  idt[index].p = 1;                      
  idt[index].offset1 = (uint16_t) (base >> 16);     
  idt[index].offset2 = (uint32_t) (base >> 32);     
  idt[index].reserved = 0;
}

void idt_init(void)
{
  uint16_t i;
  for (i = 0; i < IDT_ENTRY_NR; i++)
    idt_set_entry(i, INTERRUPT_GATE_TYPE, int_table[i], idt64);

  idtr.limit = IDT_ENTRY_NR * sizeof(idt_entry) - 1;
  idtr.base = (uint64_t) idt64;

  __asm__ volatile("lidt %0" : : "m" (idtr) : "memory");
}

void exp_handler(uint64_t irq, uint64_t *regs)
{
  printf("exception %u\n", irq);
  while (1);
}

void isr_handler(uint64_t irq)
{
  printf("interrupt %u\n", irq);
  //TODO
  //lapic_eoi();
  while (1);
}

void pit_init(void)
{
  outb(PIT_CMD, 0x34);            /* 8254 (control word) - channel 0, mode 2 */

  //TODO: set to one-shot
  /* Set interval timer to interrupt once every 1/HZth second */
  outb(PIT_CHANNEL0, (PIT_FREQ / HZ) & 0xFF);  /* channel 0 low byte */
  outb(PIT_CHANNEL0, (PIT_FREQ / HZ) >> 8);    /* channel 0 high byte */
}

void pic_init(void)
{
  /* 
   * Remap IRQs to int 0x20-0x2F
   * Need to set initialization command words (ICWs) and
   * operation command words (OCWs) to PIC master/slave
   */

  /* Master PIC */
  outb(PIC1_COMMAND, 0x11);            /* 8259 (ICW1) - xxx10x01 */
  outb(PIC1_DATA, PIC1_BASE_IRQ);   /* 8259 (ICW2) - set IRQ0... to int BASE_IRQ... */
  outb(PIC1_DATA, 0x04);            /* 8259 (ICW3) - connect IRQ2 to slave 8259 */
  outb(PIC1_DATA, 0x0D);            /* 8259 (ICW4) - Buffered master, normal EOI, 8086 mode */

  /* Slave PIC */
  outb(PIC2_COMMAND, 0x11);            /* 8259 (ICW1) - xxx10x01 */
  outb(PIC2_DATA, PIC2_BASE_IRQ);   /* 8259 (ICW2) - set IRQ8...to int BASE_IRQ... */
  outb(PIC2_DATA, 0x02);            /* 8259 (ICW3) - slave ID #2 */
  outb(PIC2_DATA, 0x09);            /* 8259 (ICW4) - Buffered slave, normal EOI, 8086 mode */
}

void interrupt_init(void)
{
  idt_init();
  pic_init();
  pit_init();
}
