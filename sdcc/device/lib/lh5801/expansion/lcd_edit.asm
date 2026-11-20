;--------------------------------------------------------------------------
; expansion/lcd_edit.asm - lcdEditBegin()/lcdEditEnd() for build-lh5801.sh
; --expansion: the line being typed, as BASIC's own line editor shows it
; to the expansion module's Screen pane (see expansion/lcd_putchar.asm).
;
; Paul Chambre, 2026
;
; lcdEditBegin(): the display's line so far (a prompt, say) goes to
; BASIC's line buffer (7BB0H) -- and leaves the output buffer, so the pane
; doesn't report it as output too -- and from then on lcdPutChar() and
; lcdClear() write there instead of the output buffer, with the cursor's
; address at 787EH/787FH; the mode (7880H) is 40H, a line being typed.
; The pane shows it as the line being edited, its cursor and all.
; lcdEditEnd(): the mode 20H, as after ENTER -- the pane reports the line
; as entered -- and the output buffer again. And the line buffer is ended
; (0DH): a CALL typed at BASIC's prompt runs from that buffer, and when the
; program returns, BASIC goes on reading it after the CALL -- what the
; program left there gave ERROR 1. So a program that exits while a line is
; being edited calls lcdEditEnd() first. (The pane reads the bus's writes
; in order, so it has the line before it's ended.)
;--------------------------------------------------------------------------

	.module lcd_edit
	.area CODE

	.globl _lcdEditBegin
	.globl _lcdEditEnd
	.globl __lcd_edit
	.globl __lcd_cursor
	.globl __lcd_fill

MODE = 0x7880			; ROM1's line editor: 40H typing, 20H entered

_lcdEditBegin:
	ldi	xh, 0x7B	; 7B60H-7B7AH (26 cells and the end) to
	ldi	xl, 0x60	; 7BB0H
	ldi	yh, 0x7B
	ldi	yl, 0xB0
	ldi	ul, 26
1$:
	lin	x
	sin	y
	lop	ul, 1$
	ldi	a, 0x0D		; the output buffer empty, as BASIC's prompt
	sta	(0x7B60)	; leaves it: the line is the typed one now
	ldi	a, 0x50
	sta	(__lcd_edit)
	ldi	a, 0x40
	sta	(MODE)
	jmp	__lcd_cursor

_lcdEditEnd:
	ldi	a, 0x00
	sta	(__lcd_edit)
	ldi	a, 0x20		; entered: the pane reports the line now
	sta	(MODE)
	ldi	xl, 0xB0	; then BASIC's line buffer ends at once: a CALL
	ldi	a, 0x0D		; typed at the prompt runs from it, and BASIC
	jmp	__lcd_fill	; goes on reading it after the program
