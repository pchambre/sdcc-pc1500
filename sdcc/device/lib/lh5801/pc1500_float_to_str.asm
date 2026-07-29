;--------------------------------------------------------------------------
; pc1500_float_to_str.asm - convert a PC-1500 native number to a decimal
; string via the real ROM's own STR$ routine, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; ROM address 0xD9CF (STR$, found via the base ROM's BASIC keyword
; table at 0xC060-0xC38B, confirmed directly against ROM1.BIN this
; session). Needs the value copied into the fixed buffer at 0x7A00
; first (same reason as every other routine in this library -- see
; pc1500_fp_helpers.asm) and 0x7894=0x10 set as an extra entry
; condition (confirmed necessary, cause not otherwise understood).
;
; STR$ writes its result into the ROM's own "String Buffer" at 0x7B10
; (confirmed against the PC-2 manual's memory map) but does NOT null-
; terminate it or report a length anywhere (confirmed directly: neither
; A, X, Y, U, nor S carry a length signal across four different-length
; test outputs). Since STR$ only ever writes the actual digit
; characters and never touches anything past the end, this wrapper
; zero-fills a generous span of the caller's output buffer first --
; the untouched trailing zero then serves as a normal C string
; terminator.
;--------------------------------------------------------------------------

	.module pc1500_float_to_str
	.area CODE

	.globl _pc1500FloatToStr
	.globl _pc1500FloatToStr_PARM_1
	.globl _pc1500FloatToStr_PARM_2
	.globl __pc1500_fp_copyin

; Longest possible output: sign, 10 mantissa digits, decimal point, "E",
; exponent sign, 2 exponent digits = 16 characters, plus the trailing
; zero this wrapper relies on -- 20 bytes is comfortable headroom.
PC1500_STR_BUF_LEN	= 20

;-----------------------------------------------------------------
; void pc1500FloatToStr(pc1500_float_t v, char *out);
;-----------------------------------------------------------------
_pc1500FloatToStr:
	lda	(_pc1500FloatToStr_PARM_1)
	sta	xh
	lda	(_pc1500FloatToStr_PARM_1 + 1)
	sta	xl
	sjp	__pc1500_fp_copyin

	ldi	a, 0x00
	sta	(0x7894)
	ori	a, 0x10
	sta	(0x7894)

	ldi	xh, 0x7b
	ldi	xl, 0x10
	ldi	a, 0x00
00101$:
	sin	x
	cpi	xl, 0x10 + PC1500_STR_BUF_LEN
	bzr	00101$

	sjp	0xD9CF

	; copy the resulting NUL-terminated string (see this file's own
	; comment: STR$ never writes anything past the real digits, and we
	; zero-filled the buffer above, so the first zero byte really is
	; the end) from the ROM's string buffer (0x7B10) to the caller's
	; output buffer -- LIN sets the zero flag from the byte it just
	; loaded (confirmed elsewhere this session), so this copies
	; including the terminator and then stops.
	lda	(_pc1500FloatToStr_PARM_2)
	sta	xh
	lda	(_pc1500FloatToStr_PARM_2 + 1)
	sta	xl
	ldi	yh, 0x7b
	ldi	yl, 0x10
00102$:
	lin	y
	sin	x
	bzr	00102$
	rtn

	.area DATA
_pc1500FloatToStr_PARM_1:
	.ds 2
_pc1500FloatToStr_PARM_2:
	.ds 2
