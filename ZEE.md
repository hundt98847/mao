## ZEE - Zero Extension Elimination ##
### Source ###
```
  MaoZee.cc 
```
### Description ###
This pass seeks to eliminate redundant zero extensions. It is a local optimization and only operates in a basic block.

In 64-bit compilation models, performing an operation on a 32-bit part of a 64-bit register will, be default, zero-extend the upper 32-bit of that register. An explicit zero-extent operation of the following form is often used in generated code:
```
    movl reg32, same-reg32
```

However, when this mov is preceeded by a 32-bit operation on reg32, this instruction is redundant. For example:
```
[ZEE]   Found redundant zero-extend:
        shrl    $8, %ecx        # id: 132, l: 166       
        movl    %ecx, %ecx      # id: 133, l: 167   
```

This pass checks for possible definitions of reg32 between the 1st assignment and the possinly redundant movl. If there is none, the movl can be eliminated.

### Extensions ###
This pass can be greatly enhanced. It should look across basic blocks for dead zero-extends. We've also identified an overlapping assignment, e.g.,
```
[ZEE]   Overlap
        movq    (8)(%rdi), %rax # id: 5661, l: 6300     
        movl    %eax, %eax      # id: 5662, l: 6301     
```
which is not yet handled.