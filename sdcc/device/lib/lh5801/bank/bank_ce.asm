;--------------------------------------------------------------------------
; bank_ce.asm - the bank switch for a CE-163, CE-1638 or CE-163F, for a
; program built with build-lh5801.sh --banks=ce163|ce1638|ce163f.
;
; Paul Chambre, 2026
;
; A write to 5800H+n (6800H+n on a PC-1500A) puts bank n in; the value
; written doesn't matter. build-lh5801.sh assembles this with two
; equates in front of it:
;
;   BANK_PORT     58H, or 68H for a PC-1500A (--pc1500a)
;   BANK_HOME_AT  where the bank that's in can be read at startup: the
;                 vendor's switcher keeps it in every bank, at 00D4H on a
;                 CE-1638 (PEEK&D4) and 00E2H on a CE-163F (PEEK&E2).
;                 0 for a CE-163, which can't say: the program must then
;                 be started from bank 0.
;
; At startup (GSINIT, after DATA is zeroed) that bank is kept as the one
; to go back to, and the exit hook (exit.asm) is pointed at the code that
; does.
;--------------------------------------------------------------------------

	.module bank_ce

	.globl __sdcc_bank_select
	.globl __sdcc_bank
	.globl __sdcc_bank_home
	.globl __sdcc_lh5801_atexit

	.area GSINIT
	.if	BANK_HOME_AT
	lda	(BANK_HOME_AT)
	.else
	ldi	a, #0
	.endif
	sta	(__sdcc_bank)
	sta	(__sdcc_bank_home)
	ldi	a, #0xBA		; JMP __sdcc_bank_exit
	sta	(__sdcc_lh5801_atexit)
	ldi	a, >__sdcc_bank_exit
	sta	(__sdcc_lh5801_atexit + 1)
	ldi	a, <__sdcc_bank_exit
	sta	(__sdcc_lh5801_atexit + 2)

	.area CODE

; A = the bank. Keeps X and U; Y goes.
__sdcc_bank_select:
	sta	(__sdcc_bank)
	sta	yl
	ldi	yh, #BANK_PORT
	sta	(y)
	rtn

__sdcc_bank_exit:
	lda	(__sdcc_bank_home)
	jmp	__sdcc_bank_select

	.area DATA
__sdcc_bank:
	.ds	1
__sdcc_bank_home:
	.ds	1
