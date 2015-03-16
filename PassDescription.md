

# Optimization Passes #

MAO comes with a set of analysis, transformation, and optimization passes. The invocation order is determined by the user via the command line interface, the order in which passes are specified will be the order in which the passes are executed.

Since MAO is a young project, some passes are developed more than others, and some might not make sense at all. In this document, we keep the current status for the various passes.

## ZEE - Zero Extension Elimination ##
This pass seeks to eliminate redundant zero extensions. It is a local optimization and only operates in a basic block. [More.](ZEE.md)

## REDTEST - Redundant Test Removal ##

This pass seeks to eliminate redundant 'test' instructions within basic blocks. [More.](REDTEST.md)

## REDMOV - Redundant Memory Move Elimination ##

This pass seeks to remove memory operations within basic blocks. [More.](REDMOV.md)

## LOOP16 - Align very tiny loops ##

This pass seeks to align very tight loops to 16-byte boundaries to avoid incurring an instruction fetch penalty. [More.](LOOP16.md)

## NOPKILL - Remove Alignments ##
This pass seeks to eliminate all nops from a function. [More.](NOPKILL.md)

## NOPIN - Insert Random Nops ##
This pass inserts random nops into functions. [More.](NOPIN.md)

## BACKBRALIGN - Back Branch Alignment ##
This pass seeks to align the position of back-edges in 2-deep loop nests, such that the jump instructions reside in different 32-byte aligned block. [More.](BACKBRALIGN.md)

## PREFNTA - Simple Prefetch Insertion ##
This pass inserts a prefetch instruction right in front of every memory load and store operation. [More.](PREFNTA.md)

## INSPREFNTA - Prefetchnta Insertion for Specified Instructions ##

This pass inserts a prefetchnta instruction right before a list of specified instructions. [More.](INSPREFNTA.md)

## SCHEDULER - Instruction Scheduling ##
This pass scheduled instructions at the basic block level. [More.](SCHEDULER.md)

## ADDADD - Remove Redundant Adds ##

The pass seeks to eliminate the first add instruction in the following pattern:
```
       add/sub rX, IMM1
       ...no redef of rX
       add/sub rX, IMM2
```
[More.](ADDADD.md)

## ADD2INC - Convert add|sub 1,reg to inc|dec reg ##
This pass modifies this single instruction pattern. inc|dec are most likely to run slightly slower on modern x86 architectures, but they have a shorter encoding, which might help for certain other passes.
[More.](ADD2INC.md)

## ADD2INC - Convert inc|dec reg to add|sub 1,reg ##
This pass modifies this single instruction pattern. inc|dec are most likely to run slightly slower on modern x86 architectures, but these instructions are still commonly used by inline asm sequences.
> [More.](INC2ADD.md)