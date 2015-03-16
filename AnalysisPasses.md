# Introduction #

MAO currently provides the following analysis passes/classes.

## Control Flow Graph, CFG ##
The CFG pass calculates the control flow graph for a function.

## Loops ##
The loop pass identifies all loops in a function, and their nesting relationship.

## Relaxer ##
The relaxer calculates the sizes of all entries within a section and returns it as either a sizemap or an offset map.

# Details #

## CFG ##

The Control Flow Graph pass divides a function into basic blocks, and creates a graph representing all possible jumps between them.

Indirect jumps are resolved by identifying compiler-generated generated code sequences. When MAO finds a jump where it is unable to determine all possible targets, the CFG is marked as not being _well formed_.


The CFG is calculated per function, and is cached in the MaoUnit. The following code can be used to get the CFG for a given function:
```
CFG *cfg = CFG::GetCFG(unit_, function_);
```

The most common operation is to operate is to iterate over the basic blocks and entries in the CFG. The following code goes through all instructions in the CFG:
```
  FORALL_CFG_BB(cfg,it) {
      MaoEntry *first = (*it)->GetFirstInstruction();
      if (!first) continue;

      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();
        // TODO: Process instruction here
      }
  }
```

If changes are made the MaoUnit that causes the CFG to be inaccurate, it should be invalidated.

```
  static void CFG::InvalidateCFG(Function *function);
```

To access the in and outedges for a basic block:
```
  for (BasicBlock::ConstEdgeIterator inedges = bb->BeginInEdges();
       inedges != bb->EndInEdges(); ++inedges) {
    BasicBlockEdge *edge   = *inedges;
    BasicBlock     *bb_source = edge->source();
  }
  for (BasicBlock::ConstEdgeIterator outedges = bb->BeginOutEdges();
       outedges != bb->EndOutEdges(); ++outedges) {
    BasicBlockEdge *edge   = *outedges;
    BasicBlock     *bb_dest   = edge->dest();
  }
```


See MaoCFG.h for more info about the ways to access the CFG.

## Loops ##
The LFINDER pass implements an interfaces to loop related functionality in MAO
using the following classes:
  * SimpleLoop - a class representing a single loop in a routine
  * LoopStructureGraph - a class representing the nesting relationships of all loops in a routine.

The loops from a function can be accessed using the GetLSG() and iterators can be used to iterate over individual loops:
```
  LoopStructureGraph *loop_graph =  LoopStructureGraph::GetLSG(unit_,
                                                               function_);

  for (SimpleLoop::LoopSet::const_iterator liter
           = loop_graph->root()->ConstChildrenBegin();
       liter != loop_graph->root()->ConstChildrenEnd();
       ++liter) {
    SimpleLoop *loop = *liter;
    //TODO: Work with loop
  }
```

For a given loop, the following code iterates over its basic block.

```
  for (SimpleLoop::BasicBlockSet::const_iterator iter =
       loop->ConstBasicBlockBegin();
       iter != loop->ConstBasicBlockEnd(); ++iter) {
    BasicBlock *bb = *iter;
    // TODO: Process basic block bb.
  }

```

See MaoLoops.h for more info on how to access the loops.

## Relaxer ##

The relaxer calculates the sizes of all instructions inside a section. The name comes from the algorithm used.

Sizes are stored in a dictionary that maps MaoEntry to integers, (MaoEntryIntMap).
Sizes are always calculated for a section at the time, so even if you only care about a function, you need to pass the whole section:
```
    MaoEntryIntMap *sizes = MaoRelaxer::GetSizeMap(unit_,
                                                   function_->GetSection());
    int size = (*sizes)[entry];
```

Note that the size of some instructions depends on the offset from the instruction to a label it references. This means that a change in one instruction might change the sizes of other instructions. Also, the sizemap is cached between calls, so it is important to invalidate the sizemap in case you edit the IR:
```
  static void MaoRelxaer::InvalidateSizeMap(Section *section);

```

The relaxer also provides an offset-map for each section. It also uses an MaoEntryIntMap, but instead stores the offset of the entry from the beginning of the section.
```
    MaoEntryIntMap *offsets = MaoRelaxer::GetOffsetMap(unit_,
                                                       function_->GetSection());
    int offset = (*offsets)[entry];
```

For more info about the relaxer, see MaoRelax.h