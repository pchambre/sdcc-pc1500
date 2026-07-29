;--------------------------------------------------------------------------
; pc1500_fp_helpers.asm - shared copy-in/copy-out helpers for the
; PC-1500 native floating-point library, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; Internal use only -- not part of the public C API in pc1500.h. Raw
; register-based convention (not the normal PARM_n calling convention
; every C-callable function in this port uses), since every pc1500*.asm
; wrapper in this library needs exactly this operation and a real
; C-style function call would cost more than it saves.
;
; Every PC-1500 ROM math routine (SQR, SIN, ADD, etc.) only operates
; correctly on data actually stored at the fixed RAM addresses 0x7A00
; (first/only operand) and 0x7A10 (second operand, two-operand routines
; only) -- confirmed by direct testing this session: pointing X at an
; arbitrary caller-supplied buffer address does NOT work (the ROM
; routine returns almost immediately without doing anything), even
; though X superficially looks like it should be a general pointer
; parameter. So every wrapper must copy the caller's pc1500_float_t
; buffer in before calling the ROM routine, and copy the (possibly
; modified) result back out afterward.
;
; __pc1500_fp_copyin:  X = source buffer address (caller's buffer).
;                      Copies 8 bytes from (X) to 0x7A00-0x7A07.
;                      Clobbers A, X.
; __pc1500_fp_copyin2: X = source buffer address for the SECOND
;                      operand (two-operand routines only). Copies 8
;                      bytes from (X) to 0x7A10-0x7A17. Clobbers A, X.
; __pc1500_fp_copyout: X = destination buffer address (caller's
;                      buffer). Copies 8 bytes from 0x7A00-0x7A07 to
;                      (X). Also captures the ROM's UH error-code
;                      register (confirmed this session: divide by
;                      zero leaves a nonzero code there) into
;                      _pc1500_last_error, readable via pc1500Error() --
;                      see pc1500_error.asm -- since it's called
;                      immediately after every math routine, before UH
;                      could be clobbered by anything else. Clobbers A, X.
;--------------------------------------------------------------------------

	.module pc1500_fp_helpers
	.area CODE

	.globl __pc1500_fp_copyin
	.globl __pc1500_fp_copyin2
	.globl __pc1500_fp_copyout
	.globl _pc1500_last_error

__pc1500_fp_copyin:
	lin	x
	sta	(0x7A00)
	lin	x
	sta	(0x7A01)
	lin	x
	sta	(0x7A02)
	lin	x
	sta	(0x7A03)
	lin	x
	sta	(0x7A04)
	lin	x
	sta	(0x7A05)
	lin	x
	sta	(0x7A06)
	lin	x
	sta	(0x7A07)
	rtn

__pc1500_fp_copyin2:
	lin	x
	sta	(0x7A10)
	lin	x
	sta	(0x7A11)
	lin	x
	sta	(0x7A12)
	lin	x
	sta	(0x7A13)
	lin	x
	sta	(0x7A14)
	lin	x
	sta	(0x7A15)
	lin	x
	sta	(0x7A16)
	lin	x
	sta	(0x7A17)
	rtn

__pc1500_fp_copyout:
	lda	uh
	sta	(_pc1500_last_error)
	lda	(0x7A00)
	sin	x
	lda	(0x7A01)
	sin	x
	lda	(0x7A02)
	sin	x
	lda	(0x7A03)
	sin	x
	lda	(0x7A04)
	sin	x
	lda	(0x7A05)
	sin	x
	lda	(0x7A06)
	sin	x
	lda	(0x7A07)
	sin	x
	rtn

	.area DATA
_pc1500_last_error:
	.ds 1
