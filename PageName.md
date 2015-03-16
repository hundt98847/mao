## SCHED - Instruction Scheduling ##
### Description ###
This pass scheduled instructions at the basic block level. The scheduler first splits the entries in a basic block into scheduler nodes. A scheduler node is a consecutive sequence of entries that need to be together to preserve program semantics. Then a dependence DAG is constructed between the scheduler nodes. Memory dependences are conservatively handled by treating any two memory operations to be dependent on each other. A list scheduling algorithm is used to schedule the nodes of the dependence DAG. It considers the set of available nodes (nodes whose predecessors are scheduled) and picks one based on some4 priority.

The scheduler pass can support any metric to prioritize the nodes. The current implementation uses two metrics to prioritize the nodes. First, it uses a dependence height metric for the purpose. The leaf nodes are assigned a dependence height of zero. A non-leaf node is assigned a weight which is one more than the maximum weight of all its children. While all dependences (flow, anti and output) dependences are respected for correctness, only flow dependences are used for computing dependence heights. The dependence heights od nodes with loop-carried dependences inside small loops are bumped higher. The idea behind using the dependence height is that if there are many instructions dependent on a currently executing instruction, it is important that a dependent instruction that is on the critical path appears next on the instruction streams to avoid reservation stalls on some Intel X86 processors.

The second measure used by the algorithm tries to schedule a node one of whose source registers is the same as that of a recently scheduled node's destination. Intel's x86 manuals say that it is better for a register to be forwarded from a recently executed instruction than to read it from the register file due to limited number of register read ports.

The scheduler pass has resulted in significant speedups (as high as 12%) on important routines.
### Options ###
```
 function_list: (string) A comma separated list of mangled function names on which this pass is applied. An empty string means the pass is applied on all functions
  functions_file: (string) A file with a list of mangled function names. The position in this file gives a unique number to the functions
  start_func: (int)    Number of the first function in functions_file that is optimized.
  end_func  : (int)    Number of the last  function in functions_file that is optimized
  max_steps : (int)    Maximum number of scheduling operations performed in any function
```

These options mainly assist in debugging the scheduler by limiting the code region on which scheduling is applied.

### Extensions ###
Many extensions are possible to the scheduler pass. First, the scope of the scheduling can be enlarged from basic-block level to the function level.  If instructions are scheduled across basic blocks, then it would require inserting compensation code, thereby increasing the complexity. An alternative would be to compute the dependence heights at the function level and do the scheduling at the BB level. Another major improvement would be to identify all other scenarios where scheduling improves performance and implement them. During our experiments we have seen cases where swapping two independent instructions improve performance, but it is not clear why that happens.

### Source ###
```
  MaoScheduler.cc
```