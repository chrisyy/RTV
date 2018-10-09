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

#include "utils/string.h"

uint64_t strlen(const char *str)
{
  uint64_t len = 0;
  while (str[len] != '\0')
    len++;
  return len;
}

int strncmp(const char *s1, const char *s2, uint64_t n)
{
  unsigned char c1 = 0, c2 = 0;

  while (n > 0) {
    c1 = (unsigned char) *s1;
    c2 = (unsigned char) *s2;
    if (c1 == 0 || c1 != c2)
      return c1 - c2;
    n--;
    s1++;
    s2++;
  }

  return c1 - c2;
}
