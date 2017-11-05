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

#include <stdarg.h>
#include "types.h"

/*  The number of columns. */
#define COLUMNS                 80
/*  The number of lines. */
#define LINES                   24
/*  The attribute of an character. */
#define ATTRIBUTE               7
/*  The video memory address. */
#define VIDEO                   0xB8000

/*  Save the X position. */
static int xpos;
/*  Save the Y position. */
static int ypos;

static void _putchar(char c)
{
  volatile unsigned char *video = (unsigned char *) VIDEO;

  if (c == '\n' || c == '\r')
  {
newline:
    xpos = 0;
    ypos++;
    if (ypos >= LINES)
      ypos = 0;
    return;
  }

  *(video + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
  *(video + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;

  xpos++;
  if (xpos >= COLUMNS)
    goto newline;
}

static uint32_t base10_u32_divisors[10] = {
  1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1
};

void vprintf(void putc(char), const char *fmt, va_list args)
{
  uint32_t precision, width, mode, upper, ells;
  char padding;

  while (*fmt) {
    /* handle ordinary characters and directives */
    switch (*fmt) {
    case '\0':
      return;

    case '%':
      fmt++;
      precision = 0;
      width = 0;
      upper = 1;
      padding = ' ';
      ells = 0;
#define PRINTF_MODE_PRECISION 1
      mode = 0;
      /* handle directive arguments */
      while (*fmt) {
        switch (*fmt) {
        case 'x':
          upper = 0;

        case 'X':{
          /* hexadecimal output */
          uint64_t x;
          uint32_t i, li, print_padding = 0, print_digits = 0;
          int32_t w = (ells == 2 ? 16 : 8);

          if (ells == 2)
            x = va_arg(args, uint64_t);
          else
            x = va_arg(args, uint32_t);

          for (i = 0; i < w; i++) {
            li = (x >> (((w - 1) - i) << 2)) & 0x0F;

#define HANDLE_OPTIONS(q,max_w,end_i)                                   \
            if (q != 0 || i == end_i)                                   \
              print_digits = 1;                                         \
            if (q == 0 && !print_digits && i >= (max_w - width))        \
              print_padding = 1;                                        \
            if (q == 0 && !print_digits && i >= (max_w - precision))    \
              print_digits = 1;                                         \
            if (q == 0 && print_padding && !print_digits)               \
              putc(padding);

            HANDLE_OPTIONS(li, w, (w-1));

            if (print_digits) {
              if (li > 9)
                putc((upper ? 'A' : 'a') + li - 0x0A);
              else
                putc('0' + li);
            }
          }

          goto directive_finished;
        }  
        case 'u':{
            /* decimal output */
            uint32_t x = va_arg(args, uint32_t);
            uint32_t i, q, print_padding = 0, print_digits = 0;
            uint32_t *divisors = base10_u32_divisors;

            for (i = 0; i < 10; i++) {
              q = x / divisors[i];
              x %= divisors[i];

              HANDLE_OPTIONS(q, 10, 9);

              if (print_digits)
                putc('0' + q);
            }
            goto directive_finished;
        }  
        case 'd':{
            /* decimal output */
            int32_t x = va_arg(args, int32_t);
            uint32_t i, q, print_padding = 0, print_digits = 0;
            uint32_t *divisors = base10_u32_divisors;

            if (x < 0) {
              putc('-');
              x *= -1;
            }

            for (i = 0; i < 10; i++) {
              q = x / divisors[i];
              x %= divisors[i];

              HANDLE_OPTIONS(q, 10, 9);

              if (print_digits)
                putc('0' + q);
            }
            goto directive_finished;
        }  
        case 's':{
            /* string argument */
            char *s = va_arg(args, char *);
            if (s) {
              if (precision > 0)
                while (*s && precision-- > 0)
                  putc(*s++);
              else
                while (*s)
                  putc(*s++);
            } else {
              putc('('); 
              putc('n'); 
              putc('u'); 
              putc('l'); 
              putc('l'); 
              putc(')');
            }
            goto directive_finished;
        }  
        case '%':{
            /* single % */
            putc('%');
            goto directive_finished;
        }  
        case 'l':{
          /* "long" annotation */
          ells++;
          break;
        }
        case '.':{
          mode = PRINTF_MODE_PRECISION;
          break;
        }
        default:
          if ('0' <= *fmt && *fmt <= '9') {
            if (mode == PRINTF_MODE_PRECISION) {
              /* precision specifier */
              precision *= 10;
              precision += *fmt - '0';
            } else if (mode == 0 && width == 0 && *fmt == '0') {
              /* padding char is zero */
              padding = '0';
            } else {
              /* field width */
              width *= 10;
              width += *fmt - '0';
            }
          }
          break;
        }

        fmt++;
      }
directive_finished:
      break;
      
    default:
      /* regular character */
      putc(*fmt);
      break;
    }

    fmt++;
  }
}

void printf(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  //spinlock_lock(&screen_lock);
  vprintf(_putchar, fmt, args);
  //spinlock_unlock(&screen_lock);
  va_end(args);
}
