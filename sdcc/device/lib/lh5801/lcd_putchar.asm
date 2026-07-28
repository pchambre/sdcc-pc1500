;--------------------------------------------------------------------------
; lcd_putchar.asm - print one character to the real PC-1500 LCD via the
; ROM's own documented graphics subroutines, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; Uses the ROM's SBR vector table (0xFF80-0xFFFE, entered via the VMJ
; opcode -- "VMJ n" pushes P and jumps to the address stored at
; 0xFF00|n) instead of a hand-rolled font/display driver: per
; /home/paul/Documents/PC1500/ASMADDR.txt's disassembly-derived vector
; table (cross-checked directly against the real ROM1.BIN, see the
; verification harness in this session's scratchpad --
; rom_print_test.cpp/rom_print_seq.cpp -- which loaded the actual ROM,
; called each vector, and rendered the resulting LCD buffer as ASCII art
; to confirm correct glyph shapes):
;
;   VMJ 0x8C (SBR8C, ROM EE1F) - "BC = Ram address of current Graphic
;       Cursor position": reads the Graphic Cursor Unit (GCU, a single
;       byte at the fixed, documented address 0x7875, raw column 0-155)
;       and computes the real display-buffer address for it into X
;       (LH5801's BC-equivalent register pair).
;   VMJ 0x8A (SBR8A, ROM ED5B) - "Print char A at pos BC on LCD": look
;       up the ROM's own built-in font for character A and draw it at
;       the address in X, using the ROM's own font table -- confirmed
;       directly (not a guess) by loading the real ROM and rendering the
;       resulting buffer: 'H' produced a correct 5-column H shape
;       (full-height / mid-row-only x3 / full-height), 'i' a correct
;       dot-gap-stem shape centered in its cell. Auto-advances X across
;       the glyph's own width (confirmed: X advanced by 12 bytes = 6
;       columns for one character) but does NOT touch the persistent GCU
;       variable.
;   VMJ 0x8E (SBR8E, ROM EDB1) - "Increment GCU and test END OF SCREEN":
;       advances GCU by exactly ONE column and sets the carry flag if
;       GCU now exceeds 155. This is a per-COLUMN primitive, not
;       per-character -- confirmed directly: calling it once per
;       character left the next glyph overlapping the previous one
;       (GCU only advanced 0->1->2); calling it 6 times per character
;       (matching the font's own confirmed 5-column-glyph + 1-column-gap
;       cell width) produced correctly spaced, non-overlapping
;       characters.
;
; Deliberately NOT reimplementing font rendering or the nibble-packed
; 7600H-77FFH buffer format ourselves (see docs/pc1500_hardware_reference.md
; "LCD" section for what that would entail) -- the real font table and
; column-writing logic already exist in every real PC-1500's ROM1, so
; calling into it costs a handful of bytes here instead of duplicating a
; multi-hundred-byte font table and driver in every compiled program.
;--------------------------------------------------------------------------

	.module lcd_putchar
	.area CODE

	.globl _lcdPutChar
	.globl _lcdPutChar_PARM_1

;-----------------------------------------------------------------
; void lcdPutChar(unsigned char c);
;
; Non-reentrant C calling convention (this port's default): the caller
; writes the argument directly to _lcdPutChar_PARM_1 before calling,
; confirmed via a standalone compile of a plain call site -- no IPUSH
; involved, since this function is not reentrant/variadic.
;-----------------------------------------------------------------
_lcdPutChar:
	vmj	0x8C		; X = real display-buffer address for the
				; current Graphic Cursor position
	lda	(_lcdPutChar_PARM_1)
	vmj	0x8A		; draw the character at X using the ROM's
				; own font table
	vmj	0x8E		; advance the Graphic Cursor Unit by one
	vmj	0x8E		; column per call -- 6 calls total, matching
	vmj	0x8E		; this ROM font's confirmed 5-column glyph +
	vmj	0x8E		; 1-column inter-character gap cell width.
	vmj	0x8E
	vmj	0x8E
	rtn

	.area DATA
_lcdPutChar_PARM_1:
	.ds 1
