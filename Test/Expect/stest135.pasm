pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_zero
	cmps	arg02, #1 wc
 if_b	jmp	#LR__0002
	mov	_var01, arg02
LR__0001
	mov	_var02, #0
	wrword	_var02, arg01
	add	arg01, #2
	djnz	_var01, #LR__0001
LR__0002
_zero_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
