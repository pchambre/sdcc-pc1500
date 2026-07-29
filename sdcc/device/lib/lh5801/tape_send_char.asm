;--------------------------------------------------------------------------
; tape_send_char.asm - send one byte to cassette tape via a CE-150
; expansion module, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; VMJ 0xA8 (SBRA8 per /home/paul/Documents/PC1500/ASMADDR.txt's vector
; table) jumps to the *module's* own address 0xBDC7, reached through a
; 3-byte JMP trampoline at 0xB88B inside the real CE-150 ROM (dumped and
; disassembled directly this session) -- this is NOT in the base system
; ROM at all, so it only works with a CE-150 (or a CE-158, if it shares
; this same vector slot -- unconfirmed) module loaded and selected via
; PU/PV (see pc1500emu's Bus::RomModule; CE-150 answers at PV=low, its
; documented default).
;
; Confirmed by direct emulation (two different input bytes, watching
; every write to #(0xF006), the LH5811 serial-transmit trigger
; register): the routine sends the byte's low nibble as (0xF0 | nibble),
; then its high nibble the same way, then a fixed (0xF0),(0xF0) trailer
; -- four transmissions total, the last two constant regardless of the
; input byte. Waits for the TD-ready flag (IF bit 3) and polls the
; ROM's own BREAK-key test (SBRA6) between each nibble, so this can take
; a while and is abortable by BREAK on real hardware -- callers don't
; need to do anything special for that; VMJ 0xA8 will simply return
; (carry set, per BDD8's "bzr 0xBD78" abort path) if BREAK was pressed
; mid-transmission. This backend doesn't yet check/report that carry
; flag back to C -- see the header's own comment.
;--------------------------------------------------------------------------

	.module tape_send_char
	.area CODE

	.globl _tapeSendChar
	.globl _tapeSendChar_PARM_1

;-----------------------------------------------------------------
; void tapeSendChar(unsigned char c);
;-----------------------------------------------------------------
_tapeSendChar:
	lda	(_tapeSendChar_PARM_1)
	sta	yh
	vmj	0xA8
	rtn

	.area DATA
_tapeSendChar_PARM_1:
	.ds 1
