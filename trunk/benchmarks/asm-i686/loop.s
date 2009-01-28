	.file	"loop.cc"
	.text
.globl _Z3fooiii
	.type	_Z3fooiii, @function
_Z3fooiii:
.LFB2:
	pushl	%ebp
.LCFI0:
	movl	%esp, %ebp
.LCFI1:
	subl	$16, %esp
.LCFI2:
	movl	$0, -16(%ebp)
	movl	$0, -12(%ebp)
	jmp	.L2
.L5:
	movl	$0, -8(%ebp)
	jmp	.L3
.L4:
	movl	-8(%ebp), %eax
	movl	-12(%ebp), %edx
	leal	(%edx,%eax), %eax
	addl	%eax, -16(%ebp)
	addl	$1, -8(%ebp)
.L3:
	movl	-8(%ebp), %eax
	cmpl	16(%ebp), %eax
	jl	.L4
	addl	$1, -12(%ebp)
.L2:
	movl	-12(%ebp), %eax
	cmpl	12(%ebp), %eax
	jl	.L5
	movl	$0, -4(%ebp)
	jmp	.L6
.L7:
	movl	-4(%ebp), %eax
	imull	12(%ebp), %eax
	subl	%eax, -16(%ebp)
	addl	$1, -4(%ebp)
.L6:
	movl	16(%ebp), %eax
	movl	12(%ebp), %edx
	leal	(%edx,%eax), %eax
	cmpl	-4(%ebp), %eax
	jg	.L7
	movl	-16(%ebp), %eax
	leave
	ret
.LFE2:
	.size	_Z3fooiii, .-_Z3fooiii
	.ident	"GCC: (Google_crosstoolv12-gcc-4.3.1-glibc-2.3.6-grte) 4.3.1"
	.section	.note.GNU-stack,"",@progbits
