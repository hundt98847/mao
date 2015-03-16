# Verification Strategy #

MAO reads the assembly and represents the compilation unit (called a MaoUnit) using its own Internal Representation (IR). It is from this IR the final emitted assembly is generated.

To make sure that the MAO IR captures everything in the assembly input, a design goal is to make sure that the output from MAO, when no optimization is applied, should be equivalent to the input assembly. With this we mean that the resulting object files generated from the input and the output should be binary equivalent.

# Details #

Most instruction and directives are represented in the IR by one of the classed derived from MaoEntry. To make working with the IR easy, we want to avoid having entries for attributes (such as .globl, .local, .comm, and .type) and instead store these attributes as members on the corresponding object. Since the order of these directives will matter in the object-file, the approach used in MAO is to generate a set of assembly-lines at the top of the output that makes sure that the symbol-table will have the same order as the input, regardless of the IR.

At the top of each emitted file, the header will have the following:

  * List of all sections in the same order as they appeared in the input.
Within each section, local symbols will be listed.
  * List of all global symbols, in the same order as they are found in the input.
  * List of comm symbols, in the order as they are found in the input.
  * List of all the type directives in the order as they are found in the input.

Because of the way .ident works, it is handled a bit differently compared to other entries. Since it modifies the comment section, it is listed among the other sections in the header. Even though its in the IR, it must not be outputted again when writing out the IR.