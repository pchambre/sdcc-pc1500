;--------------------------------------------------------------------------
; expansion/lcd_putchar.asm - lcdPutChar() for build-lh5801.sh
; --expansion: as ../lcd_putchar.asm, and the character also goes to
; BASIC's own text buffer, so the expansion module's Screen pane (MCONF
; BLDISP) shows the program's display as it shows BASIC's.
;
; Paul Chambre, 2026
;
; The Screen pane (PC1500-PSOC5 RP2350/screen.c) reads BASIC's buffers off
; the bus, not the LCD's dots: PRINT's output at 7B60H-7BAFH, the line
; being typed at 7BB0H-7BFFH, with the mode at 7880H and the cursor's LCD
; address at 787EH/787FH. Each burst of writes is reported once nothing
; has been written for 20 ms -- a line of output, or the typed line as it
; stands. So the character goes in at its cell (the Graphic Cursor / 6) of
; the output buffer; between lcdEditBegin() and lcdEditEnd(), of the line
; buffer instead, and the cursor's address after it, as ROM1's own line
; editor keeps them. lcdClear() blanks the buffer's 26 cells.
;--------------------------------------------------------------------------

	.module lcd_putchar
	.area CODE

	.globl _lcdPutChar
	.globl __lcd_edit
	.globl __lcd_cursor
	.globl __lcd_draw

GCU = 0x7875			; the Graphic Cursor (0-155)
CURSOR_H = 0x787E		; the cursor's LCD address, as ROM1's editor
CURSOR_L = 0x787F		; keeps it

;-----------------------------------------------------------------
; void lcdPutChar(unsigned char c);
;-----------------------------------------------------------------
_lcdPutChar:			; c in A
	sta	(lcdPutChar_c)
	lda	(GCU)		; past the last cell: dropped (see
	cpi	a, 151		; ../lcd_putchar.asm)
	bcs	lcdPutChar_full
	ldi	yl, 0x60	; YL = 60H + the cell
1$:
	cpi	a, 6		; Carry: A >= 6
	bcr	2$
	sbi	a, #6		; (Carry set: no borrow)
	inc	yl
	bch	1$
2$:
	lda	(__lcd_edit)	; 00H, or 50H while editing: 7BB0H
	rec
	adc	yl
	sta	xl
	ldi	xh, 0x7B
	lda	(lcdPutChar_c)
	sta	(x)
	sjp	__lcd_draw	; drawn (lcd_draw.asm: ' [ ] its own), the
				; cursor a cell on; then into __lcd_cursor

; While editing: the cursor's LCD address to 787EH/787FH, as ROM1's line
; editor keeps it -- the Screen pane's cursor. Past the last cell, left
; on the last one (an address off the display reads as the line entered).
; Not editing: nothing.
__lcd_cursor:
	lda	(__lcd_edit)
	bzs	lcdPutChar_full
	lda	(GCU)
	cpi	a, 151
	bcs	lcdPutChar_full
	vmj	0x8C
	lda	xh
	sta	(CURSOR_H)
	lda	xl
	sta	(CURSOR_L)
lcdPutChar_full:
	rtn

	.area DATA
lcdPutChar_c:
	.ds 1
__lcd_edit:
	.ds 1
