;--------------------------------------------------------------------------
; _fs2ulong.asm - float to every integer type but long long, for the
; LH5801 port (see _lh5801_fs.asm). Toward zero, saturating as SDCC's
; generic _fs2*.c: a negative float is 0 as an unsigned type, too large
; is the type's maximum (minimum). Unlike the generic __fs2slong, which
; gave -__fs2ulong(-f) -- 1 for -1e10 -- a long saturates as well.
;
; Paul Chambre, 2026
;
; One argument area for them all (each name a label on it): fs_whole
; turns it into |f| truncated, in XH XL YH YL, C set if it is 2^32 or more.
;--------------------------------------------------------------------------

	.module _fs2ulong
	.area CODE

	.globl ___fs2ulong
	.globl ___fs2ulong_PARM_1
	.globl ___fs2slong
	.globl ___fs2slong_PARM_1
	.globl ___fs2uint
	.globl ___fs2uint_PARM_1
	.globl ___fs2sint
	.globl ___fs2sint_PARM_1
	.globl ___fs2uchar
	.globl ___fs2uchar_PARM_1
	.globl ___fs2schar
	.globl ___fs2schar_PARM_1
	.globl __fs_unpack
	.globl __fs_sgn
	.globl __fs_e
	.globl __fs_m
	.globl __lh5801_ret2
	.globl __lh5801_ret3

fs_whole:
	ldi	xh, >(___fs2ulong_PARM_1)
	ldi	xl, <(___fs2ulong_PARM_1)
	ldi	yh, >(__fs_m)
	ldi	yl, <(__fs_m)
	sjp	__fs_unpack
	sta	(__fs_e)
	lda	uh
	ani	a, #0x80
	sta	(__fs_sgn)
	lda	(__fs_e)
	cpi	a, #127
	bcr	fw_zero			; below 1 (a zero too)
	cpi	a, #159
	bcs	fw_over			; 2^32 or more
	ldi	xh, 0x00
	lda	(__fs_m)
	sta	xl
	lda	(__fs_m + 1)
	sta	yh
	lda	(__fs_m + 2)
	sta	yl
	lda	(__fs_e)
	sec
	sbi	a, #150			; the shift: left if >= 0
	bcr	fw_right
	sta	ul
	bzs	fw_done
fw_left:
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
	dec	ul
	bzr	fw_left
	bch	fw_done
fw_right:
	eai	a, #0xFF
	inc	a
	sta	ul
fw_rbit:
	lda	xl
	shr
	sta	xl
	lda	yh
	ror
	sta	yh
	lda	yl
	ror
	sta	yl
	dec	ul
	bzr	fw_rbit
fw_done:
	rec
	rtn
fw_zero:
	ldi	xh, 0x00
	ldi	xl, 0x00
	ldi	yh, 0x00
	ldi	yl, 0x00
	rec
	rtn
fw_over:
	sec
	rtn

___fs2ulong:
	sjp	fs_whole
	lda	(__fs_sgn)
	bzr	fu_zero
	bcr	fs_ret32
	ldi	xh, 0xFF
	ldi	xl, 0xFF
	ldi	yh, 0xFF
	ldi	yl, 0xFF
	bch	fs_ret32
fu_zero:
	ldi	xh, 0x00
	ldi	xl, 0x00
	ldi	yh, 0x00
	ldi	yl, 0x00
fs_ret32:
	lda	yh
	sta	(__lh5801_ret2)
	lda	yl
	sta	(__lh5801_ret3)
	rtn

___fs2slong:
	sjp	fs_whole
	bcs	fsl_sat
	lda	xh
	bii	a, #0x80
	bzr	fsl_sat			; 2^31 or more
	lda	(__fs_sgn)
	bzs	fs_ret32
	sec
	ldi	a, 0x00
	sbc	yl
	sta	yl
	ldi	a, 0x00
	sbc	yh
	sta	yh
	ldi	a, 0x00
	sbc	xl
	sta	xl
	ldi	a, 0x00
	sbc	xh
	sta	xh
	bch	fs_ret32
fsl_sat:
	lda	(__fs_sgn)
	bzr	fsl_min
	ldi	xh, 0x7F
	ldi	xl, 0xFF
	ldi	yh, 0xFF
	ldi	yl, 0xFF
	bch	fs_ret32
fsl_min:
	ldi	xh, 0x80
	ldi	xl, 0x00
	ldi	yh, 0x00
	ldi	yl, 0x00
	bch	fs_ret32

___fs2uint:
	sjp	___fs2ulong
	lda	xh
	bzr	fui_max
	lda	xl
	bzr	fui_max
	lda	yh
	sta	xh
	lda	yl
	sta	xl
	rtn
fui_max:
	ldi	xh, 0xFF
	ldi	xl, 0xFF
	rtn

___fs2uchar:
	sjp	___fs2uint
	lda	xh
	bzr	fuc_max
	lda	xl
	rtn
fuc_max:
	ldi	a, 0xFF
	rtn

___fs2sint:			; in range if XH and XL extend YH's sign
	sjp	___fs2slong
	lda	yh
	shl
	ldi	a, 0x00
	sbi	a, #0x00
	eai	a, #0xFF
	cpa	xh
	bzr	fsi_sat
	cpa	xl
	bzr	fsi_sat
	lda	yh
	sta	xh
	lda	yl
	sta	xl
	rtn
fsi_sat:
	lda	xh
	bii	a, #0x80
	bzr	fsi_min
	ldi	xh, 0x7F
	ldi	xl, 0xFF
	rtn
fsi_min:
	ldi	xh, 0x80
	ldi	xl, 0x00
	rtn

___fs2schar:
	sjp	___fs2sint
	lda	xl
	shl
	ldi	a, 0x00
	sbi	a, #0x00
	eai	a, #0xFF
	cpa	xh
	bzr	fsc_sat
	lda	xl
	rtn
fsc_sat:
	lda	xh
	bii	a, #0x80
	bzr	fsc_min
	ldi	a, 0x7F
	rtn
fsc_min:
	ldi	a, 0x80
	rtn

	.area DATA
___fs2ulong_PARM_1:
___fs2slong_PARM_1:
___fs2uint_PARM_1:
___fs2sint_PARM_1:
___fs2uchar_PARM_1:
___fs2schar_PARM_1:
	.ds	4
