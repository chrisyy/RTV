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

#include "mm/physical.h"
#include "utils/bits.h"

static uint64_t mm_table[MM_TABLE_MAX] ALIGNED(PG_SIZE);
static uint64_t mm_limit;

void physical_free_range(uint64_t begin, uint64_t length)
{
  uint64_t remain = begin % PG_SIZE;
  uint64_t end_remain = (begin + length) % PG_SIZE;
  if (remain == 0)
    bitmap64_set_range(mm_table, begin >> PG_BITS, 
                       (length - end_remain) >> PG_BITS);
  else
    bitmap64_set_range(mm_table, (begin >> PG_BITS) + 1, 
                       (length - (PG_SIZE - remain) - end_remain) >> PG_BITS);
}

void physical_take_range(uint64_t begin, uint64_t length)
{
  uint64_t remain = begin % PG_SIZE;
  uint64_t end_remain = (begin + length) % PG_SIZE;
  if (end_remain == 0)
    bitmap64_clear_range(mm_table, begin >> PG_BITS, (length + remain) >> PG_BITS);
  else
    bitmap64_clear_range(mm_table, begin >> PG_BITS, 
                         (length + remain + PG_SIZE - end_remain) >> PG_BITS);
}

void physical_set_limit(uint64_t limit)
{
  mm_limit = limit >> PG_BITS;

#if 1
  /* mm_table checking */
  uint64_t i;
  int flag = 0;
  for (i = 0; i < MM_TABLE_MAX; i++) {
    if (mm_table[i] != UINT64_MAX && flag != 0) {
      printf("mm_table[0x%llX]: %llX\n", i, mm_table[i]);
      if (!(mm_table[i] & ((uint64_t) 0x1 << 63)))
        flag = 0;
    } else if (mm_table[i] != 0 && flag == 0) {
      printf("mm_table[0x%llX]: %llX\n", i, mm_table[i]);
      if (mm_table[i] & ((uint64_t) 0x1 << 63))
        flag = 1;
    }
  }
#endif
}
