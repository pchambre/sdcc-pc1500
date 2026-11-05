;--------------------------------------------------------------------------
; _ulong2fs.asm - every integer type but long long to float, for the
; LH5801 port (see _lh5801_fs.asm), rounded to nearest: the magnitude
; goes into __fs_m with the exponent that makes it its own value, and
; __fs_pack does the rest.
;
; Paul Chambre, 2026
;
; As C passes them: a long in its PARM_1 (both longs' names on one area),
; an int in X, a char in A.
;--------------------------------------------------------------------------

	.module _ulong2fs
	.area CODE

	.globl ___ulong2fs
	.globl ___ulong2fs_PARM_1
	.globl ___slong2fs
	.globl ___slong2fs_PARM_1
	.globl ___uint2fs
	.globl ___sint2fs
	.globl ___uchar2fs
	.globl ___schar2fs
	.globl __fs_pack
	.globl __fs_sgn
	.globl __fs_m

___slong2fs:
	lda	(___ulong2fs_PARM_1)
	ani	a, #0x80
	sta	(__fs_sgn)
	bzs	ul_copy
	sec
	ldi	a, 0x00
	sbc	(___ulong2fs_PARM_1 + 3)
	sta	(__fs_m + 3)
	ldi	a, 0x00
	sbc	(___ulong2fs_PARM_1 + 2)
	sta	(__fs_m + 2)
	ldi	a, 0x00
	sbc	(___ulong2fs_PARM_1 + 1)
	sta	(__fs_m + 1)
	ldi	a, 0x00
	sbc	(___ulong2fs_PARM_1)
	sta	(__fs_m)
	bch	ul_pack
___ulong2fs:
	ldi	a, 0x00
	sta	(__fs_sgn)
ul_copy:
	lda	(___ulong2fs_PARM_1)
	sta	(__fs_m)
	lda	(___ulong2fs_PARM_1 + 1)
	sta	(__fs_m + 1)
	lda	(___ulong2fs_PARM_1 + 2)
	sta	(__fs_m + 2)
	lda	(___ulong2fs_PARM_1 + 3)
	sta	(__fs_m + 3)
ul_pack:
	ldi	yh, 0x00
	ldi	yl, 158
	jmp	__fs_pack

___sint2fs:
	lda	xh
	ani	a, #0x80
	sta	(__fs_sgn)
	bzs	ui_copy
	sec
	ldi	a, 0x00
	sbc	xl
	sta	xl
	ldi	a, 0x00
	sbc	xh
	sta	xh
	bch	ui_copy
___uint2fs:
	ldi	a, 0x00
	sta	(__fs_sgn)
ui_copy:
	lda	xh
	sta	(__fs_m)
	lda	xl
	sta	(__fs_m + 1)
	ldi	a, 0x00
	sta	(__fs_m + 2)
	sta	(__fs_m + 3)
	ldi	yh, 0x00
	ldi	yl, 142
	jmp	__fs_pack

___schar2fs:
	sta	xl
	ani	a, #0x80
	sta	(__fs_sgn)
	bzs	uc_copy
	lda	xl
	eai	a, #0xFF
	inc	a
	sta	xl
	bch	uc_copy
___uchar2fs:
	sta	xl
	ldi	a, 0x00
	sta	(__fs_sgn)
uc_copy:
	lda	xl
	sta	(__fs_m)
	ldi	a, 0x00
	sta	(__fs_m + 1)
	sta	(__fs_m + 2)
	sta	(__fs_m + 3)
	ldi	yh, 0x00
	ldi	yl, 134
	jmp	__fs_pack

	.area DATA
___ulong2fs_PARM_1:
___slong2fs_PARM_1:
	.ds	4
