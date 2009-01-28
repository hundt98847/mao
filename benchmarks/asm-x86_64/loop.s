	.file	"loop.cc"
	.text
.globl _Z3fooiii
	.type	_Z3fooiii, @function
_Z3fooiii:
.LFB2:
	pushq	%rbp
.LCFI0:
	movq	%rsp, %rbp
.LCFI1:
	movl	%edi, -20(%rbp)
	movl	%esi, -24(%rbp)
	movl	%edx, -28(%rbp)
	movl	$0, -16(%rbp)
	movl	$0, -12(%rbp)
	jmp	.L2
.L5:
	movl	$0, -8(%rbp)
	jmp	.L3
.L4:
	movl	-8(%rbp), %eax
	movl	-12(%rbp), %edx
	leal	(%rdx,%rax), %eax
	addl	%eax, -16(%rbp)
	addl	$1, -8(%rbp)
.L3:
	movl	-8(%rbp), %eax
	cmpl	-28(%rbp), %eax
	jl	.L4
	addl	$1, -12(%rbp)
.L2:
	movl	-12(%rbp), %eax
	cmpl	-24(%rbp), %eax
	jl	.L5
	movl	$0, -4(%rbp)
	jmp	.L6
.L7:
	movl	-4(%rbp), %eax
	imull	-24(%rbp), %eax
	subl	%eax, -16(%rbp)
	addl	$1, -4(%rbp)
.L6:
	movl	-28(%rbp), %eax
	movl	-24(%rbp), %edx
	leal	(%rdx,%rax), %eax
	cmpl	-4(%rbp), %eax
	jg	.L7
	movl	-16(%rbp), %eax
	leave
	ret
.LFE2:
	.size	_Z3fooiii, .-_Z3fooiii
	.section	.eh_frame,"a",@progbits
.Lframe1:
	.long	.LECIE1-.LSCIE1
.LSCIE1:
	.long	0x0
	.byte	0x1
.globl __gxx_personality_v0
	.string	"zPR"
	.uleb128 0x1
	.sleb128 -8
	.byte	0x10
	.uleb128 0x6
	.byte	0x3
	.long	__gxx_personality_v0
	.byte	0x3
	.byte	0xc
	.uleb128 0x7
	.uleb128 0x8
	.byte	0x90
	.uleb128 0x1
	.align 8
.LECIE1:
.LSFDE1:
	.long	.LEFDE1-.LASFDE1
.LASFDE1:
	.long	.LASFDE1-.Lframe1
	.long	.LFB2
	.long	.LFE2-.LFB2
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI0-.LFB2
	.byte	0xe
	.uleb128 0x10
	.byte	0x86
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI1-.LCFI0
	.byte	0xd
	.uleb128 0x6
	.align 8
.LEFDE1:
	.ident	"GCC: (Google_crosstoolv12-gcc-4.3.1-glibc-2.3.6-grte) 4.3.1"
	.section	.note.GNU-stack,"",@progbits
