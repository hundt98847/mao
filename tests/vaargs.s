#Option: --mao=CFG=collect_stats[1] --mao=TEST
#grep VA_ARG patterns    :      1 1

	.file	"vaargs.c"
	.text
	.p2align 4,,15
.globl f
	.type	f, @function
f:
.LFB26:
	pushq	%rbx
.LCFI0:
	subq	$208, %rsp
.LCFI1:
	movq	%rdx, 48(%rsp)
	movzbl	%al, %edx
	movq	%rsi, 40(%rsp)
	leaq	0(,%rdx,4), %rax
	movl	$.L2, %edx
	movq	%rcx, 56(%rsp)
	movq	%r8, 64(%rsp)
	movq	%r9, 72(%rsp)
	movq	%rdi, %rsi
	subq	%rax, %rdx
	leaq	207(%rsp), %rax
	jmp	*%rdx
	movaps	%xmm7, -15(%rax)
	movaps	%xmm6, -31(%rax)
	movaps	%xmm5, -47(%rax)
	movaps	%xmm4, -63(%rax)
	movaps	%xmm3, -79(%rax)
	movaps	%xmm2, -95(%rax)
	movaps	%xmm1, -111(%rax)
	movaps	%xmm0, -127(%rax)
.L2:
	leaq	224(%rsp), %rax
	movq	stdout(%rip), %rbx
	movq	%rsp, %rdx
	movl	$8, (%rsp)
	movl	$48, 4(%rsp)
	movq	%rax, 8(%rsp)
	leaq	32(%rsp), %rax
	movq	%rbx, %rdi
	movq	%rax, 16(%rsp)
	call	vfprintf
	movq	%rbx, %rsi
	movl	$10, %edi
	call	fputc
	addq	$208, %rsp
	popq	%rbx
	ret
.LFE26:
	.size	f, .-f
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"%d %d %d\n"
.LC1:
	.string	"%f\n"
	.text
	.p2align 4,,15
.globl main
	.type	main, @function
main:
.LFB27:
	pushq	%rbx
.LCFI2:
	movl	%edi, %ecx
	movl	%edi, %ebx
	movl	%edi, %edx
	movl	%edi, %esi
	xorl	%eax, %eax
	movl	$.LC0, %edi
	call	f
	cvtsi2sd	%ebx, %xmm0
	movl	$1, %eax
	movl	$.LC1, %edi
	call	f
	xorl	%eax, %eax
	popq	%rbx
	ret
.LFE27:
	.size	main, .-main
	.section	.eh_frame,"a",@progbits
.Lframe1:
	.long	.LECIE1-.LSCIE1
.LSCIE1:
	.long	0x0
	.byte	0x1
	.string	"zR"
	.uleb128 0x1
	.sleb128 -8
	.byte	0x10
	.uleb128 0x1
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
	.long	.LFB26
	.long	.LFE26-.LFB26
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI0-.LFB26
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI1-.LCFI0
	.byte	0xe
	.uleb128 0xe0
	.byte	0x83
	.uleb128 0x2
	.align 8
.LEFDE1:
.LSFDE3:
	.long	.LEFDE3-.LASFDE3
.LASFDE3:
	.long	.LASFDE3-.Lframe1
	.long	.LFB27
	.long	.LFE27-.LFB27
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI2-.LFB27
	.byte	0xe
	.uleb128 0x10
	.byte	0x83
	.uleb128 0x2
	.align 8
.LEFDE3:
	.section	.note.GNU-stack,"",@progbits
