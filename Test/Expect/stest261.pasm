pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_plot
	rdword	_var01, arg01
	shl	_var01, #16
	sar	_var01, #16
	mov	outa, _var01
	add	arg01, #2
	rdword	_var01, arg01
	shl	_var01, #16
	sar	_var01, #16
	mov	outb, _var01
_plot_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
