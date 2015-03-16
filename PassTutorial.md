# Introduction #

In MAO, one or more passes are applied to the input assembly. Each pass processess, and possibly update the MAO IR before passing it on to the next pass.

This section describes how to create and test your own pass. A pass can either be built in to the MAO binary, or be compiled as a plugin (dynamically loaded library). See the section "Plugin" for specific information about the latter.

# Details #

In the following sections, we will build a pass, one step at the time, explaining the parts as we go along. At the end of the document, we should have a complete pass that prints out the sizes of all inner loops.

There are two types of passes, function passes and unit passes. A unit is the name used for the object representing the whole input object. See the IR wiki for more information.

  * Function pass -- The pass is called for each function and will operate on one function at the time.
  * Unit pass -- The pass is called once for the whole input.

For our pass, a function pass is appropriate.

### Pass essentials ###

This guide will show how to create a pass that prints the size of each inner loop. Lets call this LOOPSIZEINFO.

First, create a file MaoLoopSizeInfo.cc and put the following boiler plate code in it:
```
     1	#include "MaoDebug.h"
     2	#include "MaoPasses.h"
     3	#include "MaoUnit.h"
     4	
     5	namespace {
     6	
     7	// --------------------------------------------------------------------
     8	// Options
     9	// --------------------------------------------------------------------
    10	MAO_OPTIONS_DEFINE(MAOLOOPSIZEINFO, 0) {
    11	};
    12	
    13	class MaoLoopSizeInfoPass : public MaoFunctionPass {
    14	 public:
    15	  MaoLoopSizeInfoPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
    16	      : MaoFunctionPass("MAOLOOPSIZEINFO", options, mao, function), mao_(mao) {
    17	    // Empty
    18	  }
    19	
    20	  bool Go() {
    21	    Trace(3, "Process function: %s", function_->name().c_str());
    22	    return true;
    23	  }
    24	 private:
    25	  MaoUnit *mao_;
    26	};
    27	
    28	REGISTER_FUNC_PASS("MAOLOOPSIZEINFO", MaoLoopSizeInfoPass)
    29	
    30	}  // namespace
```

All function passes needs the following to work.

  * Include MaoDebug.h, MaoPasses.h, and MaoUnit.h
  * Anonymous namespace to avoid classes when loading plugins.
  * Macro defining options used in the pass.
  * Pass-class that inherits from MaoFunctionPass (or MaoUnitPass).
  * Implementation of the bool Go() method that is called for each function (unit).
  * Macro that registers the pass. Macro takes the pass-name and the pass-class.


### Tracing ###
The pass class provides trace methods for simple logging.
```
  virtual void Trace(unsigned int level, const char *fmt, ...) const;
```
The pass-specific option "trace" (defaults to 0) is used to specify what trace messages to print. Only messages with the same or lover level is printed.
In the code above, the Trace message on line 17 is only printed if the trace level is 3 or higher.

### Options ###
Pass specific options are defined using the MAO\_OPTIONS\_DEFINE macro. MAO supports three types of options; integer (int), boolean (bool) and string (str). The following code defines three options (one of each).

```
MAO_OPTIONS_DEFINE(MAOLOOPSIZEINFO, 3) {
  OPTION_STR("string_option", "default", "Description for string option."),
  OPTION_BOOL("boolean_option", false, "Description for boolean option."),
  OPTION_INT("integer_option", 47, "Description for integer option."),
 };
```

The options can be fetched using the following code:
```
  bool string_option = GetOptionString("string_option");
  bool bool_option = GetOptionBool("boolean_option");
  bool int_option = GetOptionInt("integer_option");
```

### Working with the IR ###

Inside the Go() function, we can use the analysis passes and work with the IR:

The control flow graph (CFG) and the loop structure graph (LSG) can be fetched using the following code sequence:
```
  CFG *cfg = CFG::GetCFG(unit_, function_);
  // Make sure CFG is well formed and all jumps are resolved.
  if (!cfg->IsWellFormed()) {
    return false;
  }
  LoopStructureGraph *loop_graph =  LoopStructureGraph::GetLSG(unit_,
                                                               function_);
```

The CFG and LSG are cached, so make sure they are invalidated after modifying the IR:
```
MaoRelaxer::InvalidateSizeMap(function_->GetSection());
CFG::InvalidateCFG(function_);
```

