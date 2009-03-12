#Option: -mao:REDMOV=enable,trace
#grep REDMOV 0
        .type foo, @function
foo:
        # shouldn't work, eax is subreg of rax.
        movq	(32)(%rbp), %rax	
	movl	(%rax), %eax	 
	movq	(32)(%rbp), %rax	
