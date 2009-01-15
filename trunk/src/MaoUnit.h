//
// Copyright 2008 Google Inc.
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef MAOUNIT_H_
#define MAOUNIT_H_

#include <list>
#include <map>
#include <vector>

#include "ir-gas.h"
#include "irlink.h"
#include "SymbolTable.h"

#define DEFAULT_SECTION_NAME "text"
#define DEFAULT_SECTION_CREATION_OP "\t.text"

typedef unsigned int entry_index_t;
typedef unsigned int subsection_index_t;

class BasicBlock;
class MaoUnitEntry;
class Section;
class SubSection;
class DirectiveEntry;
class SymbolTable;
class Symbol;
class InstructionEntry;
class MaoUnitEntryBase;

class MaoUnit {
 public:
  MaoUnit();
  ~MaoUnit();

  // Inserts an entry (Label, Instruction, Directive, ...) to the compilation
  // unit. create_default_section signals if a default section should be created
  // if there is no current subsection. Some directives in the beginning of the
  // file does not belong to any subsection.
  bool AddEntry(MaoUnitEntryBase *entry, bool create_default_section);
  // Add a common symbol to the symbol table
  bool AddCommSymbol(const char *name, unsigned int common_size,
                     unsigned int common_align);
  // Returns a handle to the symbol table
  SymbolTable *GetSymbolTable() {return &symbol_table_;}

  // Note that a given subsection identifier can occur in many places in the
  // code. (.text, .data, .text.)
  // This will be create several SubSection objects with .text in the
  // sub_sections_ member.
  void SetSubSection(const char *section_name, unsigned int subsection_number,
                     const char *creation_op);
  // Create the section section_name if it does not already exists. Returns a
  // pointer the section.
  Section *FindOrCreateAndFind(const char *section_name);

  // Insert a new basic block in the unit
  BasicBlock *AddAndGetBasicBlock();

  void PrintMaoUnit() const;
  void PrintMaoUnit(FILE *out) const;
  void PrintIR() const;
  void PrintIR(FILE *out) const;
 private:

  // Used by the STL-maps of sections and subsections.
  struct ltstr {
    bool operator()(const char* s1, const char* s2) const {
      return strcmp(s1, s2) < 0;
    }
  };

  // List of all the entries in the unit
  std::vector<MaoUnitEntryBase *> entries_;

  // A list of all subsections found in the unit.
  // Each subsection should have a pointer to the first and last
  // entry of the subsection in entries_
  std::vector<SubSection *> sub_sections_;

  // List of sections_ found in the program. Each subsection has a pointer to
  // its section.
  std::map<const char *, Section *, ltstr> sections_;

  // Pointer to current subsection. Used when parsing the assembly file.
  SubSection *current_subsection_;

  // Basic blocks
  std::vector<BasicBlock *> basicblocks_;
  BasicBlock *current_basicblock_; // Points to the current basic block, or NULL if none.


  // One symbol table holds all symbols found in the assembly file.
  SymbolTable symbol_table_;
};

// Base class for all types of entries in the MaoUnit. Example of entries
// are Labels, Directives, and Instructions
class MaoUnitEntryBase {
 public:
  MaoUnitEntryBase(unsigned int line_number, const char* line_verbatim);
  virtual ~MaoUnitEntryBase();

  virtual void PrintEntry(FILE *out) const = 0;
  virtual void PrintIR(FILE *out) const = 0;
  virtual char GetDescriptiveChar() const = 0;

  // Types of possible entries
  enum EntryType {
    UNDEFINED = 0,
    INSTRUCTION,
    LABEL,
    DIRECTIVE,
    DEBUG,
  };
  virtual EntryType entry_type() const = 0;

  virtual bool BelongsInBasicBlock() const = 0;
  virtual bool EndsBasicBlock() const = 0;

 protected:
  // Line number assembly was found in the original file
  unsigned int line_number_;
  // A verbatim copy of the assembly instruction this entry is
  // generated from. Might be NULL for some entries.
  const char *line_verbatim_;
  // Helper function to indent
  void Spaces(unsigned int n, FILE *outfile) const;
};


// A label
class Label {
 public:
  // allocates memory in the constructor
  explicit Label(const char *name);
  ~Label();
  const char *name() const;
 private:
  // name of the label.
  char *name_;
};

// An Entry of the type Label
class LabelEntry : public MaoUnitEntryBase {
 public:
  LabelEntry(const char *name, unsigned int line_number,
             const char* line_verbatim);
  ~LabelEntry();
  void PrintEntry(FILE *out) const;
  void PrintIR(FILE *out) const;
  MaoUnitEntryBase::EntryType  entry_type() const;
  const char *name();
  char GetDescriptiveChar() const {return 'L';}
  bool BelongsInBasicBlock() const {return false;}
  bool EndsBasicBlock() const {return true;}
 private:
  // The actual label
  Label *label_;
};

// A Directive
class Directive {
  // examples of directives
  //  .file "filename.c"
  //  .text
  //  .ident ""
  //  .section
 public:
  // Memory is allocated in the constructor and needs to be freed in the
  // destructor.
  Directive(const char *key, const char *value);
  ~Directive();
  const char *key();
  const char *value();
 private:
  // key_ holds the name of the directive
  char *key_;
  // value holds the arguments to the directive
  char *value_;
};

