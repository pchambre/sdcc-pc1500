;--------------------------------------------------------------------------
; _fsdiv.asm - float a / b for the LH5801 port (see _lh5801_fs.asm).
;
; Paul Chambre, 2026
;
; 32 quotient bits of the mantissas by restoring division: the remainder
; in XH XL YH YL (25 bits at most), the quotient shifted into __fs_m.
; What is left of the remainder is the sticky bit. As SDCC's generic
; __fsdiv: x / 0 is an infinity of x's sign, 0 / 0 a NaN (FFC00000).
;--------------------------------------------------------------------------

	.module _fsdiv
	.area CODE

	.globl ___fsdiv
	.globl ___fsdiv_PARM_1
	.globl ___fsdiv_PARM_2
	.globl __fs_unpack
	.globl __fs_pack
	.globl __fs_zero
	.globl __fs_inf
	.globl __fs_sgn
	.globl __fs_e
	.globl __fs_t
	.globl __fs_d
	.globl __fs_n
	.globl __fs_m
	.globl __fs_b
	.globl __lh5801_ret2
	.globl __lh5801_ret3

___fsdiv:
	ldi	xh, >(___fsdiv_PARM_1)
	ldi	xl, <(___fsdiv_PARM_1)
	ldi	yh, >(__fs_m)
	ldi	yl, <(__fs_m)
	sjp	__fs_unpack
	sta	(__fs_e)
	lda	uh
	sta	(__fs_sgn)
	ldi	xh, >(___fsdiv_PARM_2)
	ldi	xl, <(___fsdiv_PARM_2)
	ldi	yh, >(__fs_b)
	ldi	yl, <(__fs_b)
	sjp	__fs_unpack
	sta	(__fs_t)
	lda	uh
	eor	(__fs_sgn)
	ani	a, #0x80
	sta	(__fs_sgn)
	lda	(__fs_t)
	bzs	fd_inf
	lda	(__fs_e)
	bzs	fd_zero
	sec			; the exponent: ea - eb + 127, kept in __fs_e/__fs_t
	sbc	(__fs_t)
	sta	yl
	ldi	a, 0x00
	sbi	a, #0x00	; 0 or FF: the borrow, extended
	sta	yh
	rec
	lda	yl
	adi	a, #127
	sta	(__fs_t)
	lda	yh
	adi	a, #0x00
	sta	(__fs_e)

	ldi	a, 0x00
	sta	xh
	lda	(__fs_m)
	sta	xl
	lda	(__fs_m + 1)
	sta	yh
	lda	(__fs_m + 2)
	sta	yl
	ldi	a, 32
	sta	(__fs_n)
fd_loop:
	sec			; remainder - divisor, kept if it doesn't borrow
	lda	yl
	sbc	(__fs_b + 2)
	sta	ul
	lda	yh
	sbc	(__fs_b + 1)
	sta	uh
	lda	xl
	sbc	(__fs_b)
	sta	(__fs_d)
	lda	xh
	sbi	a, #0x00
	bcr	fd_bit
	sta	xh
	lda	(__fs_d)
	sta	xl
	lda	uh
	sta	yh
	lda	ul
	sta	yl
fd_bit:				; C is the quotient bit
	lda	(__fs_m + 3)
	rol
	sta	(__fs_m + 3)
	lda	(__fs_m + 2)
	rol
	sta	(__fs_m + 2)
	lda	(__fs_m + 1)
	rol
	sta	(__fs_m + 1)
	lda	(__fs_m)
	rol
	sta	(__fs_m)
	lda	yl
	shl
	sta	yl
	lda	yh
	rol
	sta	yh
	lda	xl
	rol
	sta	xl
	lda	xh
	rol
	sta	xh
	lda	(__fs_n)
	dec	a
	sta	(__fs_n)
	bzr	fd_loop

	lda	xh
	bzr	fd_sticky
	lda	xl
	bzr	fd_sticky
	lda	yh
	bzr	fd_sticky
	lda	yl
	bzs	fd_pack
fd_sticky:
	lda	(__fs_m + 3)
	ori	a, #0x01
	sta	(__fs_m + 3)
fd_pack:
	lda	(__fs_e)
	sta	yh
	lda	(__fs_t)
	sta	yl
	jmp	__fs_pack
fd_inf:				; x / 0, as the generic one: +-inf by x's sign,
	lda	(__fs_e)		; NaN for 0 / 0
	bzs	fd_nan
	lda	(___fsdiv_PARM_1)
	ani	a, #0x80
	sta	(__fs_sgn)
	jmp	__fs_inf
fd_nan:
	ldi	xh, 0xFF
	ldi	xl, 0xC0
	ldi	a, 0x00
	sta	(__lh5801_ret2)
	sta	(__lh5801_ret3)
	rtn
fd_zero:
	jmp	__fs_zero

	.area DATA
___fsdiv_PARM_1:
	.ds	4
___fsdiv_PARM_2:
	.ds	4
