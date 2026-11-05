;--------------------------------------------------------------------------
; _fsmul.asm - float a * b for the LH5801 port (see _lh5801_fs.asm).
;
; Paul Chambre, 2026
;
; The 24 x 24-bit product of the mantissas by shift and add, its 48 bits
; in XH XL YH YL UH UL: the multiplier starts in the low half and moves
; out of it as the product moves in. The top 32 bits go to __fs_pack, the
; rest as a sticky bit.
;--------------------------------------------------------------------------

	.module _fsmul
	.area CODE

	.globl ___fsmul
	.globl ___fsmul_PARM_1
	.globl ___fsmul_PARM_2
	.globl __fs_unpack
	.globl __fs_pack
	.globl __fs_zero
	.globl __fs_sgn
	.globl __fs_e
	.globl __fs_t
	.globl __fs_n
	.globl __fs_m
	.globl __fs_b

___fsmul:
	ldi	xh, >(___fsmul_PARM_1)
	ldi	xl, <(___fsmul_PARM_1)
	ldi	yh, >(__fs_m)
	ldi	yl, <(__fs_m)
	sjp	__fs_unpack
	sta	(__fs_e)
	lda	uh
	sta	(__fs_sgn)
	ldi	xh, >(___fsmul_PARM_2)
	ldi	xl, <(___fsmul_PARM_2)
	ldi	yh, >(__fs_b)
	ldi	yl, <(__fs_b)
	sjp	__fs_unpack
	sta	(__fs_t)
	lda	uh
	eor	(__fs_sgn)
	ani	a, #0x80
	sta	(__fs_sgn)
	lda	(__fs_t)
	bzs	fm_zero
	lda	(__fs_e)
	bzs	fm_zero
	rec			; the exponent: ea + eb - 126, kept in __fs_e/__fs_t
	adc	(__fs_t)
	sta	yl
	ldi	a, 0x00
	adi	a, #0x00
	sta	yh
	sec
	lda	yl
	sbi	a, #126
	sta	(__fs_t)
	lda	yh
	sbi	a, #0x00
	sta	(__fs_e)

	ldi	a, 0x00
	sta	xh
	sta	xl
	sta	yh
	lda	(__fs_b)
	sta	yl
	lda	(__fs_b + 1)
	sta	uh
	lda	(__fs_b + 2)
	sta	ul
	ldi	a, 24
	sta	(__fs_n)
fm_loop:
	rec
	lda	ul
	bii	a, #0x01
	bzs	fm_shift
	lda	yh
	adc	(__fs_m + 2)
	sta	yh
	lda	xl
	adc	(__fs_m + 1)
	sta	xl
	lda	xh
	adc	(__fs_m)
	sta	xh
fm_shift:
	lda	xh
	ror
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
	lda	uh
	ror
	sta	uh
	lda	ul
	ror
	sta	ul
	lda	(__fs_n)
	dec	a
	sta	(__fs_n)
	bzr	fm_loop

	lda	xh
	sta	(__fs_m)
	lda	xl
	sta	(__fs_m + 1)
	lda	yh
	sta	(__fs_m + 2)
	lda	ul
	bzr	fm_sticky
	lda	uh
	bzs	fm_store
fm_sticky:
	lda	yl
	ori	a, #0x01
	sta	yl
fm_store:
	lda	yl
	sta	(__fs_m + 3)
	lda	(__fs_e)
	sta	yh
	lda	(__fs_t)
	sta	yl
	jmp	__fs_pack
fm_zero:
	jmp	__fs_zero

	.area DATA
___fsmul_PARM_1:
	.ds	4
___fsmul_PARM_2:
	.ds	4
