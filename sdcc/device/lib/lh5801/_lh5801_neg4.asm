;--------------------------------------------------------------------------
; _lh5801_neg4.asm - the LH5801 backend's 4-byte in-place negation.
;
; Paul Chambre, 2026
;
; v = -v for a 4-byte memory operand, with the address of its LAST
; (least significant) byte inline after the call, instead of a 33-byte
; LDI/SBC/STA chain (5 bytes this way):
;
;	sjp	__lh5801_neg4
;	.dw	v + 3
;
; 0 - v going up from the low byte; SDE X stores and steps back without
; touching the borrow. See _lh5801_cpy4.asm for how the inline operand
; is read. Clobbers A, X and U.
;--------------------------------------------------------------------------

	.module _lh5801_neg4
	.area CODE

	.globl __lh5801_neg4

__lh5801_neg4:
	pop	u
	lin	u
	sta	xh
	lin	u
	sta	xl
	psh	u
	sec
	ldi	a, 0x00
	sbc	(x)
	sde	x
	ldi	a, 0x00
	sbc	(x)
	sde	x
	ldi	a, 0x00
	sbc	(x)
	sde	x
	ldi	a, 0x00
	sbc	(x)
	sta	(x)
	rtn
