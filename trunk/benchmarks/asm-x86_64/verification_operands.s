# addressing modes
 movl 100, %eax
 movl %es:100, %eax
 movl (%eax), %eax
 movl (%eax,%ebx), %eax
 movl (%ecx,%ebx,2), %eax
 movl (,%ebx,2), %eax
 movl -10(%eax), %eax
 movl %ds:-10(%ebp), %eax
 movl %fs:-10(%ebp), %eax

#registers
 mov 0, %eax
 mov 0, %ebx
 mov 0, %ecx
 mov 0, %edx
 mov 0, %esi
 mov 0, %edi
 mov 0, %ebp
 mov 0, %esp

 mov 0, %ax
 mov 0, %bx
 mov 0, %cx
 mov 0, %dx
 mov 0, %si
 mov 0, %di
 mov 0, %bp
 mov 0, %sp

 mov 0, %ah
 mov 0, %bh
 mov 0, %ch
 mov 0, %dh
 mov 0, %al
 mov 0, %bl
 mov 0, %cl
 mov 0, %dl
	
 mov 0, %cs
 mov 0, %ds
 mov 0, %ss
 mov 0, %es
 mov 0, %fs
 mov 0, %gs

 mov 0, %dil
 mov 0, %sil
 mov 0, %bpl
 mov 0, %spl
	
# mov 0, %r8l
# mov 0, %r9l
# mov 0, %r10l
# mov 0, %r11l
# mov 0, %r12l
# mov 0, %r13l
# mov 0, %r14l
# mov 0, %r15l

 mov 0, %r8w
 mov 0, %r9w
 mov 0, %r10w
 mov 0, %r11w
 mov 0, %r12w
 mov 0, %r13w
 mov 0, %r14w
 mov 0, %r15w

 mov 0, %r8d
 mov 0, %r9d
 mov 0, %r10d
 mov 0, %r11d
 mov 0, %r12d
 mov 0, %r13d
 mov 0, %r14d
 mov 0, %r15d

 mov 0, %rax
 mov 0, %rbx
 mov 0, %rcx
 mov 0, %rdx
 mov 0, %rsi
 mov 0, %rdi
 mov 0, %rbp
 mov 0, %rsp

 mov 0, %r8
 mov 0, %r9
 mov 0, %r10
 mov 0, %r11
 mov 0, %r12
 mov 0, %r13
 mov 0, %r14
 mov 0, %r15

 mov (%eip), %eax
 mov (%rip), %eax

# fadd %st, %st(0)
	
 movd 0, %mm0
 movd 0, %mm1
 movd 0, %mm2
 movd 0, %mm3
 movd 0, %mm4
 movd 0, %mm5
 movd 0, %mm6
 movd 0, %mm7

 movd 0, %xmm0
 movd 0, %xmm1
 movd 0, %xmm2
 movd 0, %xmm3
 movd 0, %xmm4
 movd 0, %xmm5
 movd 0, %xmm6
 movd 0, %xmm7
	
 # cr0?
 # dr0?

	

	