// An Entry of the type Directive
class DirectiveEntry : public MaoUnitEntryBase {
 public:
  DirectiveEntry(const char *key, const char* value, unsigned int line_number,
                 const char* line_verbatim);
  ~DirectiveEntry();
  void PrintEntry(FILE *out) const;
  void PrintIR(FILE *out) const;
  MaoUnitEntryBase::EntryType  entry_type() const;
  char GetDescriptiveChar() const {return 'D';}
  bool BelongsInBasicBlock() const {return false;}
  bool EndsBasicBlock() const {return false;}
 private:
  // The actual directive
  Directive *directive_;
};

// An Entry of the type Debug
// Used for debug directives.
class DebugEntry : public MaoUnitEntryBase {
 public:
  DebugEntry(const char *key, const char* value, unsigned int line_number,
             const char* line_verbatim);
  ~DebugEntry();
  void PrintEntry(FILE *out) const;
  void PrintIR(FILE *out) const;
  MaoUnitEntryBase::EntryType  entry_type() const;
  char GetDescriptiveChar() const {return 'g';}
  bool BelongsInBasicBlock() const {return false;}
  bool EndsBasicBlock() const {return false;}
 private:
  // Currently reuses the Directive class for debug entries
  Directive *directive_;
};

// Asm instruction is a wrapper around i386_insn structure used
// by the GNU Assembler 2.19.
class AsmInstruction {
 public:
  // Allocates memory in the constructor and frees it in the destructor
  explicit AsmInstruction(i386_insn *inst);
  ~AsmInstruction();
  void PrintInstruction(FILE *out) const;
  static const unsigned int kMaxRegisterNameLength = MAX_REGISTER_NAME_LENGTH;
  const char *get_op() const;
  bool EndsBasicBlock() const;
 private:
  i386_insn *instruction_;

  // Helper functions

  // Used to determine type of operand
  static bool IsMemOperand(const i386_insn *instruction,
                           const unsigned int op_index);
  static bool IsImmediateOperand(const i386_insn *instruction,
                                 const unsigned int op_index);
  static bool  IsRegisterOperand(const i386_insn *instruction,
                                 const unsigned int op_index);
  // Allocates memory for a new instruction and populates it.
  // The instruction passed from gas might not be allocated
  // until the end of the program.
  i386_insn *CreateInstructionCopy(i386_insn *in_inst);
  // Allocates memory for a register entry to be used
  // in the instruction
  reg_entry *CopyRegEntry(const reg_entry *in_reg);
  // Frees all allocated memory for this instruction
  void FreeInstruction();
};

// An Entry of the type Instruction
class InstructionEntry : public MaoUnitEntryBase {
 public:
  InstructionEntry(i386_insn* instruction, unsigned int line_number,
                   const char* line_verbatim);
  ~InstructionEntry();
  void PrintEntry(FILE *out) const;
  void PrintIR(FILE *out) const;
  MaoUnitEntryBase::EntryType  entry_type() const;
  char GetDescriptiveChar() const {return 'I';}
  bool BelongsInBasicBlock() const {return true;}
  bool EndsBasicBlock() const {return instruction_->EndsBasicBlock();}
 private:
  AsmInstruction *instruction_;
};



// A Subsection is part of a section. The subsection concept allows the assembly
// file to write the code more freely, but still keep the data organized in
// sections. Each subsection has a pointer to the section it belongs, as well
// as a number to allow the subsections to be ordered correctly.
class SubSection {
 public:
  // Constructor needs subsection number, a pointer to the actual section, and
  // the assembly code needed to create the subsection.
  explicit SubSection(unsigned int subsection_number, const char *name,
                      const char *creation_op);
  ~SubSection();
  unsigned int number() const;
  const char *name() const;
  const char *creation_op() const;

  entry_index_t first_entry_index() { return first_entry_index_;}
  entry_index_t last_entry_index() { return last_entry_index_;}
  void set_first_entry_index(entry_index_t index) { first_entry_index_ = index;}
  void set_last_entry_index(entry_index_t index) { last_entry_index_ = index;}
 private:
  // The subsection number
  const unsigned int number_;
  char *name_;

  // Points to the first and last entry for the subsection.
  // Value is stored as index into the entry vector.
  entry_index_t first_entry_index_;
  entry_index_t last_entry_index_;

  // The assembly code needed to create this subsection.
  char *creation_op_;
};


// A section
class Section {
 public:
  // Memory for the name is allocated in the constructor and freed
  // in the destructor.
  explicit Section(const char *name);
  ~Section();
  const char *name() const;
  void AddSubSectionIndex(subsection_index_t index);
  std::vector<subsection_index_t> *GetSubSectionIndexes() {return &sub_section_indexes_;}
 private:
  char *name_;  // .text -> "text", .data -> "data"
  std::vector<subsection_index_t> sub_section_indexes_;
};

class BasicBlock {
 public:
  BasicBlock();
  ~BasicBlock();

  entry_index_t first_entry_index() { return first_entry_index_;}
  entry_index_t last_entry_index() { return last_entry_index_;}
  void set_first_entry_index(entry_index_t index) { first_entry_index_ = index;}
  void set_last_entry_index(entry_index_t index) { last_entry_index_ = index;}

 private:
  // Points to the first and last entry for the subsection.
  // Value is stored as index into the entry vector
  entry_index_t first_entry_index_;
  entry_index_t last_entry_index_;

};


#endif  // MAOUNIT_H_
