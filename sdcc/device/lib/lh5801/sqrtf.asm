;--------------------------------------------------------------------------
; sqrtf.asm - float sqrtf(float x) for the LH5801 port (see
; _lh5801_fs.asm), correctly rounded. As SDCC's generic sqrtf.c: a zero
; is returned as it is, a negative x sets errno to EDOM and gives 0.
;
; Paul Chambre, 2026
;
; The square root of the mantissa M (shifted one left for an odd
; exponent), times 2^26, digit by digit in base 2: two bits of the
; radicand at a time from __fs_b into the remainder (XH XL YH YL), 26
; root bits kept as T = 4 * root in __fs_m. The remainder left over is
; the sticky bit. sqrtf is __reentrant in math.h on this port: x comes on
; the stack (the caller removes it).
;--------------------------------------------------------------------------

	.module sqrtf
	.area CODE

	.globl _sqrtf
	.globl _errno
	.globl __fs_unpack
	.globl __fs_pack
	.globl __fs_zero
	.globl __fs_sgn
	.globl __fs_e
	.globl __fs_d
	.globl __fs_n
	.globl __fs_m
	.globl __fs_b

_sqrtf:
	ldx	s
	inc	x			; past the return address, to x
	inc	x
	inc	x
	ldi	yh, >(__fs_m)
	ldi	yl, <(__fs_m)
	sjp	__fs_unpack
	sta	(__fs_e)
	lda	uh
	ani	a, #0x80
	sta	(__fs_sgn)
	lda	(__fs_e)
	bzr	sq_some
	jmp	__fs_zero		; +0 or -0, as it is
sq_some:
	lda	(__fs_sgn)
	bzs	sq_pos
	ldi	a, 0x00
	sta	(_errno)
	ldi	a, 33			; EDOM
	sta	(_errno + 1)
	ldi	a, 0x00
	sta	(__fs_sgn)
	jmp	__fs_zero
sq_pos:
	lda	(__fs_m)		; the radicand, from bit 31: M >> 1, or M >> 2
	shr				; for an even exponent (M << 6 or << 7 of 32)
	sta	(__fs_b)
	lda	(__fs_m + 1)
	ror
	sta	(__fs_b + 1)
	lda	(__fs_m + 2)
	ror
	sta	(__fs_b + 2)
	ldi	a, 0x00
	ror
	sta	(__fs_b + 3)
	lda	(__fs_e)
	shr
	bcs	sq_go
	lda	(__fs_b)
	shr
	sta	(__fs_b)
	lda	(__fs_b + 1)
	ror
	sta	(__fs_b + 1)
	lda	(__fs_b + 2)
	ror
	sta	(__fs_b + 2)
	lda	(__fs_b + 3)
	ror
	sta	(__fs_b + 3)
sq_go:
	ldi	a, 0x00
	sta	xh
	sta	xl
	sta	yh
	sta	yl
	sta	(__fs_m)
	sta	(__fs_m + 1)
	sta	(__fs_m + 2)
	sta	(__fs_m + 3)
	ldi	a, 26
	sta	(__fs_n)
sq_loop:
	ldi	ul, 0x01		; twice: the radicand's top bit into the remainder
sq_pair:
	lda	(__fs_b + 3)
	shl
	sta	(__fs_b + 3)
	lda	(__fs_b + 2)
	rol
	sta	(__fs_b + 2)
	lda	(__fs_b + 1)
	rol
	sta	(__fs_b + 1)
	lda	(__fs_b)
	rol
	sta	(__fs_b)
	lda	yl
	rol
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
	lop	ul, sq_pair
	rec				; remainder - (T + 1), kept if it doesn't borrow
	lda	yl
	sbc	(__fs_m + 3)
	sta	ul
	lda	yh
	sbc	(__fs_m + 2)
	sta	uh
	lda	xl
	sbc	(__fs_m + 1)
	sta	(__fs_d)
	lda	xh
	sbc	(__fs_m)
	bcr	sq_shift
	sta	xh
	lda	(__fs_d)
	sta	xl
	lda	uh
	sta	yh
	lda	ul
	sta	yl
	lda	(__fs_m + 3)		; a 1: T + 2, before T * 2
	ori	a, #0x02
	sta	(__fs_m + 3)
sq_shift:
	lda	(__fs_m + 3)
	shl
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
	lda	(__fs_n)
	dec	a
	sta	(__fs_n)
	bzr	sq_loop

	lda	(__fs_m)		; m = T / 2 = root * 2, the sticky bit below
	shr
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
	lda	xh
	bzr	sq_sticky
	lda	xl
	bzr	sq_sticky
	lda	yh
	bzr	sq_sticky
	lda	yl
	bzs	sq_pack
sq_sticky:
	lda	(__fs_m + 3)
	ori	a, #0x01
	sta	(__fs_m + 3)
sq_pack:
	lda	(__fs_e)		; 69 + e / 2
	shr
	rec
	adi	a, #69
	sta	yl
	ldi	yh, 0x00
	jmp	__fs_pack
