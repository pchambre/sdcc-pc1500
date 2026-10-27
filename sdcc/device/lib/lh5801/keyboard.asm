;--------------------------------------------------------------------------
; keyboard.asm - read the PC-1500 keyboard through the ROM's own key
; scanners, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; Both are base-ROM system subroutines (PC-2 Assembly Language manual),
; already used by the PC-1500 expansion board's own ROM on real hardware
; (PC1500-PSOC5 rom.asm KBD_SCAN/BLKEY_FN, rom_defs.inc KEYSCAN_WAIT):
;
;   E243H KEYSCAN_WAIT   - blocks until a key is pressed (sleeping on the
;       timer between scans, with the ROM's own auto power-off), returns
;       its code in A with Carry clear, or Carry set for BREAK (ON).
;   E42CH KEYSCAN_NOWAIT - scans the matrix once: Carry clear and A = the
;       code of the key that's down, or Carry set and A = 0 for none. Like
;       BASIC's INKEY$ (ROM1 LD9AA calls it), a key held down keeps
;       reading as pressed on every call.
;
; Codes are the ROM's key table's (FE80H...): letters and digits are
; their ASCII codes, ENTER 0DH, CL 18H, up 0BH, down 0AH -- see pc1500.h.
;--------------------------------------------------------------------------

	.module keyboard
	.area CODE

	.globl _keyWait
	.globl _keyScan

;-----------------------------------------------------------------
; unsigned char keyWait(void);
;
; The next key pressed, or PC1500_KEY_BREAK (0EH, not a key-table code)
; for BREAK.
;-----------------------------------------------------------------
_keyWait:
	sjp	0xE243
	bcs	keyWait_break
	rtn
keyWait_break:
	ldi	a, 0x0E
	rtn

;-----------------------------------------------------------------
; unsigned char keyScan(void);
;
; The key down right now, or 0 for none. Doesn't wait.
;-----------------------------------------------------------------
_keyScan:
	sjp	0xE42C
	bcr	keyScan_done
	ldi	a, 0x00
keyScan_done:
	rtn
