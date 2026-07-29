/*-------------------------------------------------------------------------
   pc1500.h - Sharp PC-1500 ROM/hardware call surface for the LH5801 port.

   Paul Chambre, 2026

   Every function declared here is a thin, hand-written assembly shim
   (device/lib/lh5801/*.asm) that either calls straight into the real
   ROM's own SBRxx vector table (0xFF80-0xFFFE, entered via the VMJ
   opcode -- see /home/paul/Documents/PC1500/ASMADDR.txt) or reads a
   fixed hardware I/O register directly. Nothing here duplicates ROM
   functionality (e.g. no reimplemented character font) -- the ROM is
   always present on a real PC-1500, so we call into it instead. Each
   .asm file documents exactly which SBRxx/ROM address or I/O register
   backs its function, and why.

   This is a phase-1 backend (see src/lh5801/gen.c's own header comment):
   there is no I/O-space (__sfr) support yet, so any routine that needs
   ME1 (I/O space) accesses it via a hand-written shim, not a plain C
   pointer dereference.

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   As a special exception, if you link this library with other files,
   some of which are compiled with SDCC, to produce an executable,
   this library does not by itself cause the resulting executable to
   be covered by the GNU General Public License. This exception does
   not however invalidate any other reasons why the executable file
   might be covered by the GNU General Public License.
-------------------------------------------------------------------------*/

#ifndef __PC1500_H
#define __PC1500_H   1

/* --- LCD (device/lib/lh5801/lcd_putchar.asm, lcd_clear.asm) ----------
   VMJ 0x8A/0x8C/0x8E (SBR8A/SBR8C/SBR8E) print one character at the
   current Graphic Cursor Unit position and advance it; VMJ 0xF2 (SBRF2)
   clears the display. lcdClear() also resets the GCU itself (0x7875),
   since SBRF2 alone does not. */
extern void lcdPutChar(unsigned char c);
extern void lcdClear(void);

/* --- ON key (device/lib/lh5801/onkey.asm) -----------------------------
   Reads Port B bit 7 directly (I/O space, register 0x0F) rather than
   going through the ROM's BREAK-key test (SBRA6) -- pc1500emu's ON key
   line isn't wired to the IF register or an interrupt, so the ROM's own
   break machinery never sees it (see onkey.asm's own comment). Returns
   non-zero while the key is held down. */
extern unsigned char onKeyPressed(void);

/* --- Power-conscious idle wait (device/lib/lh5801/idle.asm) -----------
   AM0/SIE/HLT: the same idiom the ROM's own idle loop (0xE2A2) uses to
   halt the CPU until the next timer interrupt, instead of busy-waiting.
   Not a ROM subroutine call (0xE2A2 itself isn't a callable routine --
   it doesn't end in RTN and branches into BASIC's own dispatch loop),
   just 3 raw CPU instructions -- nothing here duplicates ROM code. */
extern void idleTick(void);

#endif /* __PC1500_H */
