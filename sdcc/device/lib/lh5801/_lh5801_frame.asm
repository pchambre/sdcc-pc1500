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
;	...
;	sjp	__lh5801_frest		; before RTN
;	.dw	block + n - 1
;	.db	n
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
fr_n:
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
	psh	u
	rtn

__lh5801_frest:
	sta	(fr_a)
	pop	u
	lin	u
	sta	yh
	lin	u
	sta	yl
	lin	u
	sta	(fr_n)
00102$:
	pop	a
	sde	y
	lda	(fr_n)
	dec	a
	sta	(fr_n)
	bzr	00102$
	psh	u
	lda	(fr_a)
	rtn
