;--------------------------------------------------------------------------
; lcd_input.asm - lcdInputLine(), BASIC-INPUT-style line entry, for the
; LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; Typed characters are echoed from the current cursor and kept while the
; line has room on screen (the Graphic Cursor Unit at 7875H is the raw
; column, 6 per character) and in buf; CL clears what's been typed; ENTER
; ends the line. Between lcdEditBegin() and lcdEditEnd(), which do nothing
; in a plain build and show the line on the expansion module's Screen pane
; in an --expansion one (see pc1500.h).
;--------------------------------------------------------------------------

	.module lcd_input
	.area CODE

	.globl _lcdInputLine
	.globl _lcdPutChar
	.globl _keyWait
	.globl _lcdEditBegin
	.globl _lcdEditEnd

GCU = 0x7875

;-----------------------------------------------------------------
; unsigned char lcdInputLine(char *buf, unsigned char max);
; buf in X, max in A. Returns the length, or FFH (PC1500_INPUT_BREAK).
;-----------------------------------------------------------------
_lcdInputLine:
	sta	(li_max)
	lda	xh
	sta	(li_buf)
	lda	xl
	sta	(li_buf + 1)
	lda	(GCU)
	sta	(li_start)
	ldi	a, 0x00
	sta	(li_n)
	sjp	_lcdEditBegin
li_key:
	sjp	_keyWait
	cpi	a, 0x0D		; ENTER
	bzs	li_end
	cpi	a, 0x0E		; BREAK
	bzs	li_break
	cpi	a, 0x18		; CL
	bzs	li_cl
	cpi	a, 0x20		; Carry: A >= the operand
	bcr	li_key
	cpi	a, 0x7F
	bcs	li_key
	sta	(li_k)
	lda	(li_n)		; room in buf...
	cpa	(li_max)
	bcs	li_key
	lda	(GCU)		; ...and on the display
	cpi	a, 151
	bcs	li_key
	sjp	li_ptr		; buf[n++] = k
	lda	(li_k)
	sta	(x)
	lda	(li_n)
	inc	a
	sta	(li_n)
	lda	(li_k)
	sjp	_lcdPutChar
	bch	li_key

li_cl:				; blanks over what's typed, the cursor back
	lda	(li_start)
	sta	(GCU)
	lda	(li_n)
	bzs	2$
	sta	(li_k)
1$:
	ldi	a, 0x20
	sjp	_lcdPutChar
	lda	(li_k)
	dec	a
	sta	(li_k)
	bzr	1$
2$:
	lda	(li_start)
	sta	(GCU)
	ldi	a, 0x00
	sta	(li_n)
	bch	li_key

li_break:			; buf[0] = 0, FFH
	ldi	a, 0x00
	sta	(li_n)
	sjp	li_end
	ldi	a, 0xFF
	rtn

li_end:				; buf[n] = 0, A = n
	sjp	_lcdEditEnd
	sjp	li_ptr
	ldi	a, 0x00
	sta	(x)
	lda	(li_n)
	rtn

li_ptr:				; X = buf + n
	lda	(li_n)
	sta	yl
	lda	(li_buf + 1)
	rec
	adc	yl
	sta	xl
	lda	(li_buf)
	adi	a, #0
	sta	xh
	rtn

	.area DATA
li_buf:
	.ds 2
li_max:
	.ds 1
li_start:
	.ds 1
li_n:
	.ds 1
li_k:
	.ds 1
