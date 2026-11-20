;--------------------------------------------------------------------------
; lcd_puts.asm - lcdPutString(), for the LH5801 SDCC port: a C string from
; the cursor, a character at a time through lcdPutChar().
;
; Paul Chambre, 2026
;--------------------------------------------------------------------------

	.module lcd_puts
	.area CODE

	.globl _lcdPutString
	.globl _lcdPutChar

;-----------------------------------------------------------------
; void lcdPutString(const char *s);  -- s in X
;-----------------------------------------------------------------
_lcdPutString:
	lda	(x)
	bzs	1$
	psh	x		; lcdPutChar() may use X, Y and U
	sjp	_lcdPutChar
	pop	x
	inc	x
	bch	_lcdPutString
1$:
	rtn
