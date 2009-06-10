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
#include <stack>
#include <string>
#include <utility>
#include <vector>

extern "C" {
  #include "as.h"
  #include "tc-i386.h"
}

#include "gen-opcodes.h"

#include "MaoOptions.h"
#include "MaoDebug.h"
#include "MaoCFG.h"
#include "MaoStats.h"
#include "MaoLoops.h"
#include "MaoDefs.h"

class Section;
#include "SymbolTable.h"

#define MAO_MAJOR_VERSION 0
#define MAO_MINOR_VERSION 1

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
class MaoUnit;
class Section;
class SectionIterator;
class SubSection;
class Symbol;
class SymbolTable;

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
           MaoUnit *maounit);
  virtual ~MaoEntry();

  virtual std::string &ToString(std::string *out) const = 0;
  virtual void PrintEntry(FILE *out = stderr) const = 0;
  std::string &SourceInfoToString(std::string *out) const;
  virtual void PrintIR(FILE *out = stderr) const = 0;
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

  void Unlink();
  // Take 'entry' and link it in before/after current instruction
  void LinkBefore(MaoEntry *entry);
  void LinkAfter(MaoEntry *entry);

  // Align an entry via .p2align
  void AlignTo(int power_of_2_alignment, // 0: 1, 1: 2, 2: 4, 3: 8, 4: 16, 5: 32
               int fill_value = -1,      // -1: no parameter, leave empty in .p2align
               int max_bytes_to_skip = 0);

  unsigned int line_number() const { return line_number_; }
  const char *const line_verbatim() const { return line_verbatim_; }

  const char *GetSymbolnameFromExpression(expressionS *expr) const;

 protected:
  // Helper function to indent
  void Spaces(unsigned int n, FILE *outfile) const;
  // Gets symbolname. Understands the temporary symbolname
  // that should be translated to a dot.
  std::string GetDotOrSymbol(symbolS *symbol) const;

  const std::string &RelocToString(const enum bfd_reloc_code_real reloc,
                                   std::string *out) const;

  MaoUnit *maounit_;

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
             MaoUnit *maounit)
      : MaoEntry(line_number, line_verbatim, maounit),
        name_(strdup(name)), from_assembly_(true) { }
  ~LabelEntry() { delete name_; }

  virtual std::string &ToString(std::string *out) const;
  virtual void PrintEntry(FILE *out = stderr) const;
  virtual void PrintIR(FILE *out) const;
  virtual EntryType Type() const { return LABEL; }
  const char *name() { return name_; }
  virtual char GetDescriptiveChar() const { return 'L'; }
  bool from_assembly() const { return from_assembly_; }
  void set_from_assembly(bool value) { from_assembly_ = value; }

 private:
  const char *const name_;
  // Only labels that are read from the assembly is in the gas symbol table.
  // Local labels generated in mao are currently not inserted into the gas
  // symbol table. Labels can be generated when splitting basic blocks,
  // and for indirect jump patterns.
  bool from_assembly_;
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
    ALLOW_INDEX_REG,
    DISALLOW_INDEX_REG,
    ORG,
    CODE16,
    CODE16GCC,
    CODE32,
    CODE64,
    DC_D,
    DC_S,
    DC_X,
    NUM_OPCODES  // Used to get the size of the array
  };

  static const char *const kOpcodeNames[NUM_OPCODES];


  // The EXPRESSION_RELOC is an operand used in cons directives
  // (.byte, .long etc) _if_ a relocation is given.
  enum OperandType {
    NO_OPERAND = 0,
    STRING,
    INT,
    SYMBOL,
    EXPRESSION,
    EXPRESSION_RELOC,
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
    explicit Operand(expressionS *expr, const enum bfd_reloc_code_real reloc)
        : type(EXPRESSION_RELOC) {
      data.expr = static_cast<expressionS *>(malloc(sizeof(expressionS)));
      *data.expr_reloc.expr = *expr;
      data.expr_reloc.reloc = reloc;
    }

    explicit Operand(int value) : type(INT) { data.i = value; }
    ~Operand() {
      if (type == STRING)
        delete data.str;
      if (type == EXPRESSION)
        free(data.expr);
    }

    OperandType type;
    struct expr_reloc_s {
      expressionS *expr;
      enum bfd_reloc_code_real reloc;
    };

    union  {
      std::string *str;
      int i;
      symbolS *sym;
      expressionS *expr;
      struct expr_reloc_s expr_reloc;
    } data;
  };

  typedef std::vector<Operand *> OperandVector;

  DirectiveEntry(Opcode op, const OperandVector &operands,
                 unsigned int line_number, const char* line_verbatim,
                 MaoUnit *maounit)
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

  virtual std::string &ToString(std::string *out) const;
  virtual void PrintEntry(::FILE *out = stderr) const;
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

  const char *OpToString(operatorT op) const;
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
  static const unsigned int kMaxRegisterNameLength = MAX_REGISTER_NAME_LENGTH;

  InstructionEntry(i386_insn* instruction, enum flag_code entry_mode,
                   unsigned int line_number, const char* line_verbatim,
                   MaoUnit *maounit);
  ~InstructionEntry();
  virtual std::string &ToString(std::string *out) const;
  virtual void PrintEntry(FILE *out = stderr) const;
  virtual void PrintIR(FILE *out) const;
  virtual MaoEntry::EntryType  Type() const;

  const char *GetTarget() const;
  virtual char GetDescriptiveChar() const {return 'I';}

  bool HasPrefix(char prefix) const;
  bool SuppressLockPrefix() const;

  std::string &InstructionToString(std::string *out) const;
  std::string &ProfileToString(std::string *out) const;


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
  bool IsAdd() const;
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
  const reg_entry *GetRegisterOperand(const unsigned int op_index) {
    return instruction_->op[op_index].regs;
  }

  bool HasBaseRegister() const;
  bool HasIndexRegister() const;

  const char  *GetBaseRegisterStr() const;
  const char  *GetIndexRegisterStr() const;
  const reg_entry *GetBaseRegister() const;
  const reg_entry *GetIndexRegister() const;

  unsigned int GetLog2ScaleFactor();

  void IncrementExecutionCount(long increment) {
    if (!execution_count_valid_) {
      execution_count_valid_ = true;
      execution_count_ = 0;
    }
    execution_count_ += increment;
  }

  void SetExecutionCount(long count) {
    execution_count_valid_ = true;
    execution_count_ = count;
  }

  bool HasExecutionCount() {
    return execution_count_valid_;
  }

  long GetExecutionCount() {
    return execution_count_valid_ ? execution_count_ : -1;
  }

  enum flag_code GetFlag() const { return code_flag_; }

  /* Returns 0 if attempting to add a prefix where one from the same
     class already exists, 1 if non rep/repne added, 2 if rep/repne
     added.  */
  int AddPrefix(unsigned int prefix);

 private:
  i386_insn *instruction_;
  MaoOpcode  op_;

  // This flag states which code-mode the instruction is in. This is changed
  // in the assembly file using the .codeXX directives.
  // Values can be CODE_16BIT, CODE_32BIT, CODE_64BIT
  enum flag_code code_flag_;

  // An execution count per instruction.  execution_count_ contains
  // usable data if and only if execution_count_valid_ is true.
  bool execution_count_valid_;
  long execution_count_;

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
  expressionS *CreateExpressionCopy(expressionS *in_exp);
  bool EqualExpressions(expressionS *expression1,
                        expressionS *expression2) const;



  reg_entry *CopyRegEntry(const reg_entry *in_reg);
  // Frees all allocated memory for this instruction
  void FreeInstruction();
  bool IsInList(const MaoOpcode opcode, const MaoOpcode list[],
                const unsigned int number_of_elements) const;
  std::string &ImmediateOperandToString(std::string *out,
                                        const enum bfd_reloc_code_real reloc,
                                        const expressionS *expr) const;
  std::string &MemoryOperandToString(std::string *out,
                                     int op_index) const;

  int StripRexPrefix(int prefix) const;

  std::string &PrintRexPrefix(std::string *out, int prefix) const;

  // Returns the instruction op string, including any suffix.
  const std::string GetAssemblyInstructionName() const;
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

  // Gets the entry corresponding label_name. Returns NULL
  // if no such label is found.
  LabelEntry *GetLabelEntry(const char *label_name) const;

  // Instruction Creators
  InstructionEntry *CreateInstruction(const char *opcode,
                                      unsigned int base_opcode,
                                      Function *function);
  InstructionEntry *CreateNop(Function *function);
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


  bool Is64BitMode() const { return arch_ == X86_64; }
  bool Is32BitMode() const { return arch_ == I386;   }

  void PushCurrentSubSection();
  void PopSubSection(int line_number);
  void SetPreviousSubSection(int line_number);

 private:

  enum Arch {
    I386,
    X86_64
  };
  enum Arch arch_;

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
  // Previous subsection is needed for the .previous directive
  SubSection *prev_subsection_;
  std::stack<SubSection *> subsection_stack_;


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





