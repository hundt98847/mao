#Option: --mao=REDMOV=trace	
#grep same 2

        .type foo, @function
foo:
        # shouldn't work, eax is subreg of rax.
        movq    (%rax), %rax
        movq    (%rax), %rax
        
        # shouldn't work, eax is subreg of rax.
        movq	(32)(%rbp), %rax	
	movl	(%rax), %eax	 
	movq	(32)(%rbp), %rax	

        # should work.
        movq	(32)(%rbp), %rax	
	movl	(%rax), %ebx	 
	movq	(32)(%rbp), %rax	

        # shouldn't work, index register rax is redefined
       	movl	(%rax), %edx	 
	movq	(-16)(%rbp), %rax	
        movl	(%rax), %eax

        # should work.
        movl	(%rax), %edx	 
	movq	(-16)(%rbp), %rbx	
	movl	(%rax), %eax	

        # should not work:
        movq    (%rax), %rax     # [625], line: 640     movq (%rax),%rax
        movq    (%rax), %rax     # [626], line: 641     movq (%rax),%rax



