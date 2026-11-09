;--------------------------------------------------------------------------
; _lh5801_bcall.asm - the LH5801 backend's call into another bank.
;
; Paul Chambre, 2026
;
; A call to a __banked function (one in a file with `#pragma bank N`,
; whose code is in area CODE_N, bank N of the 0000H-3FFFH window) is
;
;	sjp	___sdcc_bcall
;	.dw	_f
;	.db	b_f		; the function's bank, from its own file
;
; This puts bank b_f in, calls _f, puts the caller's bank back and returns
; past the inline data. A (the 1-byte argument, then a 1-byte result) and
; X (the 2-byte ones) come through both ways; Y and U don't. The caller's
; bank and the return address go on the stack, so banked calls nest, and
; recurse; the JMP is patched just before it's taken, as in
; _lh5801_pcall.asm. __sdcc_bank_select (A = the bank, X kept) and
; __sdcc_bank (the bank that's in) are the device's -- build-lh5801.sh
; --banks= links its module (device/lib/lh5801/bank/).
;
; This code is in CODE, outside the banks, like everything a banked
; function needs from all of them.
;--------------------------------------------------------------------------

	.module _lh5801_bcall
	.area CODE

	.globl ___sdcc_bcall
	.globl __sdcc_bank_select
	.globl __sdcc_bank

___sdcc_bcall:
	sta	(__sdcc_bcall_a)	; the 1-byte argument
	pop	u			; the inline data: target, bank
	lin	u
	sta	(__sdcc_bcall_jmp + 1)
	lin	u
	sta	(__sdcc_bcall_jmp + 2)
	lin	u
	psh	u			; RTN goes past the data
	sta	yl
	lda	(__sdcc_bank)
	psh	a			; the caller's bank
	lda	yl
	sjp	__sdcc_bank_select
	lda	(__sdcc_bcall_a)
	sjp	__sdcc_bcall_jmp
	sta	(__sdcc_bcall_a)	; the 1-byte result
	pop	a
	sjp	__sdcc_bank_select
	lda	(__sdcc_bcall_a)
	rtn

__sdcc_bcall_jmp:
	jmp	0x0000

	.area DATA
__sdcc_bcall_a:
	.ds	1
