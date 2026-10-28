;--------------------------------------------------------------------------
; _lh5801_ret8.asm - bytes 8-63 of a wide return value (LH5801 backend).
;
; Paul Chambre, 2026
;
; Bytes 0-1 of a return value come back in X, bytes 2-7 in __lh5801_ret2..
; _ret7 (_lh5801_ret.asm), and anything wider -- a struct such as
; lldiv()'s -- in this block, which gen.c addresses as
; (__lh5801_ret8 + i - 8). A module of its own, so only a program that
; returns more than 8 bytes links it.
;--------------------------------------------------------------------------

	.module _lh5801_ret8
	.area DATA

	.globl __lh5801_ret8

__lh5801_ret8:
	.ds 56
