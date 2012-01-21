#Option: --mao=INC2ADD=trace[2]
#grep Replaced 10

.globl add1
.type	add1, @function

add1:
.LFB0:
        movl	$inName, %ecx

        inc %ah
        inc %al
        inc %ax
        inc %eax
        inc %rax

        dec %ah
        dec %al
        dec %ax
        dec %eax
        dec %rax

	.data
	.align 32
	.type	zSuffix, @object
	.size	zSuffix, 32

	.comm	inName,1034,32
