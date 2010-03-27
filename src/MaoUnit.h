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
  //                          of a new section, if there is no "current" section
  bool AddEntry(MaoEntry *entry, bool create_default_section);

  // Add a common symbol to the symbol table
  bool AddCommSymbol(const char *name, unsigned int common_size,
                     unsigned int common_align);
  // Returns a handle to the symbol table
  SymbolTable *GetSymbolTable() {return &symbol_table_;}

  SubSection *GetSubSection(unsigned int subsection_number) {
    return sub_sections_[subsection_number];
  }

  // Gets the entry corresponding label_name. Returns NULL
  // if no such label is found.
  LabelEntry *GetLabelEntry(const char *label_name) const;

  // Instruction Creators
  InstructionEntry *CreateInstruction(const char *opcode,
                                      unsigned int base_opcode,
                                      Function *function);
  InstructionEntry *CreateInstruction(i386_insn *instruction,
                                      Function *function);
  InstructionEntry *CreateNop(Function *function);
  InstructionEntry *Create2ByteNop(Function *function);
  InstructionEntry *CreateLock(Function *function);
  InstructionEntry *CreateUncondJump(LabelEntry *l, Function *function);

  // prefetch type:
  //    0:  nta
  //    1:  t0
  //    2:  t1
  //    3:  t2
  InstructionEntry *CreatePrefetch(Function *function,
                                   int prefetch_type,
                                   InstructionEntry *insn,
                                   int op_index,
                                   int offset = 0);
  LabelEntry *CreateLabel(const char *labelname,
                          Function* function,
                          SubSection* ss);
  DirectiveEntry *CreateDirective(DirectiveEntry::Opcode op,
                                  const DirectiveEntry::OperandVector &operands,
                                  Function *function,
                                  SubSection *subsection);

  // Dumpers
  void PrintMaoUnit() const;
  void PrintMaoUnit(FILE *out) const;
  void PrintIR(bool print_entries = true,
               bool print_sections = true,
               bool print_subsections = true,
               bool print_functions = true) const;
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

  MaoOptions *mao_options() { return mao_options_; }

  // Iterate over the sections
  SectionIterator SectionBegin();
  SectionIterator SectionEnd();
  ConstSectionIterator ConstSectionBegin() const;
  ConstSectionIterator ConstSectionEnd() const;

  FunctionIterator FunctionBegin();
  FunctionIterator FunctionEnd();
  ConstFunctionIterator ConstFunctionBegin() const;
  ConstFunctionIterator ConstFunctionEnd() const;

  // Find all Functions in the MaoUnit and populate functions_
  void FindFunctions(bool create_anonymous);

  Function *GetFunction(MaoEntry *entry);
  bool InFunction(MaoEntry *entry) const;
  SubSection *GetSubSection(MaoEntry *entry);
  bool InSubSection(MaoEntry *entry) const;

  // Symbol handling
  Symbol *AddSymbol(const char *name);
  Symbol *FindOrCreateAndFindSymbol(const char *name);

  // Delete the entry from the IR.
  void DeleteEntry(MaoEntry *entry);

  Stats *GetStats() {return &stats_;}


  bool Is64BitMode() const { return arch_ == X86_64; }
  bool Is32BitMode() const { return arch_ == I386;   }

  void PushSubSection();
  void PopSubSection(int line_number);
  void SetPreviousSubSection(int line_number);

  // Set the default achiecture (32-bit vs 64-bit). Should only be called
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
  // entry of the subsection in entries_
  std::vector<SubSection *> sub_sections_;

  // List of sections_ found in the program. Each subsection has a pointer to
  // its section.
  std::map<const char *, Section *, ltstr> sections_;

  // Holds the function identified in the MaoUnit
  FunctionVector  functions_;

  // Pointer to current subsection. Used when parsing the assembly file.
  SubSection *current_subsection_;
  // Previous subsection is needed for the .previous directive
  SubSection *prev_subsection_;
  // A stack of subsection is used to support push/popsection directives.
  // each pair is the subsection, and the prev_subsection
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
