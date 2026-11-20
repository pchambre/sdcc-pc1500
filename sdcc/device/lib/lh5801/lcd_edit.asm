;--------------------------------------------------------------------------
; lcd_edit.asm - lcdEditBegin()/lcdEditEnd(), for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; Nothing to do in a plain build: they matter only to the expansion
; module's Screen pane, which reads BASIC's text buffers off the bus (see
; expansion/lcd_edit.asm, the build-lh5801.sh --expansion version). Here
; so a program can call them whichever way it's built.
;--------------------------------------------------------------------------

	.module lcd_edit
	.area CODE

	.globl _lcdEditBegin
	.globl _lcdEditEnd

_lcdEditBegin:
_lcdEditEnd:
	rtn
