;--------------------------------------------------------------------------
; string_memset.asm - memset() on SIN (store A at (X), increment X), for
; the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; X = s, U = n, A = the fill byte (c's low byte -- an int, big-endian, so
; _memset_PARM_2 + 1), then SIN X and DEC U until U is 0. DEC U leaves the
; flags alone, so U is tested for zero explicitly, and A has to be loaded
; again after that test.
;--------------------------------------------------------------------------

	.module string_memset
	.area CODE

	.globl _memset
	.globl _memset_PARM_2
	.globl _memset_PARM_3

;-----------------------------------------------------------------
; void *memset(void *s, int c, size_t n);  -- returns s
;-----------------------------------------------------------------
_memset:			; s in X
	psh	x		; s, to return
	lda	(_memset_PARM_3)
	sta	uh
	lda	(_memset_PARM_3 + 1)
	sta	ul
memset_loop:
	lda	ul
	bzr	memset_byte
	lda	uh
	bzs	memset_done
memset_byte:
	lda	(_memset_PARM_2 + 1)
	sin	x
	dec	u
	bch	memset_loop
memset_done:
	pop	x
	rtn

	.area DATA
_memset_PARM_2:
	.ds 2
_memset_PARM_3:
	.ds 2
