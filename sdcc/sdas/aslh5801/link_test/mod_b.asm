	.area _CODE

	.globl _add_one
	.globl _counter

_start::
	rie
	ldi s,0x47FF
	ldi a,5
	sjp _add_one
	sta (_counter)
loop:
	bch loop
