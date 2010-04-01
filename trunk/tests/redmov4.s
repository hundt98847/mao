#Option: --mao=REDMOV=trace
#grep same 0
        .type foo, @function
foo:
	# shouldn't work because of memory write
	mov	-268(%rbp), %rax
	add	$33, -268(%rbp)
	mov	-268(%rbp), %rbx
