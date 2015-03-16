

# How to Specify and Define Options for MAO #

Some initial option handling code has been implemented.

  * Mao specific options are passed to MAO via:
> > --mao=option\_string

  * This option\_string cannot contain spaces.

  * Individual options for a pass are separated with a '+'

  * Passes are separated with a ':'. See the examples below.

  * Multiple --mao= options can be specified.

  * All other options on the MAO command line are passed to the integrated assembler.

  * Options can also be specified via the environment variable MAOOPTS. When setting this variable, omit the preceding --mao= part from the example above, only pass in options\_string.

  * Using the command line option --mao=-h displays the help text,  which shows all currently "registered" options. Some options are available for all passes, such as the trace or dump-before and dump-after options. They are summarized in this output under "ALL"

```
Mao 0.1
Usage: mao [--mao=mao-options]+ [regular-assembler-options]+ input-file

'mao-options' are seperated by '+' or ':', and are:

-h          display this help text
-v          verbose (set trace level to 3)
-s          preload all plugins
[...]

PASS=[phase-option][,phase-option]+

with PASS and 'phase-option' being:

Pass: ALL
  enable    : (bool)   En/Disable a pass
  disable   : (bool)   Disable a pass (also: off)
  trace     : (int)    Set trace level to 'val' (0..3)
  db[parm]  : (bool)   Dump before a pass
  da[parm]  : (bool)   Dump after  a pass
     with parm being one of:
        cfg : dump CFG, if available
        vcg : dump VCG file, if CFG is available
Pass: ZEE
Pass: REDMOV
  lookahead : (int)    Look ahead limit for pattern matcher
Pass: REDTEST
...
```

# Invoking Passes with Options #
Passes are invoked when they are specified on the command line. Please remember to specify an ASM pass at the end to force output of an actual asssembly file.
A simple MAO invocation to just parse an assembly file and output it right after that would lool like this:

```
   ../bin/mao-x86_64-linux --mao=ASM=o[/dev/null]:LFIND=vcg+trace[0] loop2.s
```

# Pass-specific Options #

  * To specify a pass and a pass-specific option, please take a look at this self-explanatory example:
```
        --mao=CFG=callsplit+other_opts:ZEE=trace[2]
```

  * Valid pass names must be followed by a '=' sign.

  * Options can have parameters, which can be specified with either one of:
```
        option(value)
        option[value]
```

  * Supported option types are int, bool, and string. Only
> > one value can be passed as a parameter.

  * Boolean options are set to true if no parameter is specified.

  * The -v option set the tracing level for all passes to 3.

  * The trace option sets the tracing level for a given pass to 1.

  * For example, to enable tracing for all passes at level 3, except for pass LFIND, one would specify:
```
        ../bin/mao-x86_64-linux --mao=-v:LFIND=vcg+trace[0]:ASM=o[/dev/null] loop2.s
```

# Plugin Options #
Options can be specified for built-in passes, and or passes that reside in plugins (which are basically dynamically loadable, shared objects).

Options are handled and understood by plugin's implementations (aka, the shared objects). Consequently, before pass options are being specified for passes in dynamically loaded objects, those objects need to be loaded before the options are being specified. Confusing? Here is an example.

Assume we build a plugin from
` plugin/MaoLoopSizeInfo.cc`. The default build system will create this file as output:
`../bin/MaoLoopSizeInfo-x86_64-linux.so`

If we wanted to pass these options to the pass:
` --mao=MAOLOOPSIZEINFO=trace[3],print_sizes`
we have to load the pass first, e.g.:
```
--mao=--plugin=../bin/MaoLoopSizeInfo-x86_64-linux.so \
--mao=MAOLOOPSIZEINFO=trace[3],print_sizes
```

When many passes and options are to be specified, this can be quite a hassle. As a shortcut, MAO offers the `-s` option to essentially preload all shared libraries from
  * MAO's directory (e.g., mao/bin)
  * a lib directory (e.g., mao/lib)
All files names Mao`*`.so will be loaded and scanned to see whether they are a valid MAO plugins.

Back to the example, a shorter command-line would now be (note how combining -s with -v allows you to see which shared objects are being loaded):
```
--mao=-v:-s:--mao=MAOLOOPSIZEINFO=trace[3],print_sizes
```

# Specifying Options in Source #

To add options and enable option processing for a pass in the mao source code, these are the necessary steps:

  * A particular pass must be written as a class and must be derived from
```
        MaoPass
```

  * Options are defined via the MAO\_OPTIONS\_DEFINE macro, such as:
```
      MAO_OPTIONS_DEFINE(CFG,1) {
        OPTION_BOOL("callsplit", false, "Split Basic Blocks at call sites"),
      };
```

  * Any number of options can be specified here as a comma separated list. The number of options must be passed as the second parameter to the MAO\_OPTIONS\_DEFINE macro.

  * The options array is being passed to the MaoPass as a parameter via the MAO\_OPTIONS macro. For example, at time of this writing, the CFG implementation looked like this:
```
      // --------------------------------------------------------------------
      CFGBuilder::CFGBuilder(MaoUnit *mao_unit, MaoOptions *mao_options,
                             Section *section, CFG *CFG)
        : MaoPass("CFG", mao_options, MAO_OPTIONS(CFG)),
          mao_unit_(mao_unit), section_(section), CFG_(CFG), next_id_(0) {
        split_basic_blocks_ = GetOptionBool("callsplit");
      }
```

  * Options should be held as class local variables, and can be "queried" via the GetOption... member functions from MaoPass. These functions are slow, and should therefore be called in the constructor of the pass, once.

The parser is simple, and likely to run into infinite loops for ill-formatted input.