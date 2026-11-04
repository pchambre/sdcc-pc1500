/*-------------------------------------------------------------------------
   printf_large.c - the LH5801 port's _print_format(), the formatter under
   printf(), sprintf(), vprintf() and vsprintf().

   Paul Chambre, 2026

   What device/lib/printf_large.c does on this port, with the same
   results, in less code (2026-11-04: 2833 bytes there, a third of the
   EDIT example's image). The generic one keeps an int's digits in packed
   BCD, two to a byte, through a union of the value and its bytes; here
   they are one to a byte in a small buffer, and one loop works out a
   digit of a 32-bit value in any radix with shifts and a subtraction (no
   division routine linked in).

   The same conversions, flags and quirks:
     %d %i %u %o %x %X %c %s %p %%, case-insensitive but for the letters
     in hex (%X upper case); flags - + space and a leading 0 for zero
     padding; a width; a precision for %s; modifiers l (long), b (char),
     and h j t z (ignored). %f prints <NO FLOAT>, taking the float
     (SDCC's printf only formats floats on ds390). %p is 0x and four hex
     digits. A string's padding counts its whole length, even when a
     precision cuts it. Anything else after % is printed as itself.
   Unlike the generic one: %e and %g take their float and print
   <NO FLOAT> as %f does (it printed the letter and left the float to the
   next conversion), a % at the very end of the format stops it (it
   printed the terminating 0 and read on), and a width or precision is
   at most 255 (a byte: the display is 26 cells).

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   As a special exception, if you link this library with other files,
   some of which are compiled with SDCC, to produce an executable,
   this library does not by itself cause the resulting executable to
   be covered by the GNU General Public License. This exception does
   not however invalidate any other reasons why the executable file
   might be covered by the GNU General Public License.
-------------------------------------------------------------------------*/

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#define LEFT   0x01
#define ZERO   0x02
#define PLUS   0x04
#define SPACE  0x08
#define CHAR   0x10
#define LONG   0x20
#define LOWER  0x40
#define SIGNED 0x80           /* %d %i; after the sign is taken: negative */
#define NONE   0xFF           /* no precision */

static pfn_outputchar outFn;
static void *outP;
static int count;

static void put(char c)
{
    outFn(c, outP);
    count++;
}

static void pad(char c, unsigned char n)
{
    while (n--)
        put(c);
}

static void digit(unsigned char n, unsigned char lower)
{
    char c = n + '0';

    if (c > '9') {
        c += 'A' - '0' - 10;
        if (lower)
            c += 'a' - 'A';
    }
    put(c);
}

int _print_format(pfn_outputchar pfn, void *pvoid, const char *format, va_list ap)
{
    char c;

    outFn = pfn;
    outP = pvoid;
    count = 0;
    while ((c = *format++)) {
        unsigned char flags, radix, len;
        unsigned char width, decimals;   /* decimals NONE: no precision */
        long l;
        unsigned long u;
        char buf[11];

        if (c != '%') {
            put(c);
            continue;
        }
        flags = 0;
        radix = 0;
        width = 0;
        decimals = NONE;
spec:
        c = *format++;
        if (!c)
            break;
        if (c == '%') {
            put(c);
            continue;
        }
        if (c >= '0' && c <= '9') {
            c -= '0';
            if (decimals == NONE) {
                width = (width << 3) + (width << 1) + c;
                if (!width)
                    flags |= ZERO;
            } else
                decimals = (decimals << 3) + (decimals << 1) + c;
            goto spec;
        }
        if (c == '.') {
            if (decimals == NONE)
                decimals = 0;
            goto spec;
        }
        if (c >= 'a' && c <= 'z') {      /* the last letter decides: %lX is upper */
            c &= 0xDF;
            flags |= LOWER;
        } else
            flags &= ~LOWER;
        switch (c) {
        case '-':
            flags |= LEFT;
            goto spec;
        case '+':
            flags |= PLUS;
            goto spec;
        case ' ':
            flags |= SPACE;
            goto spec;
        case 'B':
            flags |= CHAR;
            goto spec;
        case 'H':
        case 'J':
        case 'T':
        case 'Z':
            goto spec;
        case 'L':
            flags |= LONG;
            goto spec;
        case 'C':
            put((flags & CHAR) ? va_arg(ap, char) : va_arg(ap, int));
            continue;
        case 'S':
            {
                const char *s = va_arg(ap, const char *);
                unsigned int n = strlen(s);

                if (!(flags & LEFT) && n < width)
                    pad(' ', width - (unsigned char)n);
                while ((c = *s++) && (decimals == NONE || decimals--))
                    put(c);
                if ((flags & LEFT) && n < width)
                    pad(' ', width - (unsigned char)n);
            }
            continue;
        case 'P':
            {
                unsigned int p = (unsigned int)va_arg(ap, const char *);

                put('0');
                put('x');
                for (len = 0; len < 4; len++, p <<= 4)
                    digit(p >> 12, flags & LOWER);
            }
            continue;
        case 'D':
        case 'I':
            flags |= SIGNED;
            radix = 10;
            break;
        case 'O':
            radix = 8;
            break;
        case 'U':
            radix = 10;
            break;
        case 'X':
            radix = 16;
            break;
        case 'F':
        case 'E':
        case 'G':
            (void)va_arg(ap, float);
            {
                const char *s = "<NO FLOAT>";

                while ((c = *s++))
                    put(c);
            }
            continue;
        default:
            put(c);
            continue;
        }

        /* an integer in `radix` */
        if (flags & CHAR) {
            l = va_arg(ap, char);
            if (!(flags & SIGNED))
                l &= 0xFF;
        } else if (flags & LONG)
            l = va_arg(ap, long);
        else {
            l = va_arg(ap, int);
            if (!(flags & SIGNED))
                l &= 0xFFFF;
        }
        if (flags & SIGNED) {
            if (l < 0)
                l = -l;
            else
                flags &= ~SIGNED;
        }
        u = l;
        len = 0;
        do {                    /* u /= radix, the remainder to buf */
            unsigned char r = 0, i = 32;

            do {
                r = (r << 1) | ((unsigned char)(u >> 24) >> 7);
                u <<= 1;
                if (r >= radix) {
                    r -= radix;
                    u |= 1;
                }
            } while (--i);
            buf[len++] = r;
        } while (u);

        if (!width)
            width = 1;
        if (!(flags & (ZERO | LEFT)))
            while (width > len + 1) {
                put(' ');
                width--;
            }
        if (flags & SIGNED) {
            put('-');
            width--;
        } else if (flags & PLUS) {
            put('+');
            width--;
        } else if (flags & SPACE) {
            put(' ');
            width--;
        }
        if (!(flags & LEFT)) {
            if (width > len)
                pad((flags & ZERO) ? '0' : ' ', width - len);
        } else
            width = width > len ? width - len : 0;
        while (len)
            digit(buf[--len], flags & LOWER);
        if (flags & LEFT)
            pad(' ', width);
    }
    return count;
}
