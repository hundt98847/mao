#Option: --mao=READ=create_anonymous[1] --mao=REDTEST=trace[1]
#grep REDTEST 2
        
        subl     $1, %r15d
     	mov	(%rax), %r15
        testl    %r15d, %r15d

        subl     $1, %r15d
        movl    (%rax), %eax     
        testl    %r15d, %r15d

        subl     $1, %r15d
        testl    %r15d, %r15d

