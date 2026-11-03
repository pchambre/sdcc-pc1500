;--------------------------------------------------------------------------
; atomic_flag_test_and_set.asm - stdatomic.h's atomic_flag for the LH5801
; SDCC port.
;
; Paul Chambre, 2026
;
; The LH5801 has no read-modify-write that keeps the old value (its
; memory ADI/ANI/ORI/BII only leave flags of the new one), so the read and
; the write happen with interrupts off, as a __critical { } block does
; (gen.c genCritical): the flags, IE among them, saved and put back. One
; CPU, so masking interrupts is all "atomic" needs. The flag byte is 0
; clear (ATOMIC_FLAG_INIT, atomic_flag_clear()), 1 set.
;--------------------------------------------------------------------------

	.module atomic_flag_test_and_set
	.area CODE

	.globl _atomic_flag_test_and_set

;-----------------------------------------------------------------
; _Bool atomic_flag_test_and_set(volatile atomic_flag *object);
;
; X = object (the first 2-byte parameter); returns in A whether it was
; set. Clobbers Y.
;-----------------------------------------------------------------
_atomic_flag_test_and_set:
	tta			; the flags, IE among them
	rie
	psh	a
	lda	(x)		; the old flag
	sta	yl
	ldi	a, 1
	sta	(x)		; set
	pop	a
	att			; IE as it was
	lda	yl
	bzs	1$
	ldi	a, 1		; _Bool: 0 or 1
1$:
	rtn
