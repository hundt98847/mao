## NOPKILL - Remove Alignments ##

### Description ###
This pass seeks to eliminate all nops from a function. In particular, it eliminates:
```
     nop
     nopl
     xchg %ax, %ax
     xchg %eax, %eax
     xchg %rax, %rax
     .p2align ...
```

We wanted to study the performance effects of removing such compiler inserted alignments. While we found that code size can be reduced by about 2% be removing these things. There are correctness issues, as some codes rely on a certain code layout. So far, this pass is good for experimentation, in particular for the other alignment passes, but it's not ready for prime time.

### Source ###
```
  MaoNopKiller.cc
```