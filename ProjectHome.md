## Overview ##
This project seeks to build an infrastructure for micro-architectural optimizations at the instruction level.

MAO is a stand alone tool that takes assembly code as input and translates it to an IR that optimization and analysis passes can work with. MAO can then output information/statistics about the input as well as the transformed assembly code. It reuses code from the binutils project. Please see the wiki-pages below for detailed information.

The current MAO version is a fully functioning prototype targeting x86, with big plans for the future. It is deployed in production at Google.

For discussions, please use **mao-project@googlegroups.com**

## Research Papers ##

[MAO — An extensible micro-architectural optimizer](http://ieeexplore.ieee.org/xpls/abs_all.jsp?arnumber=5764669&tag=1), CGO 2012, Robert Hundt,  Easwaran Raman, Martin Thuresson, Neil Vachharajani, Google Inc.

[Compiler techniques to improve dynamic branch prediction for indirect jump and call instructions](http://dl.acm.org/citation.cfm?id=2086703&preflayout=tabs), HiPeac 2012, Jason McCandless, David Gregg, Trinity College Dublin, Ireland

## Development ##
| For Users | For Developers |
|:----------|:---------------|
| [How to Build](BuildMao.md)                                | [Coding Conventions](CodingConventions.md)    |
| [How to Run](RunMao.md)                                    | [Code Reviews](CodeReviews.md)                |
| [Option Handling](OptionHandling.md)  | [Description of the MAO IR (Intermediate Representation)](IR.md) |
|                        | [How to Write a Pass/Plug-in](PassTutorial.md) |
| [Descriptions of Optimization Passes](PassDescription.md)   | [Description of Available Analysis Passes](AnalysisPasses.md) |
|                                                        | [External Reference Documentation](ReferenceDocumentation.md)|