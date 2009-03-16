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
#include <string>
#include <utility>
#include <vector>

#include "gen-opcodes.h"
#include "ir-gas.h"
#include "irlink.h"
#include "MaoOptions.h"
#include "MaoUtil.h"
#include "MaoDebug.h"

class CFG;
class LoopStructureGraph;

// TODO(martint): Find a better way make Section available in the Symbol table
class Section;
#include "SymbolTable.h"

#define DEFAULT_SECTION_NAME ".text"

typedef int ID;

// Each ID is uniq within its own domain.
typedef ID EntryID;
typedef ID SectionID;
typedef ID SubSectionID;
typedef ID FunctionID;

class ConstSectionIterator;
class DirectiveEntry;
class Function;
class InstructionEntry;
class LabelEntry;
class MaoEntry;
class Section;
class SectionIterator;
class SubSection;
class Symbol;
class SymbolTable;

// Used by the STL-maps of sections and subsections.
struct ltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};


class Stat {
 public:
  virtual ~Stat() {}
  virtual void Print(FILE *out) = 0;
  void Print() {Print(stdout);}
 private:
};

// Print all stats to the same file.
class Stats {
 public:
  Stats() {
    stats_.clear();
  }
  ~Stats() {
    for (std::map<const char *, Stat *, ltstr>::iterator iter = stats_.begin();
        iter != stats_.end(); ++iter) {
      delete iter->second;
    }
  }
  void Add(const char *name, Stat *stat) {
    MAO_ASSERT(!HasStat(name));
    stats_[name] = stat;
  }

  bool HasStat(const char *name) const {
    return stats_.find(name) != stats_.end();
  }

  Stat *GetStat(const char *name)  {
    MAO_ASSERT(HasStat(name));
    return stats_[name];
  }

  void Print(FILE *out) {
    for (std::map<const char *, Stat *, ltstr>::iterator iter = stats_.begin();
        iter != stats_.end(); ++iter) {
      iter->second->Print(out);
    }
  }
  void Print() {Print(stdout);}
 private:
  std::map<const char *, Stat *, ltstr> stats_;
};

class MaoUnit {
 public:

  typedef std::vector<MaoEntry *>      EntryVector;
  typedef EntryVector::iterator        EntryIterator;
  typedef EntryVector::const_iterator  ConstEntryIterator;


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

  // Note that a given subsection identifier can occur in many places in the
  // code. (.text, .data, .text.)
  // This will be create several SubSection objects with .text in the
  // sub_sections_ member.
  bool SetSubSection(const char *section_name, unsigned int subsection_number,
                     MaoEntry *entry);

  SubSection *GetSubSection(unsigned int subsection_number) {
    return sub_sections_[subsection_number];
  }

  LabelEntry *GetLabelEntry(const char *label_name) const;

  // Instruction Creators
  InstructionEntry *CreateInstruction(const char *opcode);
  InstructionEntry *CreateNop();
  LabelEntry *CreateLabel(const char *labelname);

  // Dumpers
  void PrintMaoUnit() const;
  void PrintMaoUnit(FILE *out) const;
  void PrintIR(bool print_entries, bool print_sections,
               bool print_subsections, bool print_functions) const;
  void PrintIR(FILE *out, bool print_entries, bool print_sections,
               bool print_subsections, bool print_functions) const;


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
  void FindFunctions();

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

 private:
  // Create the section section_name if it does not already exists. Returns a
  // pointer the section.
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

  Stats stats_;
};  // MaoUnit

// Iterator wrapper for iterating over all the Sections in a MaoUnit.
class SectionIterator {
 public:
  SectionIterator(std::map<const char *, Section *, ltstr>::iterator
                  section_iter)
      : section_iter_(section_iter) { }
  Section *&operator *() const;
  SectionIterator &operator ++();
  SectionIterator &operator --();
  bool operator ==(const SectionIterator &other) const;
  bool operator !=(const SectionIterator &other) const;
 private:
  std::map<const char *, Section *, ltstr>::iterator section_iter_;
};

class ConstSectionIterator {
 public:
  ConstSectionIterator(std::map<const char *, Section *, ltstr>::const_iterator
                       section_iter)
      : section_iter_(section_iter) { }
  Section *const &operator *() const;
  ConstSectionIterator const &operator ++();
  ConstSectionIterator const &operator --();
  bool operator ==(const ConstSectionIterator &other) const;
  bool operator !=(const ConstSectionIterator &other) const;
 private:
  std::map<const char *, Section *, ltstr>::const_iterator section_iter_;
};

// Forward Decls
class InstructionEntry;
class DirectiveEntry;
class LabelEntry;

// base class for all types of entries in the MaoUnit. Example of entries
// are Labels, Directives, and Instructions
class MaoEntry {
 public:
  // Types of possible entries
  enum EntryType {
    UNDEFINED = 0,
    INSTRUCTION,
    LABEL,
    DIRECTIVE
  };

