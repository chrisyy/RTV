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

#include "utils/bits.h"

/* 
 * within a uint64_t, bit(n) is the least significant bit, 
 * bit(n + 63) is the most significant bit 
 */
void bitmap64_set_range(uint64_t *map, uint64_t begin, uint64_t length)
{
  uint64_t *ptr = map + (begin >> 6);
  uint8_t bits = 64 - (begin & 63);
  uint64_t end = begin + length;
  uint64_t mask = 0;
  mask = (~mask) << (begin & 63);

  while (length >= bits) {
    *ptr |= mask;
    length -= bits;
    bits = 64;
    mask = 0;
    mask = ~mask;
    ptr++;
  }

  if (length > 0) {
    mask &= (~((uint64_t) 0)) >> ((-end) & 63);
    *ptr |= mask;
  }
}

void bitmap64_clear_range(uint64_t *map, uint64_t begin, uint64_t length)
{
  uint64_t *ptr = map + (begin >> 6);
  uint8_t bits = 64 - (begin & 63);
  uint64_t end = begin + length;
  uint64_t mask = 0;
  mask = (~mask) << (begin & 63);

  while (length >= bits) {
    *ptr &= ~mask;
    length -= bits;
    bits = 64;
    mask = 0;
    mask = ~mask;
    ptr++;
  }

  if (length > 0) {
    mask &= (~((uint64_t) 0)) >> ((-end) & 63);
    *ptr &= ~mask;
  }
}

/*
 * index: bit index to the map
 * return true if bit set
 */
bool bitmap64_check_bit(uint64_t *map, uint64_t index)
{
  uint64_t *ptr = map + (index >> 6);
  uint64_t mask = 1 << (index % 64);
  if ((*ptr) & mask)
    return true;
  else
    return false;
}
