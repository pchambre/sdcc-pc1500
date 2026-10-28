;--------------------------------------------------------------------------
; _lh5801_set4.asm - the LH5801 backend's 4-byte constant-store helper.
;
; Paul Chambre, 2026
;
; gen.c's storeLiteral() stores a 4-byte constant into memory with a call
; to this, the destination and the constant's bytes inline after it,
; instead of LDI/STA pairs (up to 20 bytes, 9 this way):
;
;	sjp	__lh5801_set4
;	.dw	dst
;	.db	b0, b1, b2, b3
;
; See _lh5801_cpy4.asm for how the inline operands are read. Clobbers A,
; Y and U.
;--------------------------------------------------------------------------

	.module _lh5801_set4
	.area CODE

	.globl __lh5801_set4

__lh5801_set4:
	pop	u
	lin	u
	sta	yh
	lin	u
	sta	yl
	lin	u
	sin	y
	lin	u
	sin	y
	lin	u
	sin	y
	lin	u
	sin	y
	psh	u
	rtn
