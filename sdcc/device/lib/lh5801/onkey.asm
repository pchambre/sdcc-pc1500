;--------------------------------------------------------------------------
; onkey.asm - poll the ON key directly via hardware I/O, for the LH5801
; SDCC port.
;
; Paul Chambre, 2026
;
; pc1500emu's ON key (Bus::setOnKeyLine(), wired from the host UI) is
; currently only a raw hardware input line -- bit 7 of Port B (register
; 0x0F of the F00xH/B00xH/D00xH-style I/O controller, confirmed in
; bus.cpp's IoPortController::read() case 0x0F) -- it is NOT connected to
; any interrupt or to the IF register's own break-related bits. This
; means the ROM's own "BREAK Key test" routine (SBRA6, ROM E451, which
; just does `BII #(0xF00B),0x02 / RTN` -- tests IF register bit 1) will
; NEVER see the ON key in the current emulator, no matter what a called
; ML program does; nor will enabling CPU interrupts (SIE) help, since
; nothing sets the IF register or raises an MI interrupt from the ON key
; either. So a called ML routine that wants to notice ON being pressed
; has to read the raw hardware line itself, as this file does, not go
; through BASIC's own break machinery.
;
; Port B lives in ME1 (I/O space), a genuinely separate address space
; from ME0 (confirmed: LH5801 has distinct readME0/readME1 -- see
; bus.h/bus.cpp), reached only via the "#(addr)" operand form (FD-
; prefixed opcodes) -- not reachable from a plain C pointer dereference
; with this port's current backend (no I/O-space/__sfr support
; implemented yet), hence this hand-written helper.
;--------------------------------------------------------------------------

	.module onkey
	.area CODE

	.globl _onKeyPressed

;-----------------------------------------------------------------
; unsigned char onKeyPressed(void);
;
; Returns non-zero (0x80) if the ON key is currently held down, 0
; otherwise. 1-byte return convention: value in A (see genReturn()'s
; documented convention, gen.c).
;-----------------------------------------------------------------
_onKeyPressed:
	lda	#(0xF00F)
	ani	a, 0x80
	rtn
