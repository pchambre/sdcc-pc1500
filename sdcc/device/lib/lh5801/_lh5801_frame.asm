;--------------------------------------------------------------------------
; _lh5801_frame.asm - the LH5801 backend's reentrant-frame save/restore.
;
; Paul Chambre, 2026
;
; A reentrant function's locals are static, like every other function's
; (no indexed addressing to reach a frame on the stack), so recursion
; would overwrite the caller's. gen.c's collectFrameSave() picks the ones
; that must survive a call, overlay.c lays them out as one block, and the
; function pushes the block at entry and pops it back before it returns:
;
;	sjp	__lh5801_fsave		; at entry
;	.dw	block
;	.db	n
;	.db	slots
;	...
;	sjp	__lh5801_frest		; before RTN
;	.dw	block + n - 1
;	.db	n
;	.db	slots
;
; `slots` more bytes are reserved above the block, uninitialized: the
; function's stack slots (ralloc.c's planSlots()), for the locals a
; pointer can reach -- at S + 1 when fsave returns.
;
; Both take the return address SJP pushed (POP U), read the inline
; operands with LIN U and return with PSH U / RTN -- which leaves the
; bytes they pushed, or removes the ones they popped, under the
; function's own return address. fsave clobbers A, X, Y and U; frest
; keeps A and X (the return value) and clobbers Y and U.
;--------------------------------------------------------------------------

	.module _lh5801_frame

	.area DATA
fr_a:
	.ds 1
fr_xh:
	.ds 1
fr_xl:
	.ds 1

	.area CODE

	.globl __lh5801_fsave
	.globl __lh5801_frest

__lh5801_fsave:
	pop	u
	lin	u
	sta	yh
	lin	u
	sta	yl
	lin	u
	sta	xl
00101$:
	lin	y
	psh	a
	dec	xl
	bzr	00101$
	lin	u
	sta	xl
	ani	a, #0xff
	bzs	00104$
00103$:
	psh	a
	dec	xl
	bzr	00103$
00104$:
	psh	u
	rtn

; The return value's A and X wait in memory, so XL can count both loops
; (2026-11-04: counting in memory was 1.7 times fsave's time).
__lh5801_frest:
	sta	(fr_a)
	lda	xh
	sta	(fr_xh)
	lda	xl
	sta	(fr_xl)
	pop	u
	lin	u
	sta	yh
	lin	u
	sta	yl
	lin	u
	sta	xh			; n
	lin	u
	sta	xl			; slots
	ani	a, #0xff
	bzs	00102$
00105$:
	pop	a
	dec	xl
	bzr	00105$
00102$:
	lda	xh
	sta	xl
00103$:
	pop	a
	sde	y
	dec	xl
	bzr	00103$
	psh	u
	lda	(fr_xh)
	sta	xh
	lda	(fr_xl)
	sta	xl
	lda	(fr_a)
	rtn
