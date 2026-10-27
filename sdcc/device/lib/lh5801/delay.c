/*-------------------------------------------------------------------------
   delay.c - power-conscious delays for the PC-1500, built on idleTick().

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

void delayTicks(unsigned int ticks)
{
    while (ticks--)
        idleTick();
}
