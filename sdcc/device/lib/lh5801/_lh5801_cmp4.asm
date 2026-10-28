;--------------------------------------------------------------------------
; _lh5801_cmp4.asm - the LH5801 backend's 4-byte unsigned comparison.
;
; Paul Chambre, 2026
;
; gen.c's genCmp() compares two 4-byte memory operands for < or >= with
; a call to this and the addresses of both operands' LAST (least
; significant) bytes inline after it, instead of a 25-byte SEC, LDA/SBC
; chain (7 bytes this way):
;
;	sjp	__lh5801_cmp4
;	.dw	a + 3, b + 3
;
; It returns with the carry as that chain leaves it: set when a >= b.
; DEC X/Y step back without touching the borrow, and nothing after the
; last SBC (RTN included) changes it. See _lh5801_cpy4.asm for how the
; inline operands are read. Clobbers A, X, Y and U.
;--------------------------------------------------------------------------

	.module _lh5801_cmp4
	.area CODE

	.globl __lh5801_cmp4

__lh5801_cmp4:
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
	dec	x
	dec	y
	lda	(x)
	sbc	(y)
	dec	x
	dec	y
	lda	(x)
	sbc	(y)
	dec	x
	dec	y
	lda	(x)
	sbc	(y)
	rtn
