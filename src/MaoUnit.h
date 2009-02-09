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
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA.

#ifndef MAOUNIT_H_
#define MAOUNIT_H_

#include <list>
#include <map>
#include <vector>
#include <utility>
#include <string>

#include "ir-gas.h"
#include "gen-opcodes.h"
#include "irlink.h"
#include "SymbolTable.h"
#include "MaoUtil.h"

#define DEFAULT_SECTION_NAME ".text"


// Maps the label to a block, and the offset into this block
struct ltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};

typedef unsigned int entry_index_t;
typedef unsigned int subsection_index_t;
typedef unsigned int basicblock_index_t;
typedef std::map<const char *,
                 std::pair<basicblock_index_t, unsigned int>,
                 ltstr> labels2basicblock_map_t;

class Section;
class SubSection;
class DirectiveEntry;
class SymbolTable;
class Symbol;
class InstructionEntry;
class MaoUnitEntryBase;
class LabelEntry;
class MaoOptions;

class MaoUnit {
 public:
  typedef std::vector<MaoUnitEntryBase *> EntryVector;
  typedef EntryVector::iterator EntryIterator;
  typedef EntryVector::const_iterator ConstEntryIterator;

  MaoUnit(MaoOptions *mao_options);
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
  // TODO(martint): Check if we should build the sections in a new pass instead?
  //                That would simplify the code.
  //
  bool AddEntry(MaoUnitEntryBase *entry, bool build_sections,
                bool create_default_section);
  bool AddEntry(MaoUnitEntryBase *entry, bool build_sections,
                bool create_default_section,
                std::list<MaoUnitEntryBase *>::iterator list_iter);
  // Add a common symbol to the symbol table
  bool AddCommSymbol(const char *name, unsigned int common_size,
                     unsigned int common_align);
  // Returns a handle to the symbol table
  SymbolTable *GetSymbolTable() {return &symbol_table_;}

  // Note that a given subsection identifier can occur in many places in the
  // code. (.text, .data, .text.)
  // This will be create several SubSection objects with .text in the
  // sub_sections_ member.
  bool SetSubSection(const char *section_name, unsigned int subsection_number);

  SubSection *GetSubSection(unsigned int subsection_number) {
    return sub_sections_[subsection_number];
  }

  // Create the section section_name if it does not already exists. Returns a
  // pointer the section.
  std::pair<bool, Section *> FindOrCreateAndFind(const char *section_name);

  LabelEntry *GetLabelEntry(const char *label_name) const;

  void PrintMaoUnit() const;
  void PrintMaoUnit(FILE *out) const;
  void PrintIR() const;
  void PrintIR(FILE *out) const;

  std::list<MaoUnitEntryBase *>::iterator EntryBegin() {
    return entry_list_.begin();
  }

  std::list<MaoUnitEntryBase *>::iterator EntryEnd() {
    return entry_list_.begin();
  }

  // Simple class for generating unique names for mao-created labels.
  class BBNameGen {
   public:
    static const char *GetUniqueName();
   private:
    static int i;
  };

  MaoOptions *mao_options() { return mao_options_; }

 private:
  // Used by the STL-maps of sections and subsections.

  // Vector of the entries in the unit. The id of the entry
  // is also the index into this vector.
  EntryVector entry_vector_;

  // List of all the entries in the unit.
  std::list<MaoUnitEntryBase *> entry_list_;

  // A list of all subsections found in the unit.
  // Each subsection should have a pointer to the first and last
  // entry of the subsection in entries_
  std::vector<SubSection *> sub_sections_;

  // List of sections_ found in the program. Each subsection has a pointer to
  // its section.
  std::map<const char *, Section *, ltstr> sections_;

  // Pointer to current subsection. Used when parsing the assembly file.
  SubSection *current_subsection_;

  // One symbol table holds all symbols found in the assembly file.
  SymbolTable symbol_table_;

  std::map<const char *, LabelEntry *> labels_;

  MaoUnitEntryBase *GetEntryByID(basicblock_index_t id) const;

  MaoOptions *mao_options_;
};


// Base class for all types of entries in the MaoUnit. Example of entries
// are Labels, Directives, and Instructions
class MaoUnitEntryBase {
 public:
  // Types of possible entries
  enum EntryType {
    UNDEFINED = 0,
    INSTRUCTION,
    LABEL,
    DIRECTIVE,
    DEBUG,
  };

  MaoUnitEntryBase(unsigned int line_number, const char *const line_verbatim);
  virtual ~MaoUnitEntryBase();

