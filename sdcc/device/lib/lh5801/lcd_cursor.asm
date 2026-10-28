;--------------------------------------------------------------------------
; lcd_cursor.asm - BASIC's CURSOR, and the display-reverse routine from
; the Technical Reference Manual, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; lcdSetCursor() sets the Graphic Cursor Unit (GCU, 7875H, the raw
; column 0-155 that lcdPutChar() draws at -- see lcd_putchar.asm) to a
; character cell, 6 columns per cell like the ROM's own font, so cells
; 0-25 match BASIC's CURSOR 0-25.
;
; lcdInvert() inverts every dot of the display: both 78-byte column
; spans, 7600H-764DH and 7700H-774DH (pc1500emu's lcd.h has the layout),
; exactly as the manual's own "display reverse" example does. Calling it
; again puts the display back.
;--------------------------------------------------------------------------

	.module lcd_cursor
	.area CODE

	.globl _lcdSetCursor
	.globl _lcdInvert

;-----------------------------------------------------------------
; void lcdSetCursor(unsigned char cell);  -- cell 0-25
;-----------------------------------------------------------------
_lcdSetCursor:			; cell in A
	shl
	sta	yl			; cell * 2
	shl				; cell * 4
	rec
	adc	yl			; cell * 6
	sta	(0x7875)
	rtn

;-----------------------------------------------------------------
; void lcdInvert(void);
;-----------------------------------------------------------------
_lcdInvert:
	ldi	xh, 0x76
	ldi	xl, 0x00
lcdInvert_loop:
	lda	(x)
	eai	a, 0xFF
	sin	x
	lda	xl
	cpi	a, 0x4E
	bzr	lcdInvert_loop
	lda	xh
	cpi	a, 0x77
	bzs	lcdInvert_done
	ldi	xh, 0x77
	ldi	xl, 0x00
	bch	lcdInvert_loop
lcdInvert_done:
	rtn

	.area DATA
