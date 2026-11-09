;--------------------------------------------------------------------------
; bank_api.asm - bank_select() and bank_current() (pc1500_bank.h), for a
; program built with build-lh5801.sh --banks=.
;
; Paul Chambre, 2026
;
; Both go to the device's own module (device/lib/lh5801/bank/), which
; build-lh5801.sh links: its __sdcc_bank_select switches the 0000H-3FFFH
; window raw -- BASIC's program pointers stay as they are -- and
; __sdcc_bank is the bank that's in.
;--------------------------------------------------------------------------

	.module bank_api
	.area CODE

	.globl _bank_select
	.globl _bank_current
	.globl __sdcc_bank_select
	.globl __sdcc_bank

;-----------------------------------------------------------------
; void bank_select(unsigned char bank);	-- A = bank
;-----------------------------------------------------------------
_bank_select:
	jmp	__sdcc_bank_select

;-----------------------------------------------------------------
; unsigned char bank_current(void);
;-----------------------------------------------------------------
_bank_current:
	lda	(__sdcc_bank)
	rtn