class SectionEntryIterator {
  // This class uses the prev/next of the Entries to move.
 public:
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

  void Print();
  void Print(FILE *out);

  Section *GetSection() {
    MAO_ASSERT(subsection_);
    return subsection_->section();
  }

  SubSection *GetSubSection() {
    MAO_ASSERT(subsection_);
    return subsection_;
  }


 private:
  // These methods are to be used by the respective analyses to cache
  // analysis results.
  CFG *cfg() const {return cfg_;}
  void set_cfg(CFG *cfg);
  friend CFG *CFG::GetCFG(MaoUnit *mao, Function *function);
  friend CFG *CFG::GetCFGIfExists(const MaoUnit *mao, Function *function);
  friend void CFG::InvalidateCFG(Function *function);

  LoopStructureGraph *lsg() const {return lsg_;}
  void set_lsg(LoopStructureGraph *lsg) {lsg_ = lsg;}
  friend LoopStructureGraph *LoopStructureGraph::GetLSG(MaoUnit *mao,
                                                        Function *function);


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

// Global Map Types
typedef std::map<MaoEntry *, int> MaoEntryIntMap;

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
    name_(name), id_(id), sizes_(NULL), offsets_(NULL) {}
  ~Section() {}
  std::string name() const {return name_;}
  SectionID id() const {return id_;}

