#Option: --mao=READ=create_anonymous[1] --mao=ADDADD=trace[2]
#Plugin: MaoAddAdd
#grep \[ADDADD\].*Addi/Subi pattern identified 3
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