  virtual void PrintEntry(FILE *out) const = 0;
  virtual void PrintIR(FILE *out) const = 0;
  virtual char GetDescriptiveChar() const = 0;
  virtual EntryType Type() const = 0;

  entry_index_t index() const { return index_; }
  void set_index(entry_index_t index) {index_ = index;}

  // Property methods
  virtual bool HasFallThrough() const = 0;
  virtual bool IsControlTransfer() const = 0;
  virtual bool IsCall() const = 0;
  virtual bool IsReturn() const = 0;

 protected:
  // Helper function to indent
  void Spaces(unsigned int n, FILE *outfile) const;
  unsigned int line_number() const { return line_number_; }
  const char *const line_verbatim() const { return line_verbatim_; }

 private:
  // Line number assembly was found in the original file
  const unsigned int line_number_;

  // A verbatim copy of the assembly instruction this entry is
  // generated from. Might be NULL for some entries.
  const char *line_verbatim_;

  entry_index_t index_;
};


// An Entry of the type Label
class LabelEntry : public MaoUnitEntryBase {
 public:
  LabelEntry(const char *const name,
             unsigned int line_number,
             const char *const line_verbatim)
      : MaoUnitEntryBase(line_number, line_verbatim),
        name_(strdup(name)) { }
  ~LabelEntry() { delete name_; }

  virtual void PrintEntry(FILE *out) const;
  virtual void PrintIR(FILE *out) const;
  virtual EntryType Type() const { return LABEL; }
  const char *name() { return name_; }
  virtual char GetDescriptiveChar() const { return 'L'; }

  // Property methods
  virtual bool HasFallThrough() const { return true; }
  virtual bool IsControlTransfer() const { return false; }
  virtual bool IsCall() const { return false; }
  virtual bool IsReturn() const { return false; }

 private:
  const char *const name_;
};

// An Entry of the type Directive
class DirectiveEntry : public MaoUnitEntryBase {
  // examples of directives
  //  .file "filename.c"
  //  .text
  //  .ident ""
  //  .section
 public:
  enum Opcode {
    FILE = 0,
    SECTION,
    GLOBAL,
    LOCAL,
    WEAK,
    TYPE,
    SIZE,
    BYTE,
    WORD,
    LONG,
    QUAD,
    RVA,
    ASCII,
    STRING8,
    STRING16,
    STRING32,
    STRING64,
    SLEB128,
    ULEB128,
    P2ALIGN,
    P2ALIGNW,
    P2ALIGNL,
    SPACE,
    DS_B,
    DS_W,
    DS_L,
    DS_D,
    DS_X,
    COMM,
    IDENT,
    SET,        // Same as .equ
    EQUIV,
    NUM_OPCODES // Used to get the size of the array
  };

  static const char *const kOpcodeNames[NUM_OPCODES];

  enum OperandType {
    NO_OPERAND = 0,
    STRING,
    INT,
    SYMBOL,
    EXPRESSION,
    EMPTY_OPERAND,
  };

  class Operand {
   public:
    explicit Operand() : type(EMPTY_OPERAND) { }
    explicit Operand(const char *str) : type(STRING) {
      data.str = new std::string(str);
    }
    explicit Operand(std::string str) : type(STRING) {
      data.str = new std::string();
      data.str->swap(str);
    }
    explicit Operand(const MaoStringPiece &str) : type(STRING) {
      data.str = new std::string(str.data, str.length);
    }
    explicit Operand(symbolS *sym) : type(SYMBOL) { data.sym = sym; }
    explicit Operand(expressionS *expr) : type(EXPRESSION) {
      data.expr = (expressionS *)malloc(sizeof(expressionS));
      *data.expr = *expr;
    }
    explicit Operand(int value) : type(INT) { data.i = value; }
    ~Operand() {
      if (type == STRING)
        delete data.str;
      if (type == EXPRESSION)
        free(data.expr);
    }

    OperandType type;
    union  {
      std::string *str;
      int i;
      symbolS *sym;
      expressionS *expr;
    } data;
  };

  typedef std::vector<Operand *> OperandVector;

  DirectiveEntry(Opcode op, const OperandVector &operands,
                 unsigned int line_number, const char* line_verbatim)
      : MaoUnitEntryBase(line_number, line_verbatim),
        op_(op), operands_(operands) { }

  virtual ~DirectiveEntry() {
    for (OperandVector::iterator iter = operands_.begin();
         iter != operands_.end(); ++iter) {
      delete *iter;
    }
  }

  Opcode op() { return op_; }
  const char *GetOpcodeName() const {
    return kOpcodeNames[op_];
  }

