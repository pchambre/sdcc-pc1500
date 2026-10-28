;--------------------------------------------------------------------------
; _lh5801_scmp4.asm - the LH5801 backend's 4-byte signed comparison.
;
; Paul Chambre, 2026
;
; _lh5801_cmp4.asm for signed operands: the most significant bytes are
; compared with their sign bits flipped (EAI 80H), which maps signed
; order onto unsigned order, as genCmp() does inline. The carry is set
; on return when a >= b:
;
;	sjp	__lh5801_scmp4
;	.dw	a + 3, b + 3
;
; EAI, LDA and STA UH leave the borrow alone. Clobbers A, X, Y and U.
;--------------------------------------------------------------------------

	.module _lh5801_scmp4
	.area CODE

	.globl __lh5801_scmp4

__lh5801_scmp4:
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
	lda	(y)
	eai	a, #0x80
	sta	uh
	lda	(x)
	eai	a, #0x80
	sbc	uh
	rtn
