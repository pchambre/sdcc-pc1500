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
	.globl _strcpy_PARM_1
	.globl _strcpy_PARM_2

;-----------------------------------------------------------------
; char *strcpy(char *dest, const char *src);  -- returns dest
;-----------------------------------------------------------------
_strcpy:
	lda	(_strcpy_PARM_2)
	sta	xh
	lda	(_strcpy_PARM_2 + 1)
	sta	xl
	lda	(_strcpy_PARM_1)
	sta	yh
	lda	(_strcpy_PARM_1 + 1)
	sta	yl
strcpy_loop:
	lin	x
	sin	y
	bzr	strcpy_loop
	lda	(_strcpy_PARM_1)
	sta	xh
	lda	(_strcpy_PARM_1 + 1)
	sta	xl
	rtn

	.area DATA
_strcpy_PARM_1:
	.ds 2
_strcpy_PARM_2:
	.ds 2
