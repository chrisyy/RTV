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

#include "smp.h"
#include "mm/physical.h"
#include "helper.h"
#include "apic.h"
#include "percpu.h"

extern uint8_t status_code[], ap_stack_ptr[];
extern uint8_t ap_boot_start[];

#define BOOT_STATUS() \
(*((volatile uint64_t *) (SMP_BOOT_ADDR + status_code - ap_boot_start)))

#define BOOT_STACK() \
(*((volatile uint64_t *) (SMP_BOOT_ADDR + ap_stack_ptr - ap_boot_start)))

#define LOOPS_TO_WAIT 100000000

static inline void smp_spin(uint64_t num)
{
  uint64_t i;
  for (i = 0; i < num; i++)
    __asm__ volatile("");
}

bool smp_boot_cpu(uint8_t lapic)
{
  uint64_t stack;
  uint64_t cnt;
  uint8_t *va;
  
  stack = alloc_phys_frame();
  if (stack == 0)
    panic(__func__, "running out of memory");
  va = (uint8_t *) vm_map_page(stack, PGT_P | PGT_RW);
  if (!va)
    panic(__func__, "running out of va mapping");

  /* status code is reset to 0 */
  BOOT_STATUS() = 0;
  /* stack for AP */
  BOOT_STACK() = (uint64_t) (va + PG_SIZE);

  lapic_send_ipi(lapic, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);

  //TODO: spin 10 ms
  smp_spin(10000000);

  lapic_send_ipi(lapic, LAPIC_ICR_DM_SIPI | ((SMP_BOOT_ADDR >> 12) & 0xFF));

  /* check for successful start */
  //TODO: use time instead of iterations
  cnt = 0;
  while (!BOOT_STATUS() && cnt++ < LOOPS_TO_WAIT) {
     __asm__ volatile("");
  }
  if (cnt >= LOOPS_TO_WAIT) 
    return false;

  return true;
}

#undef LOOPS_TO_WAIT

void ap_main(void)
{
  tss_t *tss_ptr;
  uint64_t stack;
  uint16_t selector;

  percpu_init();

  lapic_init();

  //lapic_send_ipi(lapic_get_phys_id(0), LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_NMI);

  tss_ptr = percpu_pointer(get_pcpu_id(), cpu_tss);
  __asm__ volatile("movq %%rsp, %0" : "=a" (stack) : : );
  stack &= PG_MASK;
  stack += PG_SIZE;
  tss_ptr->rsp[0] = stack;
  selector = alloc_tss_desc(tss_ptr);
  load_tr(selector);

  while(1);
}
