#Option: --mao=UOPSCMPJMP=trace[2]+offset_min[25]
#grep Insert 1
#grep Found 3

.globl cmp
.type	cmpjne_no, @function

cmpjne_no:
.LFB0:
          mov    %edx,%r12d
          add    $0x1,%rax
          movzbl (%rax),%edx
          mov    %r12b,(%rax)
          cmp    %dl,%r8b
          jne    dummy1
        
          add    $1, %ah
          add    $1, %al
          add    $1, %ax
          addl   $1, %eax
          add    $1, %rax
dummy1: 
          sub    $1, %ah
          sub    $1, %al
          sub    $1, %ax
          subl   $1, %eax
          sub    $1, %rax         


.type	cmpjne, @function

cmpjne:
.LFB1:
          add    $1, %ah
          add    $1, %al
          add    $1, %ax
          addl   $1, %eax
          add    $1, %rax
          add    $1, %ah
          add    $1, %al
          add    $1, %ax
          addl   $1, %eax
          add    $1, %rax
        
        
          mov    %edx,%r12d
          add    $0x1,%rax
          movzbl (%rax),%edx
          mov    %r12b,(%rax)
          cmp    %dl,%r8b
          jne    dummy2
        
dummy2: 
          sub    $1, %ah
          sub    $1, %al
          sub    $1, %ax
          subl   $1, %eax
          sub    $1, %rax         