  MaoEntry(unsigned int line_number, const char *const line_verbatim,
           const MaoUnit *maounit);
  virtual ~MaoEntry();

  virtual void PrintEntry(FILE *out) const = 0;
  void PrintSourceInfo(FILE *out) const;
  virtual void PrintIR(FILE *out) const = 0;
  virtual char GetDescriptiveChar() const = 0;
  virtual EntryType Type() const = 0;

  const EntryID id() const { return id_; }
  void set_id(const EntryID id) {id_ = id;}

  bool IsInstruction() { return Type() == INSTRUCTION; }
  bool IsLabel() { return Type() == LABEL; }
  bool IsDirective() { return Type() == DIRECTIVE; }

  InstructionEntry *AsInstruction();
  LabelEntry *AsLabel();
  DirectiveEntry *AsDirective();

  void set_next(MaoEntry *entry) {next_ = entry;}
  void set_prev(MaoEntry *entry) {prev_ = entry;}

  MaoEntry *next() {return next_;}
  MaoEntry *prev() {return prev_;}

  InstructionEntry *nextInstruction();
  InstructionEntry *prevInstruction();

  // Take 'entry' and link it in before/after current instruction
  void LinkBefore(MaoEntry *entry);
  void LinkAfter(MaoEntry *entry);

  unsigned int line_number() const { return line_number_; }
  const char *const line_verbatim() const { return line_verbatim_; }

  const char *GetSymbolnameFromExpression(expressionS *expr) const;

 protected:
  // Helper function to indent
  void Spaces(unsigned int n, FILE *outfile) const;
  // Gets symbolname. Understands the temporary symbolname
  // that should be translated to a dot.
  const char *GetDotOrSymbol(symbolS *symbol) const;

  const MaoUnit *maounit_;

 private:
  EntryID id_;

  // Section-local pointers to previous and next entry.
  // Null-value in prev/next means first/last entry in section.
  MaoEntry *next_;
  MaoEntry *prev_;

  // Line number assembly was found in the original file
  const unsigned int line_number_;
  // A verbatim copy of the assembly instruction this entry is
  // generated from. Might be NULL for some entries.
  const char *line_verbatim_;

  // This flag is true for entries that have been added
  // by mao.
  // For labels, this means that there is NO corresponding
  // entry in the gas symbol table.s
  bool mao_local_;
};


class LabelEntry : public MaoEntry {
 public:
  LabelEntry(const char *const name,
             unsigned int line_number,
             const char *const line_verbatim,
             const MaoUnit *maounit)
      : MaoEntry(line_number, line_verbatim, maounit),
        name_(strdup(name)) { }
  ~LabelEntry() { delete name_; }

  virtual void PrintEntry(FILE *out) const;
  virtual void PrintIR(FILE *out) const;
  virtual EntryType Type() const { return LABEL; }
  const char *name() { return name_; }
  virtual char GetDescriptiveChar() const { return 'L'; }

 private:
  const char *const name_;
};

// An Entry of the type Directive
class DirectiveEntry : public MaoEntry {
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
    SET,         // Same as .equ
    EQUIV,
    WEAKREF,
    ARCH,
    LINEFILE,
    LOC,
    NUM_OPCODES  // Used to get the size of the array
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
      data.expr = static_cast<expressionS *>(malloc(sizeof(expressionS)));
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
                 unsigned int line_number, const char* line_verbatim,
                 const MaoUnit *maounit)
      : MaoEntry(line_number, line_verbatim, maounit),
        op_(op), operands_(operands) { }

  virtual ~DirectiveEntry() {
    for (OperandVector::iterator iter = operands_.begin();
         iter != operands_.end(); ++iter) {
      delete *iter;
    }
  }

  Opcode op() const { return op_; }
  const char *GetOpcodeName() const {
    return kOpcodeNames[op_];
  }

  int NumOperands() const { return operands_.size(); }
  const Operand *GetOperand(int num) { return operands_[num]; }

  virtual void PrintEntry(::FILE *out) const;
  virtual void PrintIR(::FILE *out) const;
  virtual MaoEntry::EntryType  Type() const;
  virtual char GetDescriptiveChar() const {return 'D';}

  bool IsJumpTableEntry() const;

  // For indirect jumps, this instruction finds out the
  // label identifying the jump table used. If no such label,
  // can be found, it return NULL.
  LabelEntry *GetJumpTableTarget();

 private:
  const std::string &OperandsToString(std::string *out,
                                      const char *separator) const;
  const std::string &OperandToString(const Operand &operand,
                                     std::string *out) const;

  const std::string &OperandExpressionToString(const expressionS *expr,
                                               std::string *out) const;

  const char *GetOperandSeparator() const;

  // op_ holds the type of directive
  const Opcode op_;

  // operands_ hold the operands of the directive
  OperandVector operands_;

  bool IsDebugDirective() const;
};


