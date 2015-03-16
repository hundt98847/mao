## NOPIN - Insert Random Nops ##
### Description ###
This pass inserts random nops into functions.

The idea is to find micro-architectural issues with this methodology. Using various parameters for density and distribution of nops, for example loops will end up being aligned in ways that would not be found via regular compilations.

Experiments with this pass quickly found a 3% opportunity in a compression benchmark. We have also seen around 7% performance difference caused by the insertion of a single nop in another benchmark.

### Options ###
```
  seed      : (int)    Seed for random number generation
  density   : (int)    Density for inserts, random, 1 / 'density' insn
  thick     : (int)    How many nops in a row, random, 1 / 'thick'
```

### Extensions ###
Many more experiments need to be run. Smarter nop insertion policies should be tried out, in particular, around branches and branch targets.

### Source ###
```
  MaoNopinizer.cc
```