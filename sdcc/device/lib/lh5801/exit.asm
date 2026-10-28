;--------------------------------------------------------------------------
; exit.asm - C's exit(): return to BASIC from anywhere in a program, for
; the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; The startup code's last step (SDCCglue.c, TARGET_IS_LH5801, in GSFINAL)
; saves S in __sdcc_lh5801_sp just before it calls main, and main's own
; return lands on __sdcc_lh5801_exit, which restores what startup saved
; for BASIC and returns to BASIC's CALL. exit() puts S back to that saved
; value -- discarding however deep the program's calls were -- and joins
; the same path. The status is ignored: BASIC's CALL has nowhere to put
; it.
;
; GSFINAL references __sdcc_lh5801_sp, so this module is linked into every
; program; exit() itself costs 9 bytes.
;--------------------------------------------------------------------------

	.module exit
	.area CODE

	.globl _exit
	.globl __sdcc_lh5801_sp
	.globl __sdcc_lh5801_exit

;-----------------------------------------------------------------
; void exit(int status);
;-----------------------------------------------------------------
_exit:
	lda	(__sdcc_lh5801_sp)
	sta	xh
	lda	(__sdcc_lh5801_sp + 1)
	sta	xl
	stx	s
	jmp	__sdcc_lh5801_exit

	.area DATA
__sdcc_lh5801_sp::
	.ds 2