  int NumOperands() { return operands_.size(); }
  const Operand *GetOperand(int num) { return operands_[num]; }

  virtual void PrintEntry(::FILE *out) const;
  virtual void PrintIR(::FILE *out) const;
  virtual MaoUnitEntryBase::EntryType  Type() const;
  virtual char GetDescriptiveChar() const {return 'D';}

  // Property methods
  virtual bool HasFallThrough() const { return false; }
  virtual bool IsControlTransfer() const { return false; }
  virtual bool IsCall() const { return false; }
  virtual bool IsReturn() const { return false; }

 private:
  const std::string &OperandsToString(std::string *out) const;
  const std::string &OperandToString(const Operand &operand,
                                     std::string *out) const;

  const std::string &OperandExpressionToString(const expressionS *expr,
                                               std::string *out) const;

  const char *DirectiveEntry::GetDotOrSymbol(symbolS *symbol) const;


  // op_ holds the type of directive
  const Opcode op_;

  // operands_ hold the operands of the directive
  OperandVector operands_;
};

// An Entry of the type Debug
// Used for debug directives.
class DebugEntry : public MaoUnitEntryBase {
 public:
  DebugEntry(const char *key, const char* value, unsigned int line_number,
             const char* line_verbatim)
      : MaoUnitEntryBase(line_number, line_verbatim),
        key_(key), value_(value) { }
  virtual void PrintEntry(FILE *out) const;
  virtual void PrintIR(FILE *out) const;
  virtual MaoUnitEntryBase::EntryType  Type() const;
  virtual char GetDescriptiveChar() const {return 'g';}

  // Property methods
  virtual bool HasFallThrough() const { return false; }
  virtual bool IsControlTransfer() const { return false; }
  virtual bool IsCall() const { return false; }
  virtual bool IsReturn() const { return false; }

 private:
  // key_ holds the name of the directive
  const std::string key_;
  // value holds the arguments to the directive
  const std::string value_;
};

// Asm instruction is a wrapper around i386_insn structure used
// by the GNU Assembler 2.19.
class AsmInstruction {
 public:
  // Allocates memory in the constructor and frees it in the destructor
  explicit AsmInstruction(i386_insn *inst, MaoOpcode opcode);
  ~AsmInstruction();
  void PrintInstruction(FILE *out) const;
  static const unsigned int kMaxRegisterNameLength = MAX_REGISTER_NAME_LENGTH;
  const char *GetOp() const;
  MaoOpcode   op() const { return op_; }
  bool HasFallThrough() const;
  bool HasTarget() const;
  bool IsCall() const;
  bool IsReturn() const;

  const char *GetTarget() const;

  i386_insn *instruction() { return instruction_; }

 private:
  i386_insn *instruction_;
  MaoOpcode  op_;

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
  bool IsInList(const MaoOpcode opcode, const MaoOpcode list[],
                const unsigned int number_of_elements) const;
  void PrintImmediateOperand(FILE *out, const enum bfd_reloc_code_real reloc,
                             const expressionS *expr) const;
  void PrintMemoryOperand(FILE *out, const i386_operand_type &operand_type,
                          const enum bfd_reloc_code_real reloc,
                          const expressionS *expr,
                          const char *segment_overried,
                          const bool jumpabsolute) const;
  const char *GetRelocString(const enum bfd_reloc_code_real reloc) const;

  // Determines if the suffix needs to be printed
  bool PrintSuffix() const;
};

// An Entry of the type Instruction
class InstructionEntry : public MaoUnitEntryBase {
 public:
  InstructionEntry(i386_insn* instruction, unsigned int line_number,
                   const char* line_verbatim);
  ~InstructionEntry();
  virtual void PrintEntry(FILE *out) const;
  virtual void PrintIR(FILE *out) const;
  virtual MaoUnitEntryBase::EntryType  Type() const;
  const char *GetOp() const  {return instruction_->GetOp();}
  const char *GetTarget() const  {return instruction_->GetTarget();}
  virtual char GetDescriptiveChar() const {return 'I';}
  bool HasTarget() const {return instruction_->HasTarget();}

  // Property methods
  virtual bool HasFallThrough() const { return instruction_->HasFallThrough(); }
  virtual bool IsControlTransfer() const {
    return instruction_->HasTarget() || IsCall() || IsReturn();
  }
  virtual bool IsCall() const { return instruction_->IsCall(); }
  virtual bool IsReturn() const { return instruction_->IsReturn(); }

  AsmInstruction *instruction() { return instruction_; }

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
  explicit SubSection(unsigned int subsection_number, const char *name)
      : number_(subsection_number),
        name_(name),
        first_entry_index_(0),
        last_entry_index_(0) { }

