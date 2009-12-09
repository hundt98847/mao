#Option:  --mao=READ=create_anonymous[1] --mao=ZEE=trace
#grep redundant 3

        # valid zero extent with subl       
	subl	$1, %eax	# id: 14862, l: 14875	
	xorl	%ebx, %ebx	# id: 14863, l: 14876	
	movl	%eax, (-44)(%rbp)	# id: 14864, l: 14877	
 	movl	%eax, %eax	# id: 14865, l: 14878	

        # invalid redundant zero-extent
        movq	_ZL2mu(%rip), %rax
        mov	%eax, %eax

        # redundant zero-extent
        mov 	_ZL2mu(%rip), %eax
        mov	%eax, %eax

        # invalid redundant zero-extent
       	movq	_ZL2mu(%rip), %rax
	leaq	-4128(%rbp), %r13
	movl	$_ZL10destructor, %esi
	movl	$512, %ecx
	movq	%r13, %rdi
	mov	%eax, %eax

        # redundant zero-extent
       	mov	_ZL2mu(%rip), %eax
	leaq	-4128(%rbp), %r13
	movl	$_ZL10destructor, %esi
	movl	$512, %ecx
	movq	%r13, %rdi
	mov	%eax, %eax
 
