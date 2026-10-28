;--------------------------------------------------------------------------
; _lh5801_sub4.asm - the LH5801 backend's 4-byte in-place subtract helper.
;
; Paul Chambre, 2026
;
; a -= b for two 4-byte memory operands; _lh5801_add4.asm's twin (SEC
; and SBC for REC and ADC):
;
;	sjp	__lh5801_sub4
;	.dw	a + 3, b + 3
;
; Clobbers A, X, Y and U.
;--------------------------------------------------------------------------

	.module _lh5801_sub4
	.area CODE

	.globl __lh5801_sub4

__lh5801_sub4:
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
	sec
	lda	(x)
	sbc	(y)
	sde	x
	dec	y
	lda	(x)
	sbc	(y)
	sde	x
	dec	y
	lda	(x)
	sbc	(y)
	sde	x
	dec	y
	lda	(x)
	sbc	(y)
	sde	x
	rtn
