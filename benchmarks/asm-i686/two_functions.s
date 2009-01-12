	.file	"two_functions.c"
	.text
.globl foo
	.type	foo, @function
foo:
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %eax
	addl	$2, %eax
	popl	%ebp
	ret
	.size	foo, .-foo
.globl foo2
	.type	foo2, @function
foo2:
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %eax
	addl	$4, %eax
	popl	%ebp
	ret
	.size	foo2, .-foo2
.globl main
	.type	main, @function
main:
	leal	4(%esp), %ecx
	andl	$-16, %esp
	pushl	-4(%ecx)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx
	subl	$4, %esp
	movl	$1, (%esp)
	call	foo
	movl	%eax, %ebx
	movl	$2, (%esp)
	call	foo2
	leal	(%ebx,%eax), %eax
	addl	$4, %esp
	popl	%ecx
	popl	%ebx
	popl	%ebp
	leal	-4(%ecx), %esp
	ret
	.size	main, .-main
	.section	.note.GNU-stack,"",@progbits
