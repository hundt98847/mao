## REDTEST - Redundant Test Removal ##

### Description ###

This pass seeks to eliminate redundant test instructions.  It is a local optimization and only operates in a basic block.

Within gcc, for x86, flags are not modeled with best precision. Instead, the flags are all lumped together in one pseudo operand in the compiler IR.

Correspondingly, the compiler can oftentimes not properly determine whether a test instructions is needed, or whether flags have been set properly by a preceding operation. As a result, we find many patterns of this form:

```
   subl     xxx, %r15d
   ... instructions not setting flags
   testl    %r15d, %r15d
```

The pass implementation is fairly straightforward. It finds test instructions with identical registers, and then peeks upwards, passing (only) over mov instructions (not defining the register used in the test instructions), which do not set flags, until it finds an add, sub, and, or, xor, or sbb instruction defining the same register. If such test instruction is found, it is removed.

### Options ###
None.

### Extensions ###
This pass can be extended to handle the sar, shr, sal, shl as well. However, the flag logic for these operations is non-canonical, and proper handling will be difficult. The pass can also be extended to work across basic blocks (expect little benefit)

### Source ###
```
  MaoRedundantTestElim.cc
```