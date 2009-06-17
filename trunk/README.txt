MAO - An Extensible Micro-Architectural Optimizer

This project seeks to build an infrastructure for micro-architectural
optimizations at the instruction level.  

MAO is a stand alone tool that works on the assembly level. MAO parses
the assembly file, perform all optimizations, and re-emit another
assembly file. After this, the assembler can be invoked to produce a
binary object. MAO reuses much of the code in the GNU Assembler (gas)
and needs binutils-2.19 to build correctly. 

The current MAO version is a an early prototype targeting x86, but
with big plans for the future.


Installation:
-------------
For build instruction, see INSTALL

Files and folders:
------------------

./src/               - Source
      COPYING        - 
      Makefile	     - Makefile for project.
      MaoOptions.cc  - Handles command-line options for mao.
      MaoOptions.h    
      MaoUnit.cc     - The classes handles the hold on compilation
                       unit (i.e. one assembly file)
      MaoUnit.h        Holds the IR.
      SymbolTable.cc - The symbol table used for the compilation unit.
      SymbolTable.h
      as.c           - Taken from gas. Calls the parser
      dwarf2dbg.c    - Taken from gas. Handles debug directives
      ir.cc          - Functions used for passing the data from the gas parser
                       to mao for 
      ir.h             building the compilation unit.
      irlink.h
      mao.cc         - Entry-point
      mao.h
      mao_verify.sh  - Shell script which helps to run mao and verify result.
      obj-elf.c      - Taken from gas. Elf specific directives.
      read.c         - Taken from gas. Machine independent parser.
      symbols.c      - Taken from gas. Handles parsing of symbols.
      tc-i386.c      - Taken from gas. Machine dependent parser.
		     
./benchmarks/        - Simple benchmarks

./bin/               - Holds resulting binaries.

./bniutils-2.19      - Should hold the sources for binutils. Required to build
		       mao correctly.

./binutils-2.19-obj-TARGET  - Holds object files for binutils build for TARGET
                              configure using the following command:
                              ${PATH_TO_binutils-2.19}/configure --target=TARGET
			      and then run make. Target is typically i686 or
                              x86_64


Running mao:
------------

For each target you build mao, a binary called mao-$TARGET will be placed in 
./bin/. Besides the normal flags accepted by GNU Assembler, the following can
be used.

Usage: mao [-mao_o FILE] [-mao_ir FILE] [-mao_v]
  -mao_o FILE       Prints output to FILE.
  -mao_ir FILE      Prints the IR in XML-like format to FILE
  -mao_v            Prints version and usage info, then exits


In order to read and print out assembly file example.s on a x86_64 target to 
stdout, run the following

mao-x86_64 -mao_o /dev/stdout example.s

Hooking mao into gcc:
---------------------

Simply add "-B/[MAODIR]/scripts" to your gcc command line and pass MAO directives
using "-Wa,--mao=..."

Example below:

"gcc test.c -o test -B/home/username/mao/scripts -Wa,--mao=ASM=o[mao_test.s]"

This command will compile your program 'test' and run a MAO asm dumping pass, 
dumping the asm source into the file 'mao_test.s'. 
