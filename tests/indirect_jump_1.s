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
        pushq   %rbp
        movq    %rsp, %rbp

	movq    $2, %rdi

	leaq	TABLE(%rip), %rdx
	mov	%edi, %eax
	movslq	(%rdx,%rax,4),%rax
	addq	%rdx, %rax
	jmp	*%rax
	.section	.rodata
	.align 4
	.align 4
TABLE:
	.long	L8-TABLE
	.long	L6-TABLE
	.long	L5-TABLE
	.long	L6-TABLE
	.long	L7-TABLE
	.long	L8-TABLE
	.long	L8-TABLE
	.long	L10-TABLE
	.text
L5:
	nop	
L6:
	nop
L7:
	nop
L8:
	nop
L9:
	nop
L10:
        movl    $0, %eax
        leave
        ret
