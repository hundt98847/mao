Simply add "-B/[MAODIR]/scripts/gcc_hook" to your gcc command line and pass MAO directives using "-Wa,--mao=..."

Example below:

"gcc test.c -o test -B/home/username/mao/scripts/gcc_hook -Wa,--mao=ASM=o[mao_test.s]"

This command will compile your program 'test' and run a MAO asm dumping pass, dumping the asm source into the file 'mao_test.s'. 


-Jason Mars
