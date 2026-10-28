;--------------------------------------------------------------------------
; _lh5801_add4.asm - the LH5801 backend's 4-byte in-place add helper.
;
; Paul Chambre, 2026
;
; gen.c's genAddSub() adds two 4-byte memory operands, a += b, with a
; call to this and the addresses of both operands' LAST (least
; significant) bytes inline after it, instead of a 37-byte LDA/ADC/STA
; chain (11 bytes this way):
;
;	sjp	__lh5801_add4
;	.dw	a + 3, b + 3
;
; Going down from the low bytes: LDA (X), ADC (Y), then SDE X stores and
; steps X back, DEC Y steps Y back -- neither touches the carry. See
; _lh5801_cpy4.asm for how the inline operands are read. Clobbers A, X,
; Y and U.
;--------------------------------------------------------------------------

	.module _lh5801_add4
	.area CODE

	.globl __lh5801_add4

__lh5801_add4:
	pop	u
	lin	u
	sta	xh
	lin	u
	sta	xl
	lin	u
	sta	yh
	lin	u
	sta	yl
	psh	u
	rec
	lda	(x)
	adc	(y)
	sde	x
	dec	y
	lda	(x)
	adc	(y)
	sde	x
	dec	y
	lda	(x)
	adc	(y)
	sde	x
	dec	y
	lda	(x)
	adc	(y)
	sde	x
	rtn
