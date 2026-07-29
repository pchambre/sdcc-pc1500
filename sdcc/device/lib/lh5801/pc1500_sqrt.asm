;--------------------------------------------------------------------------
; pc1500_sqrt.asm - PC-1500 native SQR (single-operand, mutate-in-
; place) math routine, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; ROM address 0xF0E9 (per the PC-2 Assembly Language manual's memory
; map, confirmed identical on the PC-1500 -- this session verified
; several of these directly against ROM1.BIN). NOT a documented SBRxx
; vector-table entry -- a plain internal ROM address, same caveat as
; tape_send_char.asm's target.
;
; Operates on the ROM's own 8-byte number format (see pc1500.h), always
; via the fixed RAM buffer at 0x7A00-0x7A07 -- pointing X at an
; arbitrary caller buffer does not work (confirmed directly), so this
; copies the caller's pc1500_float_t in via __pc1500_fp_copyin, calls
; the ROM routine with X=0x7A00, then copies the (mutated) result back
; out via __pc1500_fp_copyout.
;--------------------------------------------------------------------------

	.module pc1500_sqrt
	.area CODE

	.globl _pc1500Sqrt
	.globl _pc1500Sqrt_PARM_1
	.globl __pc1500_fp_copyin
	.globl __pc1500_fp_copyout

;-----------------------------------------------------------------
; void pc1500Sqrt(pc1500_float_t v);
;-----------------------------------------------------------------
_pc1500Sqrt:
	lda	(_pc1500Sqrt_PARM_1)
	sta	xh
	lda	(_pc1500Sqrt_PARM_1 + 1)
	sta	xl
	sjp	__pc1500_fp_copyin

	ldi	xh, 0x7a
	ldi	xl, 0x00
	sjp	0xF0E9

	lda	(_pc1500Sqrt_PARM_1)
	sta	xh
	lda	(_pc1500Sqrt_PARM_1 + 1)
	sta	xl
	sjp	__pc1500_fp_copyout
	rtn

	.area DATA
_pc1500Sqrt_PARM_1:
	.ds 2
