# Introduction #

For exception handling and stack unwinding to work correctly, the compiler generates information in the assembly code that can be used by the runtime.

In the MAO context it is important that any transformation do not render an incorrect exception frame. Failure to do so will create errors when exception happens, and debuggers will not be able to correctly unwind the stack.

# Details #

From GCC version 4.4.0, this information is stored in the assembly using .CFI directives.

http://sourceware.org/binutils/docs-2.19/as/CFI-directives.html#CFI-directives

The CFI (Call Frame Information) directives makes it possible to find the current call-frame for any PC and to find all register values saved on the stack.

The following guidelines will make sure no such bug is introduced.
  * Do not modify callee-save register before the corresponding .cfi\_offset directive (currently no convenient way to map register-number to actual registers exists. The mapping can be found in the binutils file i386-tbl.h.)
  * Make sure CFI-directvies are kept next to their associated instruction/label. cfi\_startproc/cfi\_endproc should be kept at the start and end of a function. The cfi\_def directives should be kept after the instruction they are emitted at.