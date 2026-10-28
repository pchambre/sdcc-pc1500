; setjmp.asm - __setjmp() and longjmp() for the LH5801
;
; A jmp_buf (setjmp.h: 4 bytes) holds S as it was inside __setjmp -- one
; past the return address SJP pushed -- then that return address, high
; byte first. longjmp() puts S back, writes the return address back into
; the same two stack bytes (later calls will have overwritten them) and
; returns through it, so __setjmp's caller sees a second return, with the
; value longjmp() was given (1 if that was 0).
;
; Register parameters (main.c's lh5801_reg_parm): the jmp_buf pointer in X;
; longjmp's int in _longjmp_PARM_2 (big-endian). The int result is in X.

	.module setjmp
	.area CODE

	.globl ___setjmp
	.globl _longjmp
	.globl _longjmp_PARM_2

___setjmp:
	lda	xh
	sta	yh
	lda	xl
	sta	yl		; Y = the jmp_buf
	ldx	s
	lda	xh
	sin	y
	lda	xl
	sin	y		; S
	inc	x
	lin	x
	sin	y
	lin	x
	sin	y		; the return address at S+1, S+2
	ldi	xh, 0x00
	ldi	xl, 0x00
	rtn

_longjmp:
	lda	xh
	sta	yh
	lda	xl
	sta	yl		; Y = the jmp_buf
	lin	y
	sta	xh
	lin	y
	sta	xl
	stx	s
	inc	x
	lin	y
	sin	x
	lin	y
	sin	x		; the return address back where SJP put it
	lda	(_longjmp_PARM_2)
	sta	xh
	lda	(_longjmp_PARM_2 + 1)
	sta	xl
	bzr	longjmp_ret
	lda	xh
	bzr	longjmp_ret
	ldi	xl, 0x01	; longjmp(env, 0) returns 1
longjmp_ret:
	rtn

	.area DATA
_longjmp_PARM_2::
	.ds 2
