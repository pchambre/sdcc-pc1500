;--------------------------------------------------------------------------
; expansion/lcd_clear.asm - lcdClear() for build-lh5801.sh --expansion: as
; ../lcd_clear.asm, and BASIC's text buffer that lcdPutChar() writes to
; (expansion/lcd_putchar.asm) is blanked too: 26 spaces, then the end
; (0DH). The Screen pane reports nothing for a blank line.
;
; Paul Chambre, 2026
;--------------------------------------------------------------------------

	.module lcd_clear
	.area CODE

	.globl _lcdClear
	.globl __lcd_edit
	.globl __lcd_fill

_lcdClear:
	vmj	0xF2
	ldi	a, 0x00
	sta	(0x7875)
	lda	(__lcd_edit)	; 7B60H, or 7BB0H while editing
	rec
	adi	a, #0x60
	sta	xl
	ldi	a, 0x20

; A in 7Bxxh (XL) and the 25 bytes after, then 0DH: a line's 26 cells and
; its end. (lcdEditEnd() ends BASIC's line buffer with it.)
__lcd_fill:
	ldi	xh, 0x7B
	ldi	ul, 25
1$:
	sin	x
	lop	ul, 1$
	ldi	a, 0x0D
	sta	(x)
	rtn
