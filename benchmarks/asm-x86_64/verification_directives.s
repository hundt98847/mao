# Function definition
	.file	"two_functions.c"
	.text
.globl main
	.type	main, @function
main:
Label1:
Label2:
	leave
	ret
	.size	main, .-main

#section and directives
	.section	.eh_frame,"a",@progbits
	.long	0x0
	.string	"zR"
	.sleb128 -8
	.long	.LFB2
	.uleb128 0x0
	.byte	0x4
	.align 8

# expressions
	.long	Label1-Label2
	.long	Label1-Label2	
	.align 8
Label3:
Label4:
	.set   l1, 3
	.equiv l2, 2
	.equ   l3, 1
	.eqv   l4, 4
	
