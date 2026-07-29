;--------------------------------------------------------------------------
; pc1500_error.asm - last ROM math-routine error code, for the LH5801
; SDCC port.
;
; Paul Chambre, 2026
;
; _pc1500_last_error is captured by __pc1500_fp_copyout (see
; pc1500_fp_helpers.asm) right after every PC-1500 native math call,
; from the ROM's own UH error-code register (confirmed this session:
; divide by zero leaves a nonzero code there, 0x26 in one observed
; case; the full meaning of each code value is not otherwise
; documented/verified -- treat "nonzero" as "something went wrong",
; not the specific value as meaningful yet).
;--------------------------------------------------------------------------

	.module pc1500_error
	.area CODE

	.globl _pc1500Error
	.globl _pc1500_last_error

;-----------------------------------------------------------------
; unsigned char pc1500Error(void);
;-----------------------------------------------------------------
_pc1500Error:
	lda	(_pc1500_last_error)
	rtn