// An Entry of the type Instruction.
class InstructionEntry : public MaoEntry {
 public:
  InstructionEntry(i386_insn* instruction, unsigned int line_number,
                   const char* line_verbatim, const MaoUnit *maounit);
  ~InstructionEntry();
  virtual void PrintEntry(FILE *out) const;
  virtual void PrintIR(FILE *out) const;
  virtual MaoEntry::EntryType  Type() const;

  const char *GetTarget() const;
  virtual char GetDescriptiveChar() const {return 'I';}

  void PrintInstruction(FILE *out) const;
  static const unsigned int kMaxRegisterNameLength = MAX_REGISTER_NAME_LENGTH;

  const char *op_str() const;
  MaoOpcode   op() const { return op_; }
  void        set_op(MaoOpcode op) { op_ = op; }

  // Property methods.
  bool HasTarget() const;
  bool HasFallThrough() const;
  bool IsControlTransfer() const {
    return HasTarget() || IsCall() || IsReturn();
  }
  bool IsIndirectJump() const;
  bool IsCondJump() const;
  bool IsJump() const;
  bool IsCall() const;
  bool IsReturn() const;
  bool IsOpMov() const { return op() == OP_mov || op() == OP_movq; }
  bool IsPredicated() const;

  int NumOperands() {
    return instruction()->operands;
  }
  bool IsMemOperand(const unsigned int op_index) {
    return IsMemOperand(instruction(), op_index);
  }
  bool IsMem8Operand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.disp8;
  }
  bool IsMem16Operand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.disp16;
  }
  bool IsMem32Operand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.disp32 ||
      instruction_->types[op_index].bitfield.disp32s;
  }
  bool IsMem64Operand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.disp64;
  }
  bool IsImmediateOperand(const unsigned int op_index) {
    return IsImmediateOperand(instruction(), op_index);
  }
  bool IsRegisterOperand(const unsigned int op_index) {
    return IsRegisterOperand(instruction(), op_index);
  }
  bool IsRegister8Operand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.reg8 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.byte);
  }
  bool IsRegister16Operand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.reg16 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.word);
  }
  bool IsRegister32Operand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.reg32 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.dword);
  }
  bool IsRegister64Operand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.reg64 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.qword);
  }
  bool IsRegisterFloatOperand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.floatreg;
  }
  bool IsRegisterXMMOperand(const unsigned int op_index) {
    return instruction_->types[op_index].bitfield.regxmm;
  }

  bool CompareMemOperand(int op1, InstructionEntry *i2, int op2);
  void SetOperand(int op1, InstructionEntry *i2, int op2);

  i386_insn   *instruction() { return instruction_; }

  const char  *GetRegisterOperandStr(const unsigned int op_index) {
    return instruction_->op[op_index].regs->reg_name;
  }
  const struct reg_entry *GetRegisterOperand(const unsigned int op_index) {
    return instruction_->op[op_index].regs;
  }

  bool HasBaseRegister() const;
  bool HasIndexRegister() const;

  const char  *GetBaseRegisterStr() const;
  const char  *GetIndexRegisterStr() const;
  const struct reg_entry *GetBaseRegister() const;
  const struct reg_entry *GetIndexRegister() const;

  unsigned int GetLog2ScaleFactor();

 private:
  i386_insn *instruction_;
  MaoOpcode  op_;

  // Used to determine type of operand.
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
  void PrintImmediateOperand(FILE *out,
                             const enum bfd_reloc_code_real reloc,
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


class SectionEntryIterator {
  // This class uses the prev/next of the Entries to move.
 public:
  typedef MaoEntry *Entry;
  explicit SectionEntryIterator(MaoEntry *entry);
  MaoEntry *operator *() {return current_entry_;}
  SectionEntryIterator &operator ++();
  SectionEntryIterator &operator --();
  bool operator ==(const SectionEntryIterator &other) const;
  bool operator !=(const SectionEntryIterator &other) const;
 private:
  MaoEntry *current_entry_;  // NULL is used for signalling the end.
};



// A Subsection is part of a section. The subsection concept allows the assembly
// file to write the code more freely, but still keep the data organized in
// sections.
class SubSection {
 public:
  // Constructor needs subsection number, a pointer to the actual section, and
  // the assembly code needed to create the subsection.
  explicit SubSection(const SubSectionID id, unsigned int subsection_number,
                      const char *name, Section *section)
      : number_(subsection_number),
        name_(name),
        id_(id),
        first_entry_(NULL),
        last_entry_(NULL),
        start_section_(false),
        section_(section) { }

  unsigned int number() const { return number_; }
  const std::string &name() const { return name_; }

  MaoEntry *first_entry() const { return first_entry_;}
  MaoEntry *last_entry() const { return last_entry_;}
  void set_first_entry(MaoEntry *entry) { first_entry_ = entry;}
  void set_last_entry(MaoEntry *entry);
  SubSectionID id() const { return id_;}

  void set_start_section(bool value) {start_section_ = value;}
  bool start_section() const {return start_section_;}

  Section *section() const {return section_;}

  SectionEntryIterator EntryBegin();
  SectionEntryIterator EntryEnd();

 private:
  // The subsection number
  const unsigned int number_;
  const std::string name_;

  const SubSectionID id_;

  // Points to the first and last entry for the subsection.
  MaoEntry *first_entry_;
  MaoEntry *last_entry_;

  // For special section that holds the inital directies that are not
  // part of the first "real" section.
  bool start_section_;

  // Points to the section
  Section *section_;
};

// Function class
// A function is defined as a sequence of instructions from a
// label matching a symbol with the type Function to the next function,
// or to the end of the section.
class Function {
 public:
  explicit Function(const std::string &name, const FunctionID id,
                    SubSection *subsection) :
      name_(name), id_(id), first_entry_(NULL), last_entry_(NULL),
      subsection_(subsection), cfg_(NULL), lsg_(NULL) {}

  void set_first_entry(MaoEntry *entry) { first_entry_ = entry;}
  void set_last_entry(MaoEntry *entry) {
    last_entry_ = entry;
    if (last_entry_->next())
      end_entry_ = last_entry_->next();
    else
      // TODO(rhundt): insert dummy node, but for now:
      end_entry_ = last_entry_;
  }
  MaoEntry *first_entry() const { return first_entry_;}
  MaoEntry *last_entry() const { return last_entry_;}
  MaoEntry *end_entry() const { return end_entry_;}

  const std::string name() const {return name_;}
  FunctionID id() const {return id_;}

  // TODO(martint): Reconsider iterator name.
  SectionEntryIterator EntryBegin();
  SectionEntryIterator EntryEnd();

  CFG *cfg() const {return cfg_;}
  void set_cfg(CFG *cfg);

  LoopStructureGraph *lsg() const {return lsg_;}
  void set_lsg(LoopStructureGraph *lsg) {lsg_ = lsg;}


  void Print();
  void Print(FILE *out);

  Section *GetSection() {
    MAO_ASSERT(subsection_);
    return subsection_->section();
  }

  // Accesses the size_map associated for the section
  // this function is in.
  std::map<MaoEntry *, int> *sizes();
  void set_sizes(std::map<MaoEntry *, int> *sizes);

 private:
  // Name of the function, as given by the function symbol.
  const std::string name_;

  // The uniq id of this function.
  const FunctionID id_;

  // Pointers to the first and last entry of the function.
  MaoEntry *first_entry_;
  MaoEntry *last_entry_;
  MaoEntry *end_entry_;

  // Pointer to subsection that this function starts in.
  SubSection *subsection_;

  /////////////////////////////////////////
  // members populated by analysis passes

  // Pointer to CFG, if one is build for the function.
  CFG *cfg_;
  // Pointer to Loop Structure Graph, if one is build for the function
  LoopStructureGraph *lsg_;
};

// Convenience macros
#define FORALL_FUNC_ENTRY(func, entry) \
  for (MaoEntry *entry = func->first_entry(); \
       entry != func->end_entry(); \
       entry = entry->next())

// A section
class Section {
 public:
  // Memory for the name is allocated in the constructor and freed
  // in the destructor.
  explicit Section(const char *name, const SectionID id) :
      name_(name), id_(id), sizes_(NULL) {}
  ~Section() {}
  std::string name() const {return name_;}
  SectionID id() const {return id_;}

  void AddSubSection(SubSection *subsection);

  SectionEntryIterator EntryBegin();
  SectionEntryIterator EntryEnd();

  std::vector<SubSectionID> GetSubsectionIDs() const;

  // Return the last subsection in the section or NULL if section is empty.
  SubSection *GetLastSubSection() const;

  std::map<MaoEntry *, int> *sizes() {return sizes_;}
  void set_sizes(std::map<MaoEntry *, int> *sizes) {
    // Deallocate memory if needed
    if (sizes_ != NULL) {
      delete sizes_;
    }
    sizes_ = sizes;
  }

 private:

  const std::string name_;  // e.g. ".text", ".data", etc.
  const SectionID id_;

  std::vector<SubSection *> subsections_;

  // Sizes as determined by the relaxer
  // TODO(martint): fix types to use the named type in relax.h
  // NULL if not set.
  std::map<MaoEntry *, int> *sizes_;
};

#endif  // MAOUNIT_H_
