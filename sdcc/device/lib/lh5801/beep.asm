;--------------------------------------------------------------------------
; beep.asm - the buzzer, through the ROM's own BEEP tone generator, for
; the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; The BEEP keyword (ROM1 LE5C1) parses its arguments and then calls the
; tone loop at E66FH (disassembled from ROM1.BIN with pc1500disasm):
;
;   E669H: ldi ul,08H / ldi x,01A0H, falling into E66FH -- BEEP's default
;          tone, "BEEP 1".
;   E66FH: saves X, Y, U; toggles the buzzer bit (PC6) of the I/O port at
;          F008H once per half period, a half period being a LOP UL loop
;          of UL iterations (so a bigger UL is a lower pitch, and UL 0 is
;          256); after each period it decrements X and stops when XH
;          reaches 0 -- so it plays X - 0FFH periods. It also stops early
;          when the I/O controller's IF register (F00BH) has bit 0 or 1
;          set (BREAK). Returns with X, Y, U restored.
;
; Neither checks BEEP ON/OFF (786BH bit 0) -- that is the BEEP keyword's
; own business, not the tone loop's.
;--------------------------------------------------------------------------

	.module beep
	.area CODE

	.globl _beep
	.globl _beepTone
	.globl _beepTone_PARM_1
	.globl _beepTone_PARM_2

;-----------------------------------------------------------------
; void beep(void);  -- BASIC's BEEP 1
;-----------------------------------------------------------------
_beep:
	sjp	0xE669
	rtn

;-----------------------------------------------------------------
; void beepTone(unsigned char halfPeriod, unsigned int periods);
;
; halfPeriod: LOP loop count per half period (0 = 256); periods: how
; many periods to play (0 plays nothing). X is periods + 0FFH, since the
; ROM loop stops when XH reaches 0.
;-----------------------------------------------------------------
_beepTone:
	lda	(_beepTone_PARM_2)
	ora	(_beepTone_PARM_2 + 1)
	bzs	beepTone_done
	lda	(_beepTone_PARM_1)
	sta	ul
	rec
	lda	(_beepTone_PARM_2 + 1)
	adi	a, #0xFF
	sta	xl
	lda	(_beepTone_PARM_2)
	adi	a, #0x00
	sta	xh
	sjp	0xE66F
beepTone_done:
	rtn

	.area DATA
_beepTone_PARM_1:
	.ds 1
_beepTone_PARM_2:
	.ds 2
