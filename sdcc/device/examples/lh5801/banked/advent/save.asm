;--------------------------------------------------------------------------
; save.asm - Colossal Cave Adventure for the PC-1500: room for a saved
; game in bank 0 (SAVE_BANK in advent.h), after the bank's code. io.c's
; saveGame() puts a sum (2 bytes) and the game's state here; the card's
; SRAM keeps it while the PC-1500 is off. Loading the program's banks
; again clears it.
;--------------------------------------------------------------------------

	.module save
	.area CODE_0

	.globl _save_area
_save_area:
	.ds	1200
