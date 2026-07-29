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

/* --- CE-150 cassette tape send (device/lib/lh5801/tape_send_char.asm) -
   VMJ 0xA8 (SBRA8) -- unlike every other function in this header, this
   one's target lives inside a *plug-in module's* ROM (0x8000-0xBFFF,
   selected by the CPU's PU/PV flip-flops), not the base system ROM --
   it only works with a CE-150 (or possibly CE-158) module actually
   present and selected. See pc1500emu's Bus::RomModule for how to load
   one. Confirmed by direct emulation: sends the byte's low nibble as
   (0xF0 | nibble), then its high nibble the same way, then a fixed
   (0xF0),(0xF0) trailer. There is no matching tapeReadChar() -- the
   real ROM routine for that (VMJ 0xA4) is a pulse-width timing loop
   against the cassette input line, which pc1500emu doesn't model at
   all (no simulated tape signal), so it can't be usefully wrapped or
   tested yet. */
extern void tapeSendChar(unsigned char c);

/* --- PC-1500 native numeric type and math library --------------------
   The ROM's own 8-byte floating-point format (confirmed both from the
   PC-2 Assembly Language manual and empirically, byte-for-byte, against
   real ROM1.BIN this session):

     byte 0: exponent, signed 8-bit two's complement, power of 10
     byte 1: mantissa sign (0x00 = +, 0x80 = -)
     bytes 2-6: mantissa, packed BCD, 2 digits/byte, normalized as a
                single digit before the decimal point (d.ddddddddd,
                10 significant digits)
     byte 7: always 0x00

   None of these ROM addresses are documented SBRxx vector-table entries
   -- they're plain internal addresses, same caveat as tapeSendChar's.
   Every routine here only works correctly on data actually stored at
   fixed RAM addresses (0x7A00, and 0x7A10 for a second operand) --
   confirmed directly that pointing at an arbitrary caller buffer does
   NOT work -- so every wrapper below copies its pc1500_float_t
   argument(s) in before calling the ROM routine and copies the
   (possibly modified) result back out afterward. See
   pc1500_fp_helpers.asm for that shared copy-in/copy-out logic. */
typedef unsigned char pc1500_float_t[8];

/* Captured from the ROM's own UH error-code register immediately after
   every call below (confirmed: divide by zero leaves a nonzero code
   there). Only "zero == no error" is confirmed; individual nonzero
   values aren't otherwise decoded yet. */
extern unsigned char pc1500Error(void);

/* string <-> native format, via the ROM's own STR$ (device/lib/lh5801/
   pc1500_float_to_str.asm). There is no matching pc1500StrToFloat(): the
   ROM's VAL routine (0xD9D7) does not behave as a simple isolated
   callable routine the way STR$ does (confirmed: it writes a scratch
   byte immediately *before* its input pointer and returns almost
   instantly regardless of input, suggesting it expects to be entered
   from within the tokenizer/expression-evaluator's own state, like the
   file-name routines this session already declined to wrap for the
   same reason) -- not attempted further. */
extern void pc1500FloatToStr(const pc1500_float_t v, char *out);

/* Two-operand arithmetic, in place: a is both an input and the result. */
extern void pc1500Add(pc1500_float_t a, const pc1500_float_t b); /* a += b */
extern void pc1500Sub(pc1500_float_t a, const pc1500_float_t b); /* a -= b (no direct ROM
                                                                     subtract was found; implemented
                                                                     as negate-then-add) */
extern void pc1500Mul(pc1500_float_t a, const pc1500_float_t b); /* a *= b */
extern void pc1500Div(pc1500_float_t a, const pc1500_float_t b); /* a /= b */

/* Single-operand math, in place -- all confirmed against the real ROM
   this session, all sharing the identical calling convention. */
extern void pc1500Sqrt(pc1500_float_t v);
extern void pc1500Sin(pc1500_float_t v);
extern void pc1500Cos(pc1500_float_t v);
extern void pc1500Tan(pc1500_float_t v);
extern void pc1500Log(pc1500_float_t v);  /* base 10 */
extern void pc1500Ln(pc1500_float_t v);   /* natural log */
extern void pc1500Exp(pc1500_float_t v);
extern void pc1500Atn(pc1500_float_t v);
extern void pc1500Asn(pc1500_float_t v);
extern void pc1500Acs(pc1500_float_t v);
extern void pc1500Abs(pc1500_float_t v);
extern void pc1500Int(pc1500_float_t v);
extern void pc1500Sgn(pc1500_float_t v);
extern void pc1500Deg(pc1500_float_t v);
extern void pc1500Dms(pc1500_float_t v);

#endif /* __PC1500_H */
