;--------------------------------------------------------------------------
; idle.asm - power-conscious "wait for the next timer tick" for the
; LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; Mirrors the real ROM's own idle-loop idiom exactly (disassembled
; directly from ROM1.BIN at 0xE2A2-0xE2AA, part of the boot/idle loop
; that also does keyboard scanning -- see [[pc1500_rom_lcd_print_routine]]
; for the sibling ROM-calling library files):
;
;   ldi a,0x57
;   am0      ; load the CPU's internal 9-bit timer counter (confirmed in
;            ; lh5801.cpp: AM0 sets timerCounter_ = A, MSB 0)
;   sie      ; enable interrupts
;   hlt      ; stop the CPU until an interrupt fires
;
; A busy-wait loop (e.g. `while (!onKeyPressed());`) spins the CPU at
; full speed forever, burning battery on real hardware for no reason --
; this instead lets the CPU sleep until the next periodic timer
; interrupt (the same one that drives the ROM's own keyboard-scan
; cadence), at which point *real ROM interrupt-handling code* runs (the
; timer ISR at vector 0xFFFA, per ASMADDR.txt's vector table SBRFA =
; E22C, "Timer Interrupt Remise a zero du timer interne") and returns
; control here via RTI once done -- confirmed directly: loading the real
; ROM, calling this from a standalone CPU instance, and manually driving
; time forward (CPU::tickTimer(), normally driven by the host's real-time
; frame loop -- see main.cpp) correctly halts, wakes on the interrupt,
; and returns.
;
; The 0x57 reload value matches the ROM's own choice exactly (chosen
; here for consistency with real hardware/ROM timing, not derived
; independently) -- see [[pc1500_rom_lcd_print_routine]] for how it was
; found and verified.
;--------------------------------------------------------------------------

	.module idle
	.area CODE

	.globl _idleTick

;-----------------------------------------------------------------
; void idleTick(void);
;
; Sleeps the CPU until the next timer interrupt, then returns. Call
; repeatedly (e.g. `while (!onKeyPressed()) idleTick();`) instead of a
; busy-wait loop.
;-----------------------------------------------------------------
_idleTick:
	ldi	a, 0x57
	am0
	sie
	hlt
	rtn
