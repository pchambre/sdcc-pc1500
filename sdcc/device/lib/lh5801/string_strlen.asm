;--------------------------------------------------------------------------
; string_strlen.asm - strlen() on CIN, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; CIN compares A with the byte at (X) -- flags only, as A - (X) -- and
; increments X (pc1500emu's lh5801.cpp, opcode F7H). With A = 0, the loop
; CIN / BZR stops just past the terminating 0, so the length is
; X - s - 1: a 16-bit subtraction started with the carry clear (REC),
; which on the LH5801 means "borrow one".
;--------------------------------------------------------------------------

	.module string_strlen
	.area CODE

	.globl _strlen

;-----------------------------------------------------------------
; size_t strlen(const char *s);  -- returned in X (XH high)
;-----------------------------------------------------------------
_strlen:			; s in X
	lda	xh
	sta	yh
	lda	xl
	sta	yl
	ldi	a, 0x00
strlen_loop:
	cin
	bzr	strlen_loop
	lda	xl
	rec
	sbc	yl
	sta	xl
	lda	xh
	sbc	yh
	sta	xh
	rtn

	.area DATA
