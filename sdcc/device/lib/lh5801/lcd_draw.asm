;--------------------------------------------------------------------------
; lcd_draw.asm - one character drawn at the Graphic Cursor and the cursor
; moved on a cell, for lcdPutChar() (lcd_putchar.asm, and the --expansion
; build's expansion/lcd_putchar.asm). The LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; ROM1's font has no ', [ or ]: its 27H is a box, 5BH the square root and
; 5DH pi. So those three are drawn from the library's own shapes -- the
; expansion module ROM's (PC1500-PSOC5 rom.asm TERM_GLYPHS), in the style
; of ROM1's ( and ) -- the way SBR8A (ROM1 LED5B) draws one of its own:
; five dot columns through SBR88 (VMJ 88H: a column at X, X moved on),
; then a blank one. Every other code goes to SBR8A as before -- and
; PC1500_CHAR_BOX, _SQRT and _PI (A7H, DBH, DDH: those three with bit 7
; set) go to it as 27H, 5BH and 5DH, for the ROM's own shapes.
;--------------------------------------------------------------------------

	.module lcd_draw
	.area CODE

	.globl __lcd_draw

;-----------------------------------------------------------------
; A = the character. Past the last 6-column cell (GCU > 150) it would be
; drawn outside the display buffer: it's dropped instead, so a line is
; simply cut at 26 characters.
;-----------------------------------------------------------------
__lcd_draw:
	sta	(lcd_draw_c)
	lda	(0x7875)
	cpi	a, 151
	bcs	5$
	vmj	0x8C		; X = the display address of the cursor
	lda	(lcd_draw_c)	; PC1500_CHAR_BOX/SQRT/PI: the ROM's own 27H,
	cpi	a, 0xA7		; 5BH and 5DH
	bzs	6$
	cpi	a, 0xDB
	bzs	6$
	cpi	a, 0xDD
	bzr	7$
6$:
	ani	a, #0x7F
	bch	8$
7$:
	ldi	yh, >lcd_glyphs
	ldi	yl, <lcd_glyphs
	ldi	ul, 2
1$:
	lin	y		; a shape's character; Y on its columns
	cpa	(lcd_draw_c)
	bzs	2$
	ldi	a, 5
	adr	y
	lop	ul, 1$
	lda	(lcd_draw_c)
8$:
	vmj	0x8A		; the ROM's own
	bch	4$
2$:
	ldi	ul, 4
3$:
	lin	y
	vmj	0x88
	lop	ul, 3$
	ldi	a, 0x00		; the gap
	vmj	0x88
4$:
	lda	(0x7875)	; the cursor a cell on, as ROM1's own display
	rec			; routine moves it (LED12: ADI (7875H),06H)
	adi	a, #6
	sta	(0x7875)
5$:
	rtn

; the character, then five dot columns, bit 0 the top row
lcd_glyphs:
	.db	0x27, 0x00, 0x0B, 0x07, 0x00, 0x00	; ' (ROM1's comma, at the top)
	.db	0x5B, 0x00, 0x7F, 0x41, 0x41, 0x00	; [
	.db	0x5D, 0x00, 0x41, 0x41, 0x7F, 0x00	; ]

	.area DATA
lcd_draw_c:
	.ds 1
