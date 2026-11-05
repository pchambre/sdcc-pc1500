;--------------------------------------------------------------------------
; _fsadd.asm - float a + b and a - b for the LH5801 port (see
; _lh5801_fs.asm).
;
; Paul Chambre, 2026
;
; The larger magnitude goes to __fs_m (and gives the result its sign),
; the smaller is shifted right in XH XL YH YL to line up, what falls off
; it kept as a sticky bit, then added or subtracted (never going
; negative). a - b is a + -b, sharing the arguments.
;--------------------------------------------------------------------------

	.module _fsadd
	.area CODE

	.globl ___fsadd
	.globl ___fsadd_PARM_1
	.globl ___fsadd_PARM_2
	.globl ___fssub
	.globl ___fssub_PARM_1
	.globl ___fssub_PARM_2
	.globl __fs_unpack
	.globl __fs_pack
	.globl __fs_zero
	.globl __fs_sgn
	.globl __fs_e
	.globl __fs_t
	.globl __fs_d
	.globl __fs_m
	.globl __fs_b

___fssub:
	lda	(___fsadd_PARM_2)
	eai	a, #0x80
	sta	(___fsadd_PARM_2)
___fsadd:
	lda	(___fsadd_PARM_2)	; |a| >= |b|?
	ani	a, #0x7F
	sta	(__fs_t)
	sec
	lda	(___fsadd_PARM_1 + 3)
	sbc	(___fsadd_PARM_2 + 3)
	lda	(___fsadd_PARM_1 + 2)
	sbc	(___fsadd_PARM_2 + 2)
	lda	(___fsadd_PARM_1 + 1)
	sbc	(___fsadd_PARM_2 + 1)
	lda	(___fsadd_PARM_1)
	ani	a, #0x7F
	sbc	(__fs_t)
	ldi	xh, >(___fsadd_PARM_2)	; no: b is the larger
	ldi	xl, <(___fsadd_PARM_2)
	ldi	yh, >(___fsadd_PARM_1)
	ldi	yl, <(___fsadd_PARM_1)
	bcr	fa_order
	ldi	xh, >(___fsadd_PARM_1)
	ldi	xl, <(___fsadd_PARM_1)
	ldi	yh, >(___fsadd_PARM_2)
	ldi	yl, <(___fsadd_PARM_2)
fa_order:
	psh	y
	ldi	yh, >(__fs_m)
	ldi	yl, <(__fs_m)
	sjp	__fs_unpack
	sta	(__fs_e)
	lda	uh
	ani	a, #0x80
	sta	(__fs_sgn)
	pop	x
	lda	(__fs_e)
	bzs	fa_zero			; both are zeros
	ldi	yh, >(__fs_b)
	ldi	yl, <(__fs_b)
	sjp	__fs_unpack
	bzs	fa_big			; b is a zero
	sta	(__fs_t)
	lda	uh
	eor	(__fs_sgn)
	sta	(__fs_d)		; bit 7: the signs differ
	lda	(__fs_e)
	sec
	sbc	(__fs_t)
	cpi	a, #32
	bcs	fa_big			; too small to matter
	sta	ul			; the shift
	lda	(__fs_b)
	sta	xh
	lda	(__fs_b + 1)
	sta	xl
	lda	(__fs_b + 2)
	sta	yh
	ldi	yl, 0x00
	ldi	uh, 0x00		; sticky
fa_bytes:
	lda	ul
	cpi	a, #8
	bcr	fa_bits
	sbi	a, #8
	sta	ul
	lda	yl
	bzs	fa_byte
	ldi	uh, 0x01
fa_byte:
	lda	yh
	sta	yl
	lda	xl
	sta	yh
	lda	xh
	sta	xl
	ldi	xh, 0x00
	bch	fa_bytes
fa_bits:
	lda	ul
	bzs	fa_lined
fa_bit:
	lda	xh
	shr
	sta	xh
	lda	xl
	ror
	sta	xl
	lda	yh
	ror
	sta	yh
	lda	yl
	ror
	sta	yl
	bcr	fa_next
	ldi	uh, 0x01
fa_next:
	dec	ul
	bzr	fa_bit
fa_lined:
	lda	uh
	bzs	fa_which
	lda	yl
	ori	a, #0x01
	sta	yl
fa_which:
	lda	(__fs_d)
	bii	a, #0x80
	bzr	fa_sub
	rec
	lda	(__fs_m + 3)
	adc	yl
	sta	(__fs_m + 3)
	lda	(__fs_m + 2)
	adc	yh
	sta	(__fs_m + 2)
	lda	(__fs_m + 1)
	adc	xl
	sta	(__fs_m + 1)
	lda	(__fs_m)
	adc	xh
	sta	(__fs_m)
	ldi	yh, 0x00
	lda	(__fs_e)
	sta	yl
	bcr	fa_pack
	lda	(__fs_m)		; the carry: one right, keeping what goes
	ror
	sta	(__fs_m)
	lda	(__fs_m + 1)
	ror
	sta	(__fs_m + 1)
	lda	(__fs_m + 2)
	ror
	sta	(__fs_m + 2)
	lda	(__fs_m + 3)
	ror
	sta	(__fs_m + 3)
	bcr	fa_up
	ori	a, #0x01
	sta	(__fs_m + 3)
fa_up:
	inc	y
	jmp	__fs_pack
fa_sub:
	sec
	lda	(__fs_m + 3)
	sbc	yl
	sta	(__fs_m + 3)
	lda	(__fs_m + 2)
	sbc	yh
	sta	(__fs_m + 2)
	lda	(__fs_m + 1)
	sbc	xl
	sta	(__fs_m + 1)
	lda	(__fs_m)
	sbc	xh
	sta	(__fs_m)
fa_big:				; the larger one alone (__fs_m3 is 0)
	ldi	yh, 0x00
	lda	(__fs_e)
	sta	yl
fa_pack:
	jmp	__fs_pack
fa_zero:
	jmp	__fs_zero

	.area DATA
___fsadd_PARM_1:
___fssub_PARM_1:
	.ds	4
___fsadd_PARM_2:
___fssub_PARM_2:
	.ds	4
