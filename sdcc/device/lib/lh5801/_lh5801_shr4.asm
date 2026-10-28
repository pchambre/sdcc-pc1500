;--------------------------------------------------------------------------
; _lh5801_shr4.asm - the LH5801 backend's 4-byte in-place right shifts.
;
; Paul Chambre, 2026
;
; _lh5801_shl4.asm's counterparts, from the FIRST (most significant)
; byte down: __lh5801_shr4 shifts in zeros (unsigned), __lh5801_sar4
; copies of the sign bit (signed):
;
;	sjp	__lh5801_shr4		; or __lh5801_sar4
;	.dw	v
;	.db	n
;
; Each pass puts the first byte's new top bit in the carry -- 0 (REC),
; or for sar the sign bit (LDA (X), SHL) -- then LDA (X), ROR, SIN X
; (store and step on) per byte. UH says which: the entry's SEC/REC
; survives the operand fetch (LIN, STA, PSH and LDI UH leave the carry
; alone). Clobbers A, X, Y and U.
;--------------------------------------------------------------------------

	.module _lh5801_shr4
	.area CODE

	.globl __lh5801_shr4
	.globl __lh5801_sar4

__lh5801_sar4:
	sec
	bch	__lh5801_shr4_go
__lh5801_shr4:
	rec
__lh5801_shr4_go:
	pop	u
	lin	u
	sta	yh
	lin	u
	sta	yl
	lin	u
	psh	u
	sta	ul
	ldi	uh, 0x00
	bcr	00101$
	ldi	uh, 0x01
00101$:
	lda	yh
	sta	xh
	lda	yl
	sta	xl
	rec
	lda	uh
	bzs	00103$
	lda	(x)
	shl
00103$:
	lda	(x)
	ror
	sin	x
	lda	(x)
	ror
	sin	x
	lda	(x)
	ror
	sin	x
	lda	(x)
	ror
	sta	(x)
	dec	ul
	bzr	00101$
	rtn
