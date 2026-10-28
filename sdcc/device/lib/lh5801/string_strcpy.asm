;--------------------------------------------------------------------------
; string_strcpy.asm - strcpy() on LIN/SIN, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; X = src, Y = dest; LIN X loads a byte and increments X, setting Z when
; the byte is 0 (pc1500emu's lh5801.cpp, opcode 45H), and SIN Y stores it
; and increments Y without touching the flags -- so the whole loop is
; LIN X / SIN Y / BZR, and the terminating 0 is copied too.
;--------------------------------------------------------------------------

	.module string_strcpy
	.area CODE

	.globl _strcpy
	.globl _strcpy_PARM_2

;-----------------------------------------------------------------
; char *strcpy(char *dest, const char *src);  -- returns dest
;-----------------------------------------------------------------
_strcpy:			; dest in X
	psh	x		; dest, to return
	lda	xh
	sta	yh
	lda	xl
	sta	yl
	lda	(_strcpy_PARM_2)
	sta	xh
	lda	(_strcpy_PARM_2 + 1)
	sta	xl
strcpy_loop:
	lin	x
	sin	y
	bzr	strcpy_loop
	pop	x
	rtn

	.area DATA
_strcpy_PARM_2:
	.ds 2
