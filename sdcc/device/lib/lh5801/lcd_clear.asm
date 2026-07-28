;--------------------------------------------------------------------------
; lcd_clear.asm - clear the real PC-1500 LCD via the ROM's own routine.
;
; Paul Chambre, 2026
;
; VMJ 0xF2 (SBRF2, ROM EE71 per /home/paul/Documents/PC1500/ASMADDR.txt's
; vector table -- "SBRF2 address EE71 SBR F2 Clear LCD"). Directly
; disassembled and confirmed: zeroes the 78-byte dot-matrix span at both
; 0x764D-and-down and 0x774D-and-down via LOP (loop-decrement), i.e. the
; real column data for both display halves -- see
; [[pc1500-rom-lcd-print-routine]] for the same don't-duplicate-ROM
; rationale as lcd_putchar.asm.
;
; SBRF2 does NOT touch the Graphic Cursor Unit (0x7875) -- confirmed
; directly by disassembly, no write to that address anywhere in its
; body -- so this wrapper resets it to 0 itself afterward, matching what
; a caller actually wants from "clear the screen": blank display *and*
; printing resumes from column 0.
;--------------------------------------------------------------------------

	.module lcd_clear
	.area CODE

	.globl _lcdClear

;-----------------------------------------------------------------
; void lcdClear(void);
;-----------------------------------------------------------------
_lcdClear:
	vmj	0xF2
	ldi	a, 0x00
	sta	(0x7875)
	rtn
