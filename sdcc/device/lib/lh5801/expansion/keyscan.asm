;--------------------------------------------------------------------------
; expansion/keyscan.asm - keyScan() for build-lh5801.sh --expansion (in
; ../keyboard.asm in a plain build): the PC-1500's own key first; else,
; with the expansion module's external (Bluetooth) keyboard driver on
; ROM1's keyboard hook (79D4H = 55H, the vector at 785BH/785CH = KBD_HOOK,
; 880EH -- PC1500-PSOC5 rom.asm), its key from the module's window: a
; matrix index at 87EFH, believed only when the byte at 87D8H is its
; complement (a sleeping MCU's window reads as whatever the bus holds),
; as the code at FE00H + the index -- rom.asm KBD_SCAN. KEYSCAN_NOWAIT
; (E42CH), which the plain keyScan() calls, doesn't go through the hook.
;
; Paul Chambre, 2026
;--------------------------------------------------------------------------

	.module keyscan
	.area CODE

	.globl _keyScan

KBD_HOOK = 0x880E		; rom.asm ROM_BASE+0EH
KBD_KEY = 0x87EF		; pc_exp.h EXP_KBD_KEY
KBD_KEY_CHK = 0x87D8		; ...and its complement

;-----------------------------------------------------------------
; unsigned char keyScan(void);
;-----------------------------------------------------------------
_keyScan:
	sjp	0xE42C
	bcr	keyScan_done
	lda	(0x79D4)	; the hook in use...
	cpi	a, 0x55
	bzr	keyScan_none
	lda	(0x785B)	; ...by the external keyboard's driver
	cpi	a, >KBD_HOOK
	bzr	keyScan_none
	lda	(0x785C)
	cpi	a, <KBD_HOOK
	bzr	keyScan_none
	lda	(KBD_KEY_CHK)
	eai	a, #0xFF
	cpa	(KBD_KEY)
	bzr	keyScan_none
	cpi	a, 0x00
	bzs	keyScan_none
	sta	xl
	ldi	xh, 0xFE
	lda	(x)
	rtn
keyScan_none:
	ldi	a, 0x00
keyScan_done:
	rtn

