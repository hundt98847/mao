//
// Copyright 2010 Google Inc.
//

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA.

// Mao Unit - represents the entire program being compiled by MAO
// Classes:
//   MaoUnit - represents the entire assembly program.
//
#ifndef MAOUNIT_H_
#define MAOUNIT_H_

#include <list>
#include <map>
#include <stack>
#include <string>
#include <utility>
#include <vector>

extern "C" {
  #include "as.h"
  #include "tc-i386.h"
}

#include "gen-opcodes.h"

#include "MaoDebug.h"
#include "MaoDefs.h"
#include "MaoEntry.h"
#include "MaoFunction.h"
#include "MaoOptions.h"
#include "MaoSection.h"
#include "MaoStats.h"

#include "SymbolTable.h"

#define MAO_MAJOR_VERSION 0
#define MAO_MINOR_VERSION 1

#define DEFAULT_SECTION_NAME ".text"

class Function;
class MaoUnit;
class Symbol;
class SymbolTable;


// Represents an input assembly file and provides methods to access sections,
// functions and entries in the program. Also provides methods to create new
// instructions, directives and labels.
class MaoUnit {
 public:

  typedef std::vector<MaoEntry *>      EntryVector;
  typedef EntryVector::iterator        VectorEntryIterator;
  typedef EntryVector::const_iterator  ConstVectorEntryIterator;


  typedef std::vector<Function *>         FunctionVector;
  typedef FunctionVector::iterator        FunctionIterator;
  typedef FunctionVector::const_iterator  ConstFunctionIterator;


  explicit MaoUnit(MaoOptions *mao_options);
  ~MaoUnit();

  // Inserts an entry (Label, Instruction, Directive, ...) to the compilation
  // unit. create_default_section signals if a default section should be created
  // if there is no current subsection. Some directives in the beginning of the
  // file does not belong to any subsection.
  // The entry is added at the end of the list, unless an iterator to the list
  // is given.
  //  build_section - signals if the section building should be triggered.
  //                  this is true only when building the initial structures,
  //                  typically not when adding extra entries later on.
  // create_default_section - signals if an entry can trigger automatic creation
  //                         of a new section, if there is no "current" section.
  bool AddEntry(MaoEntry *entry, bool create_default_section);

  // Adds a common symbol to the symbol table with a desired size and alignment.
  // If the symbol does not exist in the symbol table, it is added. If it exists
  // and its size or alignment is less than the specified size and alignment,
  // they are set to the specified size and/or alignment.
  bool AddCommSymbol(const char *name, unsigned int common_size,
                     unsigned int common_align);
  // Returns a handle to the symbol table.
  SymbolTable *GetSymbolTable() {return &symbol_table_;}

  // Returns the subsection specified by the given subsection number.
  SubSection *GetSubSection(unsigned int subsection_number) {
    return sub_sections_[subsection_number];
  }

  // Gets the entry corresponding label_name. Returns NULL
  // if no such label is found.
  LabelEntry *GetLabelEntry(const char *label_name) const;

  // Instruction Creators.
  //
  // Creates an instruction with the specified opcode in a given function.
  InstructionEntry *CreateInstruction(const char *opcode,
                                      unsigned int base_opcode,
                                      Function *function);
  // Creates an InstructionEntry wrapping around the specified i386_insn* and
  // associate it with the given function.
  InstructionEntry *CreateInstruction(i386_insn *instruction,
                                      Function *function);
  // Creates a nop and associate it with the given function.
  InstructionEntry *CreateNop(Function *function);
  // Creates a 2-byte nop and associate it with the given function.
  InstructionEntry *Create2ByteNop(Function *function);
  // Creates a lock instruction and associate it with the given function.
  InstructionEntry *CreateLock(Function *function);
  // Creates an unconditional jump to a specified label and associate it with
  // the given function.
  InstructionEntry *CreateUncondJump(LabelEntry *l, Function *function);

  // prefetch type:
  //    0:  nta
  //    1:  t0
  //    2:  t1
  //    3:  t2

  // Creates a prefetch instruction of the given prefetch type. The prefetch
  // address is obtained by adding offset to the 'op_index'th operand of 'insn'.
  InstructionEntry *CreatePrefetch(Function *function,
                                   int prefetch_type,
                                   InstructionEntry *insn,
                                   int op_index,
                                   int offset = 0);
  // Creates a label specified by label_name and associate it with the specified
  // function and subsection if they are not NULL.
  LabelEntry *CreateLabel(const char *labelname,
                          Function* function,
                          SubSection* ss);
  // Creates a directive with opcode 'op' and operands specified by the given
  // vector. Associate it with the specified function and subsection if they
  // are not NULL.
  DirectiveEntry *CreateDirective(DirectiveEntry::Opcode op,
                                  const DirectiveEntry::OperandVector &operands,
                                  Function *function,
                                  SubSection *subsection);

  // Dumpers.
  //
  // Prints this MAO unit.
  void PrintMaoUnit() const;
  // Prints this MAO unit to FILE *out.
  void PrintMaoUnit(FILE *out) const;
  // Prints the IR of this unit. The flags controls what gets printed.
  void PrintIR(bool print_entries = true,
               bool print_sections = true,
               bool print_subsections = true,
               bool print_functions = true) const;
  // Prints the IR of this unit to FILE *out. The flags controls what gets
  // printed.
  void PrintIR(FILE *out,
               bool print_entries = true,
               bool print_sections = true,
               bool print_subsections = true,
               bool print_functions = true) const;


  // Returns the section matching the given name. Returns
  // NULL if no match is found.
  Section * GetSection(const std::string &section_name) const;

