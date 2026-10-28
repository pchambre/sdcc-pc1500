;--------------------------------------------------------------------------
; _lh5801_shl4.asm - the LH5801 backend's 4-byte in-place left shift.
;
; Paul Chambre, 2026
;
; gen.c shifts a 4-byte memory operand left by n (1-7) bits with a call
; to this, the address of its LAST (least significant) byte and n inline
; after it, instead of n passes of LDA/SHL/STA, LDA/ROL/STA... (28 bytes
; each, unrolled up to two, else in a loop), 6 bytes this way:
;
;	sjp	__lh5801_shl4
;	.dw	v + 3
;	.db	n
;
; Each pass goes up from the low byte: LDA (X), SHL or ROL, SDE X (store
; and step back -- neither touches the carry). See _lh5801_cpy4.asm for
; how the inline operands are read. Clobbers A, X, Y and U.
;--------------------------------------------------------------------------

	.module _lh5801_shl4
	.area CODE

	.globl __lh5801_shl4

__lh5801_shl4:
	pop	u
	lin	u
	sta	yh
	lin	u
	sta	yl
	lin	u
	psh	u
	sta	ul
00101$:
	lda	yh
	sta	xh
	lda	yl
	sta	xl
	lda	(x)
	shl
	sde	x
	lda	(x)
	rol
	sde	x
	lda	(x)
	rol
	sde	x
	lda	(x)
	rol
	sta	(x)
	dec	ul
	bzr	00101$
	rtn
