;--------------------------------------------------------------------------
; pc1500_sub.asm - PC-1500 native two-operand floating-point
; subtraction, for the LH5801 SDCC port.
;
; Paul Chambre, 2026
;
; No dedicated ROM subtract routine was found on the PC-2 Assembly
; Language manual's memory-map page (only ADD, MULTIPLY, DIVIDE are
; listed) -- implemented as negate-then-add instead of searching for a
; possibly-nonexistent dedicated routine: flip operand b's sign byte
; (offset 1 of the 8-byte format, see pc1500.h), then call the same
; confirmed ADD routine (0xEFBA, "X + Y -> X") pc1500_add.asm uses.
;--------------------------------------------------------------------------

	.module pc1500_sub
	.area CODE

	.globl _pc1500Sub
	.globl _pc1500Sub_PARM_1
	.globl _pc1500Sub_PARM_2
	.globl __pc1500_fp_copyin
	.globl __pc1500_fp_copyin2
	.globl __pc1500_fp_copyout

;-----------------------------------------------------------------
; void pc1500Sub(pc1500_float_t a, pc1500_float_t b); // a -= b
;-----------------------------------------------------------------
_pc1500Sub:
	lda	(_pc1500Sub_PARM_1)
	sta	xh
	lda	(_pc1500Sub_PARM_1 + 1)
	sta	xl
	sjp	__pc1500_fp_copyin

	lda	(_pc1500Sub_PARM_2)
	sta	xh
	lda	(_pc1500Sub_PARM_2 + 1)
	sta	xl
	sjp	__pc1500_fp_copyin2

	; flip the copied-in second operand's sign byte (0x7A11) -- 0x00 <->
	; 0x80, per the confirmed format (byte 1 = mantissa sign). No A,imm
	; EOR exists on this CPU (only memory-operand forms), so just
	; branch on whichever value is already there instead.
	lda	(0x7A11)
	bzs	__pc1500_sub_was_positive
	ldi	a, 0x00
	sta	(0x7A11)
	bch	__pc1500_sub_flipped
__pc1500_sub_was_positive:
	ldi	a, 0x80
	sta	(0x7A11)
__pc1500_sub_flipped:

	ldi	xh, 0x7a
	ldi	xl, 0x00
	ldi	yh, 0x7a
	ldi	yl, 0x10
	sjp	0xEFBA

	lda	(_pc1500Sub_PARM_1)
	sta	xh
	lda	(_pc1500Sub_PARM_1 + 1)
	sta	xl
	sjp	__pc1500_fp_copyout
	rtn

	.area DATA
_pc1500Sub_PARM_1:
	.ds 2
_pc1500Sub_PARM_2:
	.ds 2
