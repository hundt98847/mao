#Option: --mao=CFG=collect_stats[1]  --mao=ASM=o[/dev/null] --mao=TEST
#grep table.patterns\:.* 1 1

	.file	"switches.c"
	.text
	.p2align 4,,15
.globl f1
	.type	f1, @function
f1:
.LFB2:
	leal	3(%rdi), %eax
	ret
.LFE2:
	.size	f1, .-f1
	.p2align 4,,15
.globl f2
	.type	f2, @function
f2:
.LFB3:
	cmpl	$6, %edi
	jbe	.L13
.L4:
	movl	%edi, %eax
	ret
	.p2align 4,,10
	.p2align 3
.L13:
	mov	%edi, %eax
	jmp	*.L9(,%rax,8)
	.section	.rodata
	.align 8
	.align 4
.L9:
	.quad	.L5
	.quad	.L6
	.quad	.L4
	.quad	.L7
	.quad	.L5
	.quad	.L8
	.quad	.L5
	.text
	.p2align 4,,10
	.p2align 3
.L8:
	movl	$36, %eax
	.p2align 4,,6
	.p2align 3
	ret
	.p2align 4,,10
	.p2align 3
.L7:
	xorl	%eax, %eax
	.p2align 4,,6
	.p2align 3
	ret
	.p2align 4,,10
	.p2align 3
.L6:
	movl	$2, %eax
	.p2align 4,,7
	.p2align 3
	ret
	.p2align 4,,10
	.p2align 3
.L5:
	movl	$3, %eax
	.p2align 4,,7
	.p2align 3
	ret
.LFE3:
	.size	f2, .-f2
	.p2align 4,,15
.globl main
	.type	main, @function
main:
.LFB4:
	pushq	%rbx
.LCFI0:
	leal	3(%rdi), %ebx
	movl	%ebx, %edi
	call	f2
	leal	(%rbx,%rax), %eax
	popq	%rbx
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
	.align 8
.LEFDE1:
.LSFDE3:
	.long	.LEFDE3-.LASFDE3
.LASFDE3:
	.long	.LASFDE3-.Lframe1
	.long	.LFB3
	.long	.LFE3-.LFB3
	.uleb128 0x0
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
	.long	.LCFI0-.LFB4
	.byte	0xe
	.uleb128 0x10
	.byte	0x83
	.uleb128 0x2
	.align 8
.LEFDE5:
	.section	.note.GNU-stack,"",@progbits
