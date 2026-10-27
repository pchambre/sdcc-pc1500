/*-------------------------------------------------------------------------
   lcd_string.c - strings on the PC-1500 LCD, and BASIC-INPUT-style line
   entry, built on the ROM-calling primitives in this directory.

   Paul Chambre, 2026

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   As a special exception, if you link this library with other files,
   some of which are compiled with SDCC, to produce an executable,
   this library does not by itself cause the resulting executable to
   be covered by the GNU General Public License.
-------------------------------------------------------------------------*/

#include <pc1500.h>

void lcdPutString(const char *s)
{
    while (*s)
        lcdPutChar(*s++);
}

/* Typed characters are echoed from the current cursor and kept while
   the line has room on screen (the Graphic Cursor Unit at 7875H is the
   raw column, 6 per character); CL clears what's been typed; ENTER ends
   the line. */
unsigned char lcdInputLine(char *buf, unsigned char max)
{
    unsigned char start = PC1500_GCU;
    unsigned char n = 0;
    unsigned char i, k;

    for (;;) {
        k = keyWait();
        if (k == PC1500_KEY_ENTER)
            break;
        if (k == PC1500_KEY_BREAK) {
            buf[0] = 0;
            return PC1500_INPUT_BREAK;
        }
        if (k == PC1500_KEY_CL) {
            PC1500_GCU = start;
            for (i = 0; i < n; i++)
                lcdPutChar(' ');
            PC1500_GCU = start;
            n = 0;
        } else if (k >= 0x20 && k < 0x7F && n < max && PC1500_GCU <= 150) {
            buf[n++] = k;
            lcdPutChar(k);
        }
    }
    buf[n] = 0;
    return n;
}