  unsigned int number() const { return number_; }
  const std::string &name() const { return name_; }

  entry_index_t first_entry_index() const { return first_entry_index_;}
  entry_index_t last_entry_index() const { return last_entry_index_;}
  void set_first_entry_index(entry_index_t index) { first_entry_index_ = index;}
  void set_last_entry_index(entry_index_t index) { last_entry_index_ = index;}
 private:

  // The subsection number
  const unsigned int number_;
  std::string name_;

  // Points to the first and last entry for the subsection.
  // Value is stored as index into the entry vector.
  entry_index_t first_entry_index_;
  entry_index_t last_entry_index_;
};


class SectionEntryIterator {
 public:
  typedef MaoUnitEntryBase *Entry;

  SectionEntryIterator(
      MaoUnit *mao,
      std::vector<subsection_index_t>::iterator sub_section_iter,
      std::vector<subsection_index_t>::iterator sub_section_iter_end,
      std::list<MaoUnitEntryBase *>::iterator entry_iter)
      : mao_(mao),
        sub_section_iter_(sub_section_iter),
        sub_section_iter_end_(sub_section_iter_end),
        entry_iter_(entry_iter) { }

  Entry &operator *() const { return *entry_iter_; }

  SectionEntryIterator &operator ++() {
    SubSection *sub_section = mao_->GetSubSection(*sub_section_iter_);
    Entry entry = *entry_iter_;
    if (entry->index() == sub_section->last_entry_index()) {
      ++sub_section_iter_;

      if (sub_section_iter_ == sub_section_iter_end_) {
        entry_iter_ = mao_->EntryEnd();
        return *this;
      }

      sub_section = mao_->GetSubSection(*sub_section_iter_);
      while((*entry_iter_)->index() != sub_section->first_entry_index())
        ++entry_iter_;
    } else {
      ++entry_iter_;
    }
    return *this;
  }

  SectionEntryIterator operator ++(int) {
    SectionEntryIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  SectionEntryIterator &operator --() {
    SubSection *sub_section = mao_->GetSubSection(*sub_section_iter_);
    Entry entry = *entry_iter_;
    if (entry->index() == sub_section->first_entry_index()) {
      --sub_section_iter_;
      sub_section = mao_->GetSubSection(*sub_section_iter_);
      while((*entry_iter_)->index() != sub_section->last_entry_index())
        --entry_iter_;
    } else {
      --entry_iter_;
    }
    return *this;
  }

  SectionEntryIterator operator --(int) {
    SectionEntryIterator tmp = *this;
    --(*this);
    return tmp;
  }

  bool operator ==(const SectionEntryIterator &other) const {
    return (mao_ == other.mao_ &&
            sub_section_iter_ == other.sub_section_iter_ &&
            entry_iter_ == other.entry_iter_);
  }

  bool operator !=(const SectionEntryIterator &other) const {
    return !((*this) == other);
  }

 private:
  MaoUnit *mao_;
  std::vector<subsection_index_t>::iterator sub_section_iter_;
  std::vector<subsection_index_t>::iterator sub_section_iter_end_;
  std::list<MaoUnitEntryBase *>::iterator entry_iter_;
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
  std::vector<subsection_index_t> *GetSubSectionIndexes() {
    return &sub_section_indexes_;
  }

  SectionEntryIterator EntryBegin(MaoUnit *mao) {
    std::vector<subsection_index_t>::iterator sub_section_iter =
        sub_section_indexes_.begin();

    if (sub_section_iter != sub_section_indexes_.end()) {
      SubSection *sub_section = mao->GetSubSection(*sub_section_iter);

      std::list<MaoUnitEntryBase *>::iterator entry_iter = mao->EntryBegin();
      while((*entry_iter)->index() != sub_section->first_entry_index())
        ++entry_iter;
      return SectionEntryIterator(mao, sub_section_iter,
                                  sub_section_indexes_.end(), entry_iter);
    } else {
      return SectionEntryIterator(mao, sub_section_iter,
                                  sub_section_indexes_.end(), mao->EntryEnd());
    }
  }

  SectionEntryIterator EntryEnd(MaoUnit *mao) {
    return SectionEntryIterator(mao, sub_section_indexes_.end(),
                                sub_section_indexes_.end(), mao->EntryEnd());
  }

 private:
  char *name_;  // e.g. ".text", ".data", etc.
  std::vector<subsection_index_t> sub_section_indexes_;
};


#endif  // MAOUNIT_H_
