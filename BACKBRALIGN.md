## BACKBRALIGN - Back Branch Alignment ##

### Description ###
This pass seeks to align the position of back-edges in 2-deep loop nests, such that the jump instructions reside in different 32-byte aligned block.

We were able to observe that for short running nested loops, the branch predictors could be confused when the two back branches were on the same 32-byte line.

This pass employs an interesting transformation strategy, based on an idea by Martin Thuresson. Instead of explicitly aligning the backbranch, we simple shift the whole loop down until the back branches end up on different lines.

Performance results so far aren't that great. More study is needed, in particular, aligning the backbranch might be an alternative codegen strategy.

### Options ###
```
  align_limit: (int)    Align to cross this byte boundary
  limit     : (int)    Limit tranformation invocations
```

### Extensions ###
More codegen and alignment strategies need to be explored.

### Source ###
```
  MaoBackBranchAlign.cc
```