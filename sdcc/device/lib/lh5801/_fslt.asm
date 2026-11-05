;--------------------------------------------------------------------------
; _fslt.asm - float a < b for the LH5801 port, as SDCC's generic
; _fslt.c: +0 and -0 are equal; otherwise the signs, then the magnitudes
; (the bit patterns, compared as unsigned numbers) decide.
;
; Paul Chambre, 2026
;--------------------------------------------------------------------------

	.module _fslt
	.area CODE

	.globl ___fslt
	.globl ___fslt_PARM_1
	.globl ___fslt_PARM_2

___fslt:
	lda	(___fslt_PARM_1)
	ora	(___fslt_PARM_2)
	ani	a, #0x7F
	ora	(___fslt_PARM_1 + 1)
	ora	(___fslt_PARM_2 + 1)
	ora	(___fslt_PARM_1 + 2)
	ora	(___fslt_PARM_2 + 2)
	ora	(___fslt_PARM_1 + 3)
	ora	(___fslt_PARM_2 + 3)
	bzs	fl_no			; two zeros
	lda	(___fslt_PARM_1)
	eor	(___fslt_PARM_2)
	bii	a, #0x80
	bzr	fl_signs
	ldi	xh, >(___fslt_PARM_1 + 3)	; both positive: |a| < |b|
	ldi	xl, <(___fslt_PARM_1 + 3)
	ldi	yh, >(___fslt_PARM_2 + 3)
	ldi	yl, <(___fslt_PARM_2 + 3)
	lda	(___fslt_PARM_1)
	bii	a, #0x80
	bzs	fl_cmp
	ldi	xh, >(___fslt_PARM_2 + 3)	; both negative: |b| < |a|
	ldi	xl, <(___fslt_PARM_2 + 3)
	ldi	yh, >(___fslt_PARM_1 + 3)
	ldi	yl, <(___fslt_PARM_1 + 3)
fl_cmp:
	sec
	lde	x
	sbc	(y)
	dec	y
	lde	x
	sbc	(y)
	dec	y
	lde	x
	sbc	(y)
	dec	y
	lde	x
	sbc	(y)
	ldi	a, 0x00			; the borrow is the answer
	adi	a, #0x00
	eai	a, #0x01
	rtn
fl_signs:			; a is the negative one?
	lda	(___fslt_PARM_1)
	shl
	ldi	a, 0x00
	adi	a, #0x00
	rtn
fl_no:
	ldi	a, 0x00
	rtn

	.area DATA
___fslt_PARM_1:
	.ds	4
___fslt_PARM_2:
	.ds	4
