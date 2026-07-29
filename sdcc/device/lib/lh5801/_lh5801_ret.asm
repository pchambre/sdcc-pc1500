;--------------------------------------------------------------------------
; _lh5801_ret.asm - real, single, shared storage for the LH5801 backend's
; __lh5801_ret2.._lh5801_ret7 wide-return-value pass-through bytes.
;
; Paul Chambre, 2026
;
; genReturn()/genCall() (src/lh5801/gen.c) pass return-value bytes beyond
; the first two (which use A/X, real CPU registers, inherently shared
; across every module) through these six named memory locations instead.
; They must be genuinely SHARED across independently-compiled modules --
; a callee's module writes them, a different caller's module reads them
; back -- which a plain per-module ".area DATA" declaration (like
; __lh5801_cmp_scratch/__lh5801_frame_ptr, gen.c's own
; genLH5801AssemblerStart()) can't provide: every module compiled that
; way gets its own private, unshared byte, confirmed directly to
; silently corrupt every real cross-module call returning more than 2
; bytes (e.g. any function in this directory returning a 4-byte `float`
; or `long`).
;
; The fix is this dedicated, one-time, always-linked module -- exactly
; the same problem, solved exactly the same way, as
; device/lib/hc08/_ret.c and device/lib/s08/_ret.c's own
; __SDCC_hc08_ret2../__SDCC_s08_ret2.. (a handful of ordinary global
; variables in one real, singly-defined module, referenced by name --
; not redeclared -- from every other module's generated code). Every
; other .asm file in this directory is unconditionally linked into any
; PC-1500 program by support/scripts/build-lh5801.sh already, so this
; one is too, with no extra wiring needed.
;--------------------------------------------------------------------------

	.module _lh5801_ret
	.area DATA

	.globl __lh5801_ret2
	.globl __lh5801_ret3
	.globl __lh5801_ret4
	.globl __lh5801_ret5
	.globl __lh5801_ret6
	.globl __lh5801_ret7

__lh5801_ret2:
	.ds 1
__lh5801_ret3:
	.ds 1
__lh5801_ret4:
	.ds 1
__lh5801_ret5:
	.ds 1
__lh5801_ret6:
	.ds 1
__lh5801_ret7:
	.ds 1
