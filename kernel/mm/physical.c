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
#include "utils/spinlock.h"
#include "utils/screen.h"


static uint64_t mm_table[MM_TABLE_MAX] ALIGNED(PG_SIZE) = {0};
static uint64_t mm_limit = 0, entry_end = 0;
static spinlock_t phy_lock = SPINLOCK_UNLOCKED;

/*
 * begin: start physical address
 * length: in bytes
 */
void physical_free_range(uint64_t begin, uint64_t length)
{
  uint64_t remain = begin % PG_SIZE;
  uint64_t end_remain = (begin + length) % PG_SIZE;

  spin_lock(&phy_lock);

  if (remain == 0)
    bitmap64_set_range(mm_table, begin >> PG_BITS,
                       (length - end_remain) >> PG_BITS);
  else
    bitmap64_set_range(mm_table, (begin >> PG_BITS) + 1,
                       (length - (PG_SIZE - remain) - end_remain) >> PG_BITS);

  spin_unlock(&phy_lock);
}

void physical_take_range(uint64_t begin, uint64_t length)
{
  uint64_t remain = begin % PG_SIZE;
  uint64_t end_remain = (begin + length) % PG_SIZE;

  spin_lock(&phy_lock);

  if (end_remain == 0)
    bitmap64_clear_range(mm_table, begin >> PG_BITS, (length + remain) >> PG_BITS);
  else
    bitmap64_clear_range(mm_table, begin >> PG_BITS,
                         (length + remain + PG_SIZE - end_remain) >> PG_BITS);

  spin_unlock(&phy_lock);
}

bool physical_is_free(uint64_t addr)
{
  return bitmap64_check_bit(mm_table, addr >> PG_BITS);
}

void physical_set_limit(uint64_t limit)
{
  spin_lock(&phy_lock);

  /* last available page + 1 */
  mm_limit = limit >> PG_BITS;
  entry_end = mm_limit >> 6;
  if ((mm_limit & 63) > 0)
    entry_end++;

  spin_unlock(&phy_lock);
}

/* mm_table checking */
void physical_check_table(void)
{
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
}

/* find the next free memory range after (including) 'current' */
void physical_next_free_range(uint64_t current, uint64_t *start, uint64_t *size)
{
  uint64_t bit = current >> PG_BITS;
  uint64_t end = ((bit + 64 - 1) / 64) * 64;
  uint64_t i, count = 0;

  if ((bit >> 6) >= entry_end) {
    *size = 0;
    return;
  }

  for ( ; bit < end; bit++) {
    if (bitmap64_check_bit(mm_table, bit))
      count++;
    else if (count > 0) {
      *start = (bit - count) << PG_BITS;
      *size = count << PG_BITS;
      return;
    }
  }

  i = end >> 6;
  if (count == 0) {
    for ( ; i < entry_end; i++) {
      if (mm_table[i] != 0)
        break;
    }

    end = (i + 1) << 6;
    for (bit = i << 6; bit < end; bit++) {
      if (bitmap64_check_bit(mm_table, bit))
        count++;
      else if (count > 0) {
        *start = (bit - count) << PG_BITS;
        *size = count << PG_BITS;
        //printf("bit %u, end %u, count %u, i %u\n", bit, end, count, i);
        //printf("result %u, %llX\n", bitmap64_check_bit(mm_table, bit), mm_table[i]);
        return;
      }
    }
    i++;
  }

  for ( ; i < entry_end; i++) {
    if (mm_table[i] == UINT64_MAX)
      count += 64;
    else {
      *start = ((i << 6) - count) << PG_BITS;
      *size = (count + count_trailing_zeros_bit64(~mm_table[i])) << PG_BITS;
      return;
    }
  }

  *start = ((i << 6) - count) << PG_BITS;
  *size = count << PG_BITS;
}

/* return page frame address, 0 if fail */
uint64_t alloc_phys_frame(void)
{
  uint64_t i;

  spin_lock(&phy_lock);

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
      spin_unlock(&phy_lock);
      return ((i << 6) + pos) << PG_BITS;
    }
  }

  spin_unlock(&phy_lock);
  return 0;
}

/* allocate contiguous page frames */
uint64_t alloc_phys_frames(uint64_t num)
{
  uint64_t i, pos, count;

  spin_lock(&phy_lock);

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
        spin_unlock(&phy_lock);
        return pos << PG_BITS;
      }

      pos = cur + 1;
      i = pos >> 6;
    } else {
      i++;
      pos += 64;
    }
  }

  spin_unlock(&phy_lock);
  return 0;
}

/*
 * allocate contiguous page frames,
 * starting address aligned to 'align' bytes
 * (align == 2^n)
 */
uint64_t alloc_phys_frames_aligned(uint64_t num, uint64_t align)
{
  uint64_t i, pos, count;
  uint64_t pos_inc = align >> PG_BITS;
  uint64_t i_inc = pos_inc >> 6;

  spin_lock(&phy_lock);

  /* skip the first 1MB */
  i = i_inc <= 4 ? 4 : i_inc;
  pos = i << 6;

  for ( ; i < entry_end; ) {
    if (mm_table[i]) {
      uint64_t cur;
      uint64_t remain;

      while (pos < mm_limit && bitmap64_get(mm_table, pos) == 0)
        pos += pos_inc;

      cur = pos;
      count = 0;
      while (count < num && cur < mm_limit && bitmap64_get(mm_table, cur)) {
        cur++;
        count++;
      }

      if (count == num) {
        bitmap64_clear_range(mm_table, pos, num);
        spin_unlock(&phy_lock);
        return pos << PG_BITS;
      }

      if (cur >= mm_limit) {
        /* out of memory */
        spin_unlock(&phy_lock);
        return 0;
      }

      pos = cur + 1;
      remain = pos % pos_inc;
      if (remain != 0)
        pos = pos + pos_inc - remain;
      i = pos >> 6;
    } else {
      if (i_inc > 0) {
        i += i_inc;
        pos += pos_inc;
      } else {
        i += 1;
        pos += 64;
      }
    }
  }

  spin_unlock(&phy_lock);
  return 0;
}

/* return page frame address below 1MB, 0 if fail */
uint64_t alloc_phys_frame_lowmem(void)
{
  uint64_t i;

  spin_lock(&phy_lock);

  for (i = 0; i < 4; i++) {
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
      spin_unlock(&phy_lock);
      return ((i << 6) + pos) << PG_BITS;
    }
  }

  spin_unlock(&phy_lock);
  return 0;
}

void free_phys_frame(uint64_t frame)
{
  spin_lock(&phy_lock);
  bitmap64_set(mm_table, frame >> PG_BITS);
  spin_unlock(&phy_lock);
}

void free_phys_frames(uint64_t frame, uint64_t num)
{
  spin_lock(&phy_lock);
  bitmap64_set_range(mm_table, frame >> PG_BITS, num);
  spin_unlock(&phy_lock);
}
