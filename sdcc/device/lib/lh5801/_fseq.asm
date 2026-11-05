;--------------------------------------------------------------------------
; _fseq.asm - float a == b and a != b for the LH5801 port, as SDCC's
; generic _fseq.c and _fsneq.c: the same bits, or two zeros.
;
; Paul Chambre, 2026
;--------------------------------------------------------------------------

	.module _fseq
	.area CODE

	.globl ___fseq
	.globl ___fseq_PARM_1
	.globl ___fseq_PARM_2
	.globl ___fsneq
	.globl ___fsneq_PARM_1
	.globl ___fsneq_PARM_2

___fsneq:
	sjp	___fseq
	eai	a, #0x01
	rtn
___fseq:
	lda	(___fseq_PARM_1 + 3)
	cpa	(___fseq_PARM_2 + 3)
	bzr	fe_zeros
	lda	(___fseq_PARM_1 + 2)
	cpa	(___fseq_PARM_2 + 2)
	bzr	fe_zeros
	lda	(___fseq_PARM_1 + 1)
	cpa	(___fseq_PARM_2 + 1)
	bzr	fe_zeros
	lda	(___fseq_PARM_1)
	cpa	(___fseq_PARM_2)
	bzs	fe_yes
fe_zeros:
	lda	(___fseq_PARM_1)
	ora	(___fseq_PARM_2)
	ani	a, #0x7F
	ora	(___fseq_PARM_1 + 1)
	ora	(___fseq_PARM_2 + 1)
	ora	(___fseq_PARM_1 + 2)
	ora	(___fseq_PARM_2 + 2)
	ora	(___fseq_PARM_1 + 3)
	ora	(___fseq_PARM_2 + 3)
	bzs	fe_yes
	ldi	a, 0x00
	rtn
fe_yes:
	ldi	a, 0x01
	rtn

	.area DATA
___fseq_PARM_1:
___fsneq_PARM_1:
	.ds	4
___fseq_PARM_2:
___fsneq_PARM_2:
	.ds	4