  // Simple class for generating unique names for mao-created labels.
  class BBNameGen {
   public:
    static const char *GetUniqueName();
   private:
    static long i;
  };

  // Returns the pointer to the MaoOptions object that contains the options
  // passed during the invocation of MAO.
  MaoOptions *mao_options() { return mao_options_; }

  // Returns an iterator that points to the first section in this unit.
  SectionIterator SectionBegin();
  // Returns an iterator that points after the last section in this unit.
  SectionIterator SectionEnd();
  // Returns a constant iterator that points to the first section in this unit.
  ConstSectionIterator ConstSectionBegin() const;
  // Returns a constant iterator that points after the last section in this
  // unit.
  ConstSectionIterator ConstSectionEnd() const;

  // Returns an iterator that points to the first function in this unit.
  FunctionIterator FunctionBegin();
  // Returns an iterator that points after the last function in this unit.
  FunctionIterator FunctionEnd();
  // Returns a constant iterator that points to the first function in this unit.
  ConstFunctionIterator ConstFunctionBegin() const;
  // Returns a constant iterator that points after the last function in this
  // unit.
  ConstFunctionIterator ConstFunctionEnd() const;

  // Finds all Functions in the MaoUnit and populate functions_.
  // create_anonymous tells whether MAO has to create anonymous functions for
  // instructions not in any function.
  void FindFunctions(bool create_anonymous);

  // Returns the function to which the given entry belongs.
  Function *GetFunction(MaoEntry *entry);
  // Returns true if the given entry is in a function.
  bool InFunction(MaoEntry *entry) const;

  // Returns the subsection to which the given entry belongs.
  SubSection *GetSubSection(MaoEntry *entry);
  // Returns true if the given entry is in a subsection.
  bool InSubSection(MaoEntry *entry) const;

  // Symbol handling.
  //
  // Adds a symbol to the symbol table.
  Symbol *AddSymbol(const char *name);
  // Checks if a symbol with the given name is in the symbol table and returns a
  // Symbol *. If the symbol does not exist in the symbol table, creates a new
  // symbol with the given name and adds it to the symbol table.
  Symbol *FindOrCreateAndFindSymbol(const char *name);

  // Deletes the entry from the IR.
  void DeleteEntry(MaoEntry *entry);

  // Returns statistics about the unit.
  Stats *GetStats() {return &stats_;}


  // Returns true if the code is in 64 bit mode.
  bool Is64BitMode() const { return arch_ == X86_64; }
  // Returns true if the code is in 32 bit mode.
  bool Is32BitMode() const { return arch_ == I386;   }

  // Implements the .pushsection directive that pushes the current subsection
  // into a subsection stack. In MAO, this directive gets translated into a
  // .section directive.
  void PushSubSection();
  // Implements the .popsection directive that pops the top of the subsection
  // stack and uses it as the current subsection. In MAO, this directive gets
  // translated into a .section and .subsection directives. The line_number
  // specified is used as the line number of the .section and .subsection
  // directive entries.
  void PopSubSection(int line_number);
  // Sets the previous subsection as the new current subsection. line_number is
  // used as the line number of the new .section directive.
  void SetPreviousSubSection(int line_number);

  // Sets the default achiecture (32-bit vs 64-bit). Should only be called
  // after GAS have parsed it arguments, since the flags --32 and --64
  // change the architecture.
  void SetDefaultArch();
 private:

  enum Arch {
    UNKNOWN,
    I386,
    X86_64
  };
  enum Arch arch_;

  // Create the section section_name if it does not already exists. Returns a
  // pair with a boolean saying if a new section was craeted, and a pointer the
  // section.
  std::pair<bool, Section *> FindOrCreateAndFind(const char *section_name);

  // Vector of the entries in the unit. The id of the entry
  // is also the index into this vector.
  EntryVector entry_vector_;

  // A list of all subsections found in the unit.
  // Each subsection should have a pointer to the first and last
  // entry of the subsection in entries_.
  std::vector<SubSection *> sub_sections_;

  // List of sections_ found in the program. Each subsection has a pointer to
  // its section.
  std::map<const char *, Section *, ltstr> sections_;

  // Holds the function identified in the MaoUnit.
  FunctionVector  functions_;

  // Pointer to current subsection. Used when parsing the assembly file.
  SubSection *current_subsection_;
  // Previous subsection is needed for the .previous directive.
  SubSection *prev_subsection_;
  // A stack of subsection is used to support push/popsection directives.
  // each pair is the subsection, and the prev_subsection.
  std::stack<std::pair<SubSection *, SubSection *> > subsection_stack_;

  // One symbol table holds all symbols found in the assembly file.
  SymbolTable symbol_table_;

  // Maps label-names to the corresponding label entry.
  std::map<const char *, LabelEntry *, ltstr> labels_;

  // Maps an entry to the corresponding Function and SubSection.
  std::map<MaoEntry *, Function *>   entry_to_function_;
  std::map<MaoEntry *, SubSection *> entry_to_subsection_;

  // Given an entry, return the name of the function it belongs to,
  // or "" if it is not in any function.
  const char *FunctionName(MaoEntry *entry) const;
  const char *SectionName(MaoEntry *entry) const;

  MaoOptions *mao_options_;

  // Called when found a new subsection reference in the assembly.
  // The following is done:
  //   - If section_name does not exists, create it.
  //   - Create a new subsection, and link it to the
  //     other subsections in the section.
  // Returns a pointer to the new subsection.
  SubSection *AddNewSubSection(const char *section_name,
                               unsigned int subsection_number, MaoEntry *entry);

  Stats stats_;
};  // MaoUnit


#endif  // MAOUNIT_H_
