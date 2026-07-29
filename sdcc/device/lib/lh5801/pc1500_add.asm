;--------------------------------------------------------------------------
; pc1500_add.asm - PC-1500 native two-operand floating-point addition,
; for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; ROM address 0xEFBA, "X + Y -> X" per the PC-2 Assembly Language
; manual, confirmed directly against ROM1.BIN this session (3.0 + 4.0 =
; 7.0, correct sign handling, UH error register works). NOT a
; documented SBRxx vector-table entry -- a plain internal ROM address,
; same caveat as tape_send_char.asm's target.
;
; Both operands must be copied into the fixed buffers at 0x7A00 and
; 0x7A10 first -- pointing X/Y at arbitrary caller buffers does not
; work (confirmed directly, see pc1500_fp_helpers.asm's own comment).
; Result overwrites the first operand's buffer, matching the ROM's own
; in-place convention (pc1500Add(a, b) computes a += b).
;--------------------------------------------------------------------------

	.module pc1500_add
	.area CODE

	.globl _pc1500Add
	.globl _pc1500Add_PARM_1
	.globl _pc1500Add_PARM_2
	.globl __pc1500_fp_copyin
	.globl __pc1500_fp_copyin2
	.globl __pc1500_fp_copyout

;-----------------------------------------------------------------
; void pc1500Add(pc1500_float_t a, pc1500_float_t b); // a += b
;-----------------------------------------------------------------
_pc1500Add:
	lda	(_pc1500Add_PARM_1)
	sta	xh
	lda	(_pc1500Add_PARM_1 + 1)
	sta	xl
	sjp	__pc1500_fp_copyin

	lda	(_pc1500Add_PARM_2)
	sta	xh
	lda	(_pc1500Add_PARM_2 + 1)
	sta	xl
	sjp	__pc1500_fp_copyin2

	ldi	xh, 0x7a
	ldi	xl, 0x00
	ldi	yh, 0x7a
	ldi	yl, 0x10
	sjp	0xEFBA

	lda	(_pc1500Add_PARM_1)
	sta	xh
	lda	(_pc1500Add_PARM_1 + 1)
	sta	xl
	sjp	__pc1500_fp_copyout
	rtn

	.area DATA
_pc1500Add_PARM_1:
	.ds 2
_pc1500Add_PARM_2:
	.ds 2
