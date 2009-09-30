MAO - An Extensible Micro-Architectural Optimizer

This project seeks to build an infrastructure for micro-architectural
optimizations at the instruction level.  

MAO is a stand alone tool that works on the assembly level. MAO parses
the assembly file, perform all optimizations, and re-emit another
assembly file. After this, the assembler can be invoked to produce a
binary object. MAO reuses much of the code in the GNU Assembler (gas)
and needs binutils-2.19.1 to build correctly. 

The current MAO version is a an early prototype targeting x86, but
with big plans for the future.

See http://code.google.com/p/mao for more infomation.
