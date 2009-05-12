#Option:  --mao=RELAX=stat[1] --mao=LOOPALIGN
#grep MaoRelax.*foo.*2 1
# In order to report the correct size, the align directive should not be
# part of the function sizes.
	.type	foo, @function
foo:	
	nop
	nop	
	.size foo, .-foo
	.align 2
	.type	bar, @function
bar:
	nop
