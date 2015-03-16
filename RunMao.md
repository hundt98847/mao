
# Running Standalone MAO #

After building MAO, for each build target, a binary called `mao-$TARGET` will be placed in
`./bin/.` For example, building a 64-bit MAO on Linux produces `./bin/mao-x86_64-linux`. The MAO binary is invoked as follows:

```
mao [--mao=mao-options]* [regular-assembler-options]* <input-file>
```

Options specific to MAO are prefixed with `--mao=...`. All other options are passed to the integrated assembler and hence need to be valid options to 'as'.

Another way to pass MAO specific options is to set the environment variable MAOOPTS. This environment variable is parsed before the actual command line parameters. It should only contain MAO specific options, without the `--mao=` prefix.




# Getting help #
Passing --mao=-h or --mao=--help lists the set of available options to MAO. Note that when -h or --help is passed without the --mao=  prefix, it gets passed on to the assembler and prints the assembler's help message.

# Passes #
Everything in MAO including reading the input file, analysis, code transformations and writing the output file is organized in passes. All passes except the file reading pass needs explicit invocation using MAO options. The syntax for the options for a single pass is as follows:

```
pass_name=option1[argument1]+option2+option3[argument3]+...
```

The pass name is separated from its argument list using the = operator. The + symbol is used as the delimiter between the different options. If an option has an argument is is enclosed within square brackets. As a concrete example, the command line

```
--mao=LOOP16=fetch_line_size[16]+limit[100]
```
invokes the LOOP16 pass with two options fetch\_line and limit with arguments 16 and 100 respectively for the two options. Multiple passes (along with their options) can be specified either using separate --mao= options or in a single option separated by : symbol. Thus, --mao=ZEE:LOOP16=fetch\_line\_size[16](16.md) is equivalent to --mao=ZEE --mao=LOOP16=fetch\_line\_size[16](16.md).

Certain options are common to all passes and are listed under a pseudo-pass ALL in the help message:

```
Pass: ALL
  trace     : (int)    Set trace level to 'val' (0..3)
  db[parm]  : (bool)   Dump before a pass
  da[parm]  : (bool)   Dump after  a pass
     with parm being one of:
        cfg : dump CFG, if available
        vcg : dump VCG file, if CFG is available
```

Certain passes that perform analysis, like the pass that build a control flow graph, are used by optimization passes. These passes are **not** specified explicitly in the command line.

# Default Output #
The ASM pass is used to print the resulting assembly file. The default output of the pass is /dev/stdout. To change the output file, the option 'o' to 'ASM' can be used. For example:

```
  # redirect output to stderr
  mao-x86_64-linux --mao=:ZEE=trace:ASM=o[/dev/stderr] input.s

  # write the output to output.s
  mao-x86_64-linux --mao=ASM=o[output.s] input.s

```

# Plugins #

In addition to the passes that come with it, MAO also supports dynamically loadable passes using plugins. A plugin is a dynamically loadable library, containing the implementation of a pass. Using a plugin pass requires an additional --mao=--plugin option along with the options for the pass implemented by the plugin. The example below shows how a plugin that implements a pass named TESTPLUG is loaded and invoked:

```
   ./mao-x86_64-linux --mao=--plugin=/path/to/plugin/MaoTestPlugin-x86_64-linux.so  --mao=TESTPLUG input.s
```

# Integrating MAO with gcc (and other compilers) #
The typical usage of gcc is to transform a source file to an object file or the executable binary. In both cases, gcc first invokes the compiler (cc1 or cc1plus) that produces an assembly file (usually a temporary file in /tmp) and then invokes the GNU assembler 'as'.

For example, to compile foo.cc, the compiler might call:

```
$ gcc -c -o foo.o foo.cc
  ...cc1 -quiet hello.c -o /tmp/cciDot0H.s
  ...as  -V -Qy -o hello.o /tmp/cciDot0H.s
```

MAO optimizations can be applied to the source file by invoking MAO between the invocations of the compiler and the assembler. One way to do this is to replace the 'as' with a script which executes MAO first, and then invoke 'as' on MAO's output. Such a script is available in ` mao/scripts/as `. This script invokes mao only when a --mao= option is passed in the command line. Command lines to this script can be passed from the gcc command line using the -Wa option.

To make gcc invoke this script instead of the actual 'as', one can:

  1. Replace the 'as' directly. For example, rename the original as, and copy the wrapper script 'as' to the location where the original as binary was present.
  1. Recommended: Pass the -B path\_to\_as\_wrapper option to gcc. gcc will look into this path first, thereby invoking the wrapper. As an example, to invoke the ZEE pass on a C file named test.c, invoke
```
$ gcc test.c -o test -B/home/username/mao/scripts -Wa,--mao=ZEE
```

> Note that in this scenario, there is no need to explicitly specify the ASM pass. The 'as' wrapper script passes that ASM option to MAO and specifies the proper input and output files. However, the 'as' script might have to be modified to point to the proper original 'as' binary and to the proper mao binary (look at the top of function main())
  1. Finally, similar to the previous method, instead of passing the -B option, the enviroment variable GCC\_EXEC\_PREFIX can be set.

In all the above use cases, the script requires a symlink named 'as-orig' pointing to the actual as binary and a sym link mao to the mao binary in the same directory as the script.