  void AddSubSection(SubSection *subsection);

  SectionEntryIterator EntryBegin() const;
  SectionEntryIterator EntryEnd() const;

  std::vector<SubSectionID> GetSubsectionIDs() const;

  // Return the last subsection in the section or NULL if section is empty.
  SubSection *GetLastSubSection() const;

  // The next 4 methods are only to be used by MaoRelaxer.  Others
  // should invoke the utility functions there to get access to the
  // size and offset.
  MaoEntryIntMap *sizes() {return sizes_;}
  MaoEntryIntMap *offsets() {return offsets_;}
  void set_sizes(MaoEntryIntMap *sizes) {
    if (sizes_ != NULL)
      delete sizes_;
    sizes_ = sizes;
  }
  void set_offsets(MaoEntryIntMap *offsets) {
    if (offsets_ != NULL)
      delete offsets_;
    offsets_ = offsets;
  }

 private:

  const std::string name_;  // e.g. ".text", ".data", etc.
  const SectionID id_;

  std::vector<SubSection *> subsections_;

  // Sizes as determined by the relaxer
  // TODO(martint): fix types to use the named type in relax.h
  // NULL if not set.
  MaoEntryIntMap *sizes_;

  // Store corresponding entry offsets
  MaoEntryIntMap *offsets_;
};





#endif  // MAOUNIT_H_
