## ADDADD - Remove Redundant Adds ##
### Source ###
```
  MaoAddAdd.cc
```
### Description ###
This is a local optimization and only operates in a basic block.
The pass seeks to eliminate the first add instruction in the following pattern:
```
       add/sub rX, IMM1
       ...no redef of rX
       add/sub rX, IMM2
```
Yes, it is hard to believe, but we find such instances in gcc 4.4 generated code. Analysis shows that such atrocities are possible, based on phase ordering difficulties in the compiler.

This simple peephole is a good example for instruction manipulation in MAO. The performance effect across our set of benchmarks is, of course, negligible.

### Extensions ###
We could look across the basic block boundaries.