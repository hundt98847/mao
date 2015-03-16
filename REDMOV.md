## REDMOV - Redundant Memory Move Elimination ##

### Description ###
This pass seeks to remove memory operations. It is a local pass and only works in a basic block.

Because of various (phase ordering) issues, compiler oftentimes generate code of the following form to load from memory:
```
    movq    24(%rsp), %rdx
    ... no def for this memory,
    ... no def for the right hand side register %rdx,
    movq    24(%rsp), %rcx
```

We find that the 2nd movg address operation is not necessary and the instruction can be replaced by:
```
  //  movq    %rdx, %rcx
```
This instruction has a shorter encoding and avoids a 2nd memory reference.


The implementation of this pass is somewhat interesting, as it has to check, whether the address in the instruction aliases with any instruction in between the two moves. It makes use of the infrastructure to determine whether registers overlap and/or are defined between the instructions.

### Options ###
```
lookahead : (int)    Look ahead limit for pattern matcher
```

### Caveats ###
The current implementation might be a little too optimistic. There is a question around multi-threaded code. We believe the transformation is legal for the current C++ memory model. There is also an issue around performance, this pass does not seem to help. Work in progress.

### Source ###
```
  MaoRedundantMemMove.cc
```