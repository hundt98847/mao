#Option: --mao=ADD2INC=trace[2]
#grep Replaced 10

.globl add1
.type	add1, @function

add1:
.LFB0:
 add    $1, %ah
 add    $1, %al
 add    $1, %ax
 addl   $1, %eax
 add    $1, %rax


 sub    $1, %ah
 sub    $1, %al
 sub    $1, %ax
 subl   $1, %eax
 sub    $1, %rax         
