;--------------------------------------------------------------------------
; string_memcpy.asm - memcpy()'s worker, __memcpy(), on the LH5801's own
; block-move instruction, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; string.h turns memcpy() into __memcpy() (device/lib/memcpy.c is the
; plain wrapper the other way round). TIN copies the byte at (X) to (Y)
; and increments both, in one byte and 7 cycles, so the loop is X = src,
; Y = dst, U = n, then TIN and DEC U until U is 0 -- about 25 cycles a
; byte, against 200-odd for device/lib/__memcpy.c compiled by this port.
; DEC U leaves the flags alone (pc1500emu's lh5801.cpp, opcode 66H), so U
; is tested for zero explicitly. Overlapping copies are undefined for
; memcpy(), and this copies upward, like the C version.
;--------------------------------------------------------------------------

	.module string_memcpy
	.area CODE

	.globl ___memcpy
	.globl ___memcpy_PARM_2
	.globl ___memcpy_PARM_3

;-----------------------------------------------------------------
; void *__memcpy(void *dest, const void *src, size_t n);  -- returns dest
;-----------------------------------------------------------------
___memcpy:			; dest in X
	lda	xh
	sta	yh
	lda	xl
	sta	yl
	psh	y		; dest, to return
	lda	(___memcpy_PARM_2)
	sta	xh
	lda	(___memcpy_PARM_2 + 1)
	sta	xl
	lda	(___memcpy_PARM_3)
	sta	uh
	lda	(___memcpy_PARM_3 + 1)
	sta	ul
memcpy_loop:
	lda	ul
	bzr	memcpy_byte
	lda	uh
	bzs	memcpy_done
memcpy_byte:
	tin
	dec	u
	bch	memcpy_loop
memcpy_done:
	pop	x
	rtn

	.area DATA
___memcpy_PARM_2:
	.ds 2
___memcpy_PARM_3:
	.ds 2
