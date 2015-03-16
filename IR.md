This page discusses MAO's internal representation(IR) and the important APIs to access and manipulate the IR. This document does not serve as a complete documentation of all the classes that implement MAO's core IR. Instead, it gives an overview of the classes and describes some of the important methods that will be useful to a developer.



MAO stores a program as a list of _entries_ which roughly correspond to lines in the assembly file. While this makes the representation simple, it does not provide a very rich interface to access and manipulate the IR. So MAO provides a hierarchy of views that are useful to the developer. Every type  that implements this hierarchy (except `MaoUnit` which is at the top-most level) implements a `id()` method that  uniquely identifies all entities of that type.

## Unit ##
The topmost level in this hierarchy is the `MaoUnit` class. This class corresponds to an entire assembly program and contains a vector of pointers to `MaoEntry` objects. This entry vector is not directly accessed from `MaoUnit`. Instead, it provides iterators to access _sections_ and _functions_ that are represented by the `Section` and `Function` classes respectively. The following methods in `MaoUnit` enable iterating over the sections and the functions.

```
  SectionIterator SectionBegin();
  SectionIterator SectionEnd();
  ConstSectionIterator ConstSectionBegin();
  ConstSectionIterator ConstSectionEnd();
  FunctionIterator FunctionBegin();
  FunctionIterator FunctionEnd();
  ConstFunctionIterator ConstFunctionBegin();
  ConstFunctionIterator ConstFunctionEnd();
```

All iterators iterate over pointers to the corresponding classes and overload the `++` and the `*` operators. Thus a common code idiom to iterate looks like this:
```
  for (FunctionIterator iter = unit->FunctionEnd(); iter != unit->FunctionEnd(); ++iter) {
    Function *func = *iter;
    //
  }
```
The entries in `MaoUnit` are not replicated in other hierarchical containers under `MaoUnit`. Instead, they typically keep a pointer to the first and last entries in the hierarchy.

In addition to the above methods, `MaoUnit` provides other useful methods that are found in `MaoUnit.h` and `MaoUnit.cc`.

## Sections and Subsections ##
A section in an assembly file corresponds to `.section` directives in the assembly file with the same name and is represented by the `Section` class. For instance, every entry between `.section text` and the next `.section` directive is represented by one `Section` object.  The entries in a Section can be iterated over using the following methods:
```
  SectionEntryIterator EntryBegin();
  SectionEntryIterator EntryEnd();
```
The `Section` class also contains a vector of `SubSection` pointers. The GNU assembler allows subsections that are not contiguous in the assembly file, but considered as belonging to the same section. Thus a section is nothing but a list of subsections that belong to it. A subsection is represented by the `SubSection` class. The method `std::vector<SubSectionID> GetSubsectionIDs()` returns a cetor of subsection IDs in a given section. These ids can be passed to the `SubSection *GetSubSection(unsigned int subsection_number)` method of `MaoUnit` to get the `SubSection` pointer. The `SubSection` class stores pointers to the first and the last entry within the subsection. `EntryBegin()` and `EntryEnd()` can also be used to iterate over the entries in an subsection. In fact, the entry iterator of a section simply iterates over the entries in all its subsections in the list order.


## Function ##
Many optimization passes operate at a function level and hence it is important to group a sequence of entries into functions. The `Function` class represents a function. It contains pointers to the first and last entries, the enclosing subsection and the enclosing section.  The following methods enable iterating over the entries of a function:

```
  SectionEntryIterator EntryBegin();
  SectionEntryIterator EntryEnd();
```

Internally, the `Function` class also stores pointers to the control flow graph (CFG) and the loop structure graph (LSG). These data structures are not directly accessed from a `Function` object. Instead, the CFG and LSG classes provide methods to obtain the CFG or LSG for a given `Function`.

## Entry ##
An entry corresponds to a non-comment line in the assembly file and is represented by the `MaoEntry` class. `MaoEntry` is a pure abstract class. It is derived by `InstructionEntry`, `LabelEntry` and `DirectiveEntry` classes. The base class `MaoEntry` implements the following methods common to all the derived classes that return the previous and next entries in the `MaoUnit`:
```
  MaoEntry *next()
  MaoEntry *prev()
```

The following methods in `MaoEntry` help to determine the specific type of an entry:
```
  bool IsInstruction()
  bool IsLabel() 
  bool IsDirective()
```

and the following methods help to cast a `MaoEntry` pointer to a specific type:
```
  InstructionEntry *AsInstruction();
  LabelEntry *AsLabel();
  DirectiveEntry *AsDirective();
```

`MaoEntry` also has the methods to insert to or remove from new entries. The relevant methods for this purpose are:
```
  void Unlink();
  void Unlink(MaoEntry *last_in_chain);
  void LinkBefore(MaoEntry *entry);
  void LinkAfter(MaoEntry *entry);
```

## Directive ##
The `DirectiveEntry` class represents assembler directives such as `.byte`, `.word`, `.p2align`, etc. A directive consists of an opcode and a vector of operands. The operands are represented by an inner `Operand` class. The opcode of the directive is returned by the `op()` method and the `const Operand *GetOperand(int num)` returns a particular operand of the directive.

## Label ##
The `LabelEntry` class represents the labels in the assembly program. The `const char *name()` method returns the label name.

## Instruction ##
The `InstructionEntry` class stores the assembly instructions. This class is essentially a wrapper around the `struct i386_insn` structure implemented in the binutils package. The `i386_insn` structure contains the details of the instructions in a decoded form as well as the details of its operands. `InstructionEntry` abstracts out the details of this structure by providing useful methods that categorize the instruction type (`bool IsCall()`, `bool IsAdd()`, `bool IsControlTransfer()`), query about the operands (`bool IsMemOperand(const unsigned int op_index) const`, `bool IsImmediateOperand(const unsigned int op_index) const`, `bool IsRegisterOperand(const unsigned int op_index) const`), and so on. If these APIs do not provide the needed functionality, developers should add relevant methods to `InstructionEntry` instead of accessing the fields directly in the passes.

Many optimization passes would need to create new instructions. `InstructionEntry` does  not provide a rich API to modify the fields of `i386_insn` and hence creating a new instruction requires directly filling in the fields of the `i386_insn` structure. To simplify the process, we have written a pass named INSBUILDPLUG that prints a C++ file that fills in the fields of the `i386_insn` structure corresponding to an instruction. The steps to create an instruction are:

  1. Create a file(input.s, for example) with the instruction  that needs to be created by the pass
  1. Run `${MAO_HOME}/bin/mao-x86_64-linux --mao=--plugin=${MAO_HOME}/bin/MaoInstructionBuilder-x86_64-linux.so --mao=INSBUILDPLUG input.s > createins.cc`
  1. The pass produces a function named `void FillInstructionDetails(i386_insn *i)` that fills in the fields of the `i386_insn` structure for the sole instruction in input.s. Depending on how this new instruction is used in the optimization pass, certain fields of the structure (those that contains the details of the operands) may need to be changed.
  1. After calling this function, the `InstructionEntry *CreateInstruction(i386_insn *,Function *)` method in `MaoUnit` needs to be invoked passing the `i386_insn` pointer and the function  in which the instruction needs to be generated. This returns a `InstructionEntry *` that can be appropriately inserted into the code using `LinkBefore` and `LinkAfter` methods of `MaoEntry`.