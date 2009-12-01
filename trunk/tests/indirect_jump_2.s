#Option:  --mao=TEST --mao=CFG=collect_stats[1] --mao=ASM=o[/dev/null]
#grep table.patterns\:.* 1 1
#
# The following pattern should be identified as a jump table.
# Here, the table needs to be parsed in order to get the correct
# Label targets

        .text
.globl main
        .type   main, @function
main:
	cmpl	$28, %eax
	ja	.L352
	leaq	.L385(%rip), %rdi
	mov	%eax, %eax
	movslq	(%rdi,%rax,4),%rax
	addq	%rdi, %rax
	jmp	*%rax
	.section	.rodata
	.align 4
.L385:
	.long	.L356-.L385
	.long	.L664-.L385
	.long	.L665-.L385
	.long	.L666-.L385
	.long	.L617-.L385
	.long	.L618-.L385
	.long	.L619-.L385
	.text
	ret
	.size	main, .-main
