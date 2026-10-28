;--------------------------------------------------------------------------
; _lh5801_cpy4.asm - the LH5801 backend's 4-byte copy helper.
;
; Paul Chambre, 2026
;
; gen.c's genAssign() copies a 4-byte value between two memory operands
; with a call to this and the two addresses inline after it, instead of
; four LDA/STA pairs (24 bytes at every copy, 7 this way):
;
;	sjp	__lh5801_cpy4
;	.dw	dst, src
;
; The return address SJP pushes points at the inline words; POP U takes
; it, LIN U reads past them, and PSH U puts back the address after them
; for RTN. Clobbers A, X, Y and U -- gen.c only calls it when none of
; them holds a live value (or always, without register allocation).
;--------------------------------------------------------------------------

	.module _lh5801_cpy4
	.area CODE

	.globl __lh5801_cpy4

__lh5801_cpy4:
	pop	u
	lin	u
	sta	yh
	lin	u
	sta	yl
	lin	u
	sta	xh
	lin	u
	sta	xl
	psh	u
	lin	x
	sin	y
	lin	x
	sin	y
	lin	x
	sin	y
	lin	x
	sin	y
	rtn
