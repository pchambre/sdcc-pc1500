;--------------------------------------------------------------------------
; bank_card.asm - the bank switch for the internal expansion card, for a
; program built with build-lh5801.sh --banks=card.
;
; Paul Chambre, 2026
;
; The card's 0000H-3FFFH is BANK 0-4, switched by the expansion ROM's
; BANK_RAW_ENTRY at 8819H: A = the bank (A = FFH: which one is in), Carry
; set on failure, X, Y and U kept. It wakes the card's MCU when it's
; asleep, so a switch is skipped when the bank is already in.
;
; At startup (GSINIT, after DATA is zeroed) the bank that's in is kept
; as the one to go back to, and the exit hook (exit.asm) is pointed at
; the code that does.
;--------------------------------------------------------------------------

	.module bank_card

	.globl __sdcc_bank_select
	.globl __sdcc_bank
	.globl __sdcc_bank_home
	.globl __sdcc_lh5801_atexit

BANK_RAW = 0x8819

	.area GSINIT
	ldi	a, #0xFF
	sjp	BANK_RAW
	bcr	1$
	ldi	a, #0xFF		; no banks: every switch fails too
1$:
	sta	(__sdcc_bank)
	sta	(__sdcc_bank_home)
	ldi	a, #0xBA		; JMP __sdcc_bank_exit
	sta	(__sdcc_lh5801_atexit)
	ldi	a, >__sdcc_bank_exit
	sta	(__sdcc_lh5801_atexit + 1)
	ldi	a, <__sdcc_bank_exit
	sta	(__sdcc_lh5801_atexit + 2)

	.area CODE

; A = the bank. Keeps X, Y and U.
__sdcc_bank_select:
	cpa	(__sdcc_bank)
	bzs	1$
	sta	(__sdcc_bank)
	sjp	BANK_RAW
1$:
	rtn

__sdcc_bank_exit:
	lda	(__sdcc_bank_home)
	jmp	__sdcc_bank_select

	.area DATA
__sdcc_bank:
	.ds	1
__sdcc_bank_home:
	.ds	1
