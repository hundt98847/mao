#Option: --mao=ADDADD=trace
#grep \[ADDADD\].*Found two immediate adds 3
#
# Three patterns should match in this code
# 
#

bar:	
	addl    $2, %eax
	nop
	addl    $2, %eax
foo:
	jmp bar
	addl    $2, %eax
	nop
	sub    $2, %ebx
	nop
	nop
	sub    $2, %ebx	
	sub    $2, %eax
	nop
