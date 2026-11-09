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
; GSFINAL references __sdcc_lh5801_sp and __sdcc_lh5801_atexit, so this
; module is linked into every program; exit() itself costs 9 bytes, the
; exit hook 3.
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

; Startup's GSFINAL calls this on the way back to BASIC, from main's
; return or exit(). A RTN, unless a module's startup code put a JMP to its
; own exit code over it: a banked program's (device/lib/lh5801/bank/*.asm)
; switches back to the bank BASIC was using. Programs run from RAM.
__sdcc_lh5801_atexit::
	rtn
	.dw	0

	.area DATA
__sdcc_lh5801_sp::
	.ds 2