### Complete pass ###
```
     1	#include "MaoCFG.h"
     2	#include "MaoDebug.h"
     3	#include "MaoPasses.h"
     4	#include "MaoRelax.h"
     5	#include "MaoUnit.h"
     6	
     7	namespace {
     8	
     9	// --------------------------------------------------------------------
    10	// Options
    11	// --------------------------------------------------------------------
    12	MAO_OPTIONS_DEFINE(MAOLOOPSIZEINFO, 1) {
    13	  OPTION_BOOL("print_sizes", false, "Print size of each loop."),
    14	};
    15	
    16	class MaoLoopSizeInfoPass : public MaoFunctionPass {
    17	 public:
    18	  MaoLoopSizeInfoPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
    19	      : MaoFunctionPass("MAOLOOPSIZEINFO", options, mao, function), mao_(mao) {
    20	    // Empty
    21	    print_sizes_ = GetOptionBool("print_sizes");
    22	  }
    23	
    24	  bool Go() {
    25	    Trace(3, "Process function: %s", function_->name().c_str());
    26	    CFG *cfg = CFG::GetCFG(unit_, function_);
    27	    // Make sure CFG is well formed.
    28	    if (!cfg->IsWellFormed()) {
    29	      return false;
    30	    }
    31	    LoopStructureGraph *loop_graph =  LoopStructureGraph::GetLSG(unit_,
    32	                                                                 function_);
    33	    MAO_ASSERT(loop_graph);
    34	    // Find inner loops (loops with no child-loops).
    35	    for (SimpleLoop::LoopSet::const_iterator liter
    36	             = loop_graph->root()->ConstChildrenBegin();
    37	         liter != loop_graph->root()->ConstChildrenEnd();
    38	         ++liter) {
    39	      SimpleLoop *loop = *liter;
    40	      if (loop->NumberOfChildren() == 0) {
    41	        Trace(0, "found inner loop in %s", function_->name().c_str());
    42	        if (print_sizes_) {
    43	          // Get the sizes for all entries using the relaxer.
    44	          MaoEntryIntMap *sizes = MaoRelaxer::GetSizeMap(unit_,
    45	                                                         function_->GetSection());
    46	          // Add up the sizes of individual entries to get the size of the loop.
    47	          int loop_size = 0;
    48	          // Iterate over basic blocks.
    49	          for (SimpleLoop::BasicBlockSet::const_iterator iter =
    50	                   loop->ConstBasicBlockBegin();
    51	               iter != loop->ConstBasicBlockEnd(); ++iter) {
    52	            // Iterate over entries inside the basic block.
    53	            FORALL_BB_ENTRY(iter, entry_iter) {
    54	              loop_size += (*sizes)[*entry_iter];
    55	            }
    56	          }
    57	          Trace(0, "size %d", loop_size);
    58	        }
    59	      }
    60	    }
    61	
    62	    return true;
    63	  }
    64	 private:
    65	  MaoUnit *mao_;
    66	  bool print_sizes_;
    67	};
    68	
    69	REGISTER_FUNC_PASS("MAOLOOPSIZEINFO", MaoLoopSizeInfoPass)
    70	
    71	}  // namespace
```

```
$ ../bin/mao-x86_64-linux --mao=MAOLOOPSIZEINFO=trace[3],print_sizes ../tests/loops-1.s [MAOLOOPSIZEINFO]	Process function: foo_with_a_loop
[MAOLOOPSIZEINFO]	found inner loop in foo_with_a_loop
[MAOLOOPSIZEINFO]	size 16

```

### Plugin ###
To compile a pass as a plugin (dynamically loadable library), the following changes are needed.

  * Update Makefile. Specifically, add the plugin source(s) to `PLUGIN_CCSRCS=...` and add the plugin name, without extension, to `PLUGINS` right below
  * Include MaoPlugin.h and add the PLUGIN\_VERSION macro, like in this example:
```
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"
#include "MaoPlugin.h"

namespace {

PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
...
```
  * Create a C-binding MaoInit function and add code to register the pass to it. MaoInit is called on loading of the shared module containing the plugin.
```
// External Entry Point
//
extern "C" {
  void MaoInit() {
    REGISTER_FUNC_PASS("MAOLOOPSIZEINFO", MaoLoopSizeInfoPass)
  }
}
```

Please check existing plugins in src/plugins for more examples.

```
$ ../bin/mao-x86_64-linux --mao=--plugin=../bin/MaoLoopSizeInfo-x86_64-linux.so --mao=MAOLOOPSIZEINFO=trace[3],print_sizes ../tests/loops-1.s 
Loading plugin ../bin/MaoLoopSizeInfo-x86_64-linux.so
[MAOLOOPSIZEINFO]	Process function: foo_with_a_loop
[MAOLOOPSIZEINFO]	found inner loop in foo_with_a_loop
[MAOLOOPSIZEINFO]	size 16
```