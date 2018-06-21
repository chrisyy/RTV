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
static uint64_t mm_limit = 0, entry_end = 0;

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
  /* last available page + 1 */
  mm_limit = limit >> PG_BITS;
  entry_end = mm_limit >> 6;
  if ((mm_limit & 63) > 0)
    entry_end++;

#if 0
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

/* return page frame address, 0 if fail */
uint64_t alloc_phys_frame(void)
{
  uint64_t i;

  /* skip the first 1MB */
  for (i = 4; i < entry_end; i++) {
    if (mm_table[i]) {
      uint8_t pos = 0;
      uint64_t data = mm_table[i];
      data = data & (-data);
      data >>= 1;
      while (data) {
        data >>= 1;
        pos++;
      }

      clear_bit64(&mm_table[i], pos);
      return (i * 64 + pos) * PG_SIZE;
    }
  }

  return 0;
}

/* allocate contiguous page frames */
uint64_t alloc_phys_frames(uint64_t num)
{
  uint64_t i, pos, count;

  /* skip the first 1MB */
  for (i = 4, pos = 256; i < entry_end; ) {
    if (mm_table[i]) {
      uint64_t cur;

      while (bitmap64_get(mm_table, pos) == 0)
        pos++;
        
      cur = pos;
      count = 0;
      while (count < num && bitmap64_get(mm_table, cur)) {
        cur++;
        count++;
      }

      if (count == num) {
        bitmap64_clear_range(mm_table, pos, num);
        return pos * PG_SIZE;
      }

      pos = cur + 1;
      i = pos >> 6;
    } else {
      i++;
      pos += 64;
    }
  }

  return 0;
}

void free_phys_frame(uint64_t frame)
{
  bitmap64_set(mm_table, frame >> PG_BITS);
}

void free_phys_frames(uint64_t frame, uint64_t num)
{
  bitmap64_set_range(mm_table, frame >> PG_BITS, num);
}
