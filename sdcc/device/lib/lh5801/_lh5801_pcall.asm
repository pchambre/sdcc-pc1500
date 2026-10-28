;--------------------------------------------------------------------------
; _lh5801_pcall.asm - the LH5801 backend's call-through-a-pointer JMP.
;
; Paul Chambre, 2026
;
; The LH5801 has no indirect call. gen.c's genCall() stores a function
; pointer into this JMP's operand and calls it:
;
;	lda	(ptr)
;	sta	(__lh5801_pcall_trampoline + 1)
;	lda	(ptr + 1)
;	sta	(__lh5801_pcall_trampoline + 2)
;	sjp	__lh5801_pcall_trampoline
;
; Programs run from RAM, so the code can be patched. Nothing runs between
; the patch and the jump, so a call that comes back here (recursion through
; a pointer) patches it again for itself.
;--------------------------------------------------------------------------

	.module _lh5801_pcall
	.area CODE

	.globl __lh5801_pcall_trampoline

__lh5801_pcall_trampoline:
	jmp	0x0000
