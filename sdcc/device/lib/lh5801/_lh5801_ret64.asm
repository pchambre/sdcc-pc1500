;--------------------------------------------------------------------------
; _lh5801_ret64.asm - bytes 64-255 of a wide return value (LH5801 backend).
;
; Paul Chambre, 2026
;
; The block after __lh5801_ret8 (_lh5801_ret8.asm), for a struct returned
; by value that is over 64 bytes; gen.c addresses it as
; (__lh5801_ret64 + i - 64). A module of its own, so only a program that
; returns more than 64 bytes links it.
;--------------------------------------------------------------------------

	.module _lh5801_ret64
	.area DATA

	.globl __lh5801_ret64

__lh5801_ret64:
	.ds 192
