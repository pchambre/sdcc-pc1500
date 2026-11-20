;--------------------------------------------------------------------------
; expansion/keyboard.asm - keyWait() for build-lh5801.sh
; --expansion: as ../keyboard.asm, aware of what the expansion module
; adds to the keyboard.
;
; Paul Chambre, 2026
;
; The module's ROM puts its own driver on ROM1's keyboard hook (79D4H =
; 55H, the vector at 785BH/785CH; PC1500-PSOC5 rom.asm KBD_HOOK): with
; MCONF BLKBD, the external (Bluetooth) keyboard's key goes through it,
; and with MCONF HISTORY, at BASIC's prompt, ENTER hands the line to the
; command history and DEF+up/down/left browse it. KEYSCAN_WAIT (E243H)
; goes through the hook; KEYSCAN_NOWAIT (E42CH) doesn't.
;
; keyWait(): the run state's "a program is running" bit (788AH bit 40H)
; is set while it waits, as it is for a BASIC program -- the driver then
; leaves ENTER and DEF+arrows to ROM1, so the program's own lines don't
; reach the command history (788AH is put back after). While a line is
; being edited (lcdEditBegin()), the cursor goes where the program left
; it first -- a backspace moves it back -- for the Screen pane.
;
; keyScan() is in expansion/keyscan.asm, so a program that doesn't use
; it doesn't carry it.
;--------------------------------------------------------------------------

	.module keyboard
	.area CODE

	.globl _keyWait
	.globl __lcd_cursor

RUN_STATE = 0x788A

;-----------------------------------------------------------------
; unsigned char keyWait(void);
;-----------------------------------------------------------------
_keyWait:
	sjp	__lcd_cursor	; (while editing)
	lda	(RUN_STATE)
	psh	a		; to put back
	ori	a, #0x40
	sta	(RUN_STATE)
	sjp	0xE243
	bcr	1$		; (Carry: BREAK -- dealt with before the POP)
	ldi	a, 0x0E
1$:
	sta	yl
	pop	a
	sta	(RUN_STATE)
	lda	yl
	rtn
