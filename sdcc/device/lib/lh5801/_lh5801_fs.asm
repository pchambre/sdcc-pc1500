;--------------------------------------------------------------------------
; _lh5801_fs.asm - what the LH5801 port's float routines share: the work
; area, unpacking a float, and rounding and packing a result.
;
; Paul Chambre, 2026
;
; The port's own float library (_fsadd, _fsmul, _fsdiv, _fslt, _fseq,
; _fs2ulong, _ulong2fs, sqrtf) replaces SDCC's generic C one, which is
; 7.8K and slow here (2026-11-05: 43 ms a multiply, 10 ms an add). The
; format is the same IEEE single, big-endian like every multi-byte value
; on this port; results are rounded to nearest, ties to even, which the
; generic one did only roughly. As there: no subnormals (an exponent of
; 0 is zero, FLT_HAS_SUBNORM is 0), no NaN, overflow gives infinity and
; underflow a zero of the result's sign.
;
; A value being worked on is a sign (__fs_sgn, bit 7), a 16-bit signed
; exponent (Y, passed to __fs_pack) and a 32-bit mantissa (__fs_m, four
; bytes, most significant first), worth m * 2^(Y - 158): m0's bit 7 is
; the hidden bit once normalized, m3 the bits below the last one kept.
;
; __fs_unpack: X = a float, Y = 4 bytes to unpack its mantissa to:
;              (Y) = 1.mantissa (the hidden bit set) in 3 bytes, then 0.
;              Returns A = the biased exponent, Z set if it is 0 (a zero),
;              UH = the float's first byte (its sign in bit 7). X and Y
;              end past what they read and wrote. Clobbers UL.
; __fs_pack:   normalizes __fs_m (Y adjusted to match), rounds it to 24
;              bits and returns the float in XH, XL, __lh5801_ret2/3 as a
;              C function returns one. A mantissa of 0 gives +0.
; __fs_zero, __fs_inf: return a zero or an infinity of __fs_sgn's sign.
;--------------------------------------------------------------------------

	.module _lh5801_fs
	.area CODE

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

__fs_unpack:
	lin	x
	sta	uh
	lin	x
	sta	ul
	ori	a, #0x80
	sin	y
	lin	x
	sin	y
	lin	x
	sin	y
	ldi	a, 0x00
	sin	y
	lda	ul
	shl			; C = the exponent's low bit
	lda	uh
	rol			; A = the exponent, Z if 0
	rtn

__fs_pack:
	lda	(__fs_m)
	ora	(__fs_m + 1)
	ora	(__fs_m + 2)
	ora	(__fs_m + 3)
	bzr	fp_bytes
	sta	(__fs_sgn)	; an exact 0: +0
	bch	__fs_zero
fp_bytes:			; a byte at a time while m0 is 0
	lda	(__fs_m)
	bzr	fp_bits
	lda	(__fs_m + 1)
	sta	(__fs_m)
	lda	(__fs_m + 2)
	sta	(__fs_m + 1)
	lda	(__fs_m + 3)
	sta	(__fs_m + 2)
	ldi	a, 0x00
	sta	(__fs_m + 3)
	dec	y
	dec	y
	dec	y
	dec	y
	dec	y
	dec	y
	dec	y
	dec	y
	bch	fp_bytes
fp_bits:			; then a bit at a time, A = m0
	bii	a, #0x80
	bzr	fp_round
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
	dec	y
	bch	fp_bits
fp_round:			; to nearest, ties to even
	lda	(__fs_m + 3)
	cpi	a, #0x80
	bcr	fp_range	; below half
	bzr	fp_up		; above half
	lda	(__fs_m + 2)
	shr
	bcr	fp_range	; a tie, already even
fp_up:
	sec
	lda	(__fs_m + 2)
	adi	a, #0x00
	sta	(__fs_m + 2)
	lda	(__fs_m + 1)
	adi	a, #0x00
	sta	(__fs_m + 1)
	lda	(__fs_m)
	adi	a, #0x00
	sta	(__fs_m)
	bcr	fp_range
	ldi	a, 0x80		; 1.111..1 rounded up to 10.000..0
	sta	(__fs_m)
	inc	y
fp_range:
	lda	yh
	bzs	fp_byte
	bii	a, #0x80
	bzr	__fs_zero	; below 1: underflow
	bch	__fs_inf
fp_byte:
	lda	yl
	bzs	__fs_zero
	cpi	a, #0xFF
	bzs	__fs_inf
	lda	(__fs_m)
	shl			; the hidden bit goes
	sta	xl
	lda	yl
	shr			; C = the exponent's low bit
	ora	(__fs_sgn)
	sta	xh
	lda	xl
	ror
	sta	xl
	lda	(__fs_m + 1)
	sta	(__lh5801_ret2)
	lda	(__fs_m + 2)
	sta	(__lh5801_ret3)
	rtn

__fs_zero:
	lda	(__fs_sgn)
	sta	xh
	ldi	xl, 0x00
	ldi	a, 0x00
	sta	(__lh5801_ret2)
	sta	(__lh5801_ret3)
	rtn

__fs_inf:
	lda	(__fs_sgn)
	ori	a, #0x7F
	sta	xh
	ldi	xl, 0x80
	ldi	a, 0x00
	sta	(__lh5801_ret2)
	sta	(__lh5801_ret3)
	rtn

	.area DATA
__fs_sgn:
	.ds	1
__fs_e:
	.ds	1
__fs_t:
	.ds	1
__fs_d:
	.ds	1
__fs_n:
	.ds	1
__fs_m:
	.ds	4
__fs_b:
	.ds	4
