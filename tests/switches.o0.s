#Option: --mao=CFG=collect_stats[1]  --mao=ASM=o[/dev/null] --mao=TEST
#grep table.patterns\:.* 1 1
	
	.file	"switches.c"
	.text
.globl f1
	.type	f1, @function
f1:
.LFB2:
	pushq	%rbp
.LCFI0:
	movq	%rsp, %rbp
.LCFI1:
	movl	%edi, -20(%rbp)
	movl	-20(%rbp), %eax
	addl	$3, %eax
	movl	%eax, -4(%rbp)
	movl	-4(%rbp), %eax
	leave
	ret
.LFE2:
	.size	f1, .-f1
.globl f2
	.type	f2, @function
f2:
.LFB3:
	pushq	%rbp
.LCFI2:
	movq	%rsp, %rbp
.LCFI3:
	movl	%edi, -20(%rbp)
	cmpl	$6, -20(%rbp)
	ja	.L4
	mov	-20(%rbp), %eax
	movq	.L11(,%rax,8), %rax
	jmp	*%rax
	.section	.rodata
	.align 8
	.align 4
.L11:
	.quad	.L5
	.quad	.L6
	.quad	.L4
	.quad	.L7
	.quad	.L8
	.quad	.L9
	.quad	.L10
	.text
.L5:
	movl	$3, -4(%rbp)
	jmp	.L12
.L6:
	movl	$2, -4(%rbp)
	jmp	.L12
.L7:
	movl	$0, -4(%rbp)
	jmp	.L12
.L8:
	movl	$3, -4(%rbp)
	jmp	.L12
.L9:
	movl	$36, -4(%rbp)
	jmp	.L12
.L10:
	movl	$3, -4(%rbp)
	jmp	.L12
.L4:
	movl	-20(%rbp), %eax
	movl	%eax, -4(%rbp)
.L12:
	movl	-4(%rbp), %eax
	leave
	ret
.LFE3:
	.size	f2, .-f2
.globl main
	.type	main, @function
main:
.LFB4:
	pushq	%rbp
.LCFI4:
	movq	%rsp, %rbp
.LCFI5:
	subq	$32, %rsp
.LCFI6:
	movl	%edi, -20(%rbp)
	movq	%rsi, -32(%rbp)
	movl	$0, -4(%rbp)
	movl	-20(%rbp), %edi
	call	f1
	addl	%eax, -4(%rbp)
	movl	-4(%rbp), %edi
	call	f2
	addl	%eax, -4(%rbp)
	movl	-4(%rbp), %eax
	leave
	ret
.LFE4:
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
.LSFDE3:
	.long	.LEFDE3-.LASFDE3
.LASFDE3:
	.long	.LASFDE3-.Lframe1
	.long	.LFB3
	.long	.LFE3-.LFB3
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI2-.LFB3
	.byte	0xe
	.uleb128 0x10
	.byte	0x86
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI3-.LCFI2
	.byte	0xd
	.uleb128 0x6
	.align 8
.LEFDE3:
.LSFDE5:
	.long	.LEFDE5-.LASFDE5
.LASFDE5:
	.long	.LASFDE5-.Lframe1
	.long	.LFB4
	.long	.LFE4-.LFB4
	.uleb128 0x0
	.byte	0x4
	.long	.LCFI4-.LFB4
	.byte	0xe
	.uleb128 0x10
	.byte	0x86
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI5-.LCFI4
	.byte	0xd
	.uleb128 0x6
	.align 8
.LEFDE5:
	.section	.note.GNU-stack,"",@progbits
