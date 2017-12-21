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

void physical_free_range(uint64_t begin, size_t length)
{
  bitmap64_set_range(mm_table, begin, length);
}

void physical_take_range(uint64_t begin, size_t length)
{
  bitmap64_clear_range(mm_table, begin, length);
}

void physical_set_limit(uint64_t limit)
{
  mm_limit = limit;
}
