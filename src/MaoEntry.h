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

#ifndef MAOENTRY_H_
#define MAOENTRY_H_

#include <string>
#include <utility>
#include <vector>

extern "C" {
  #include "as.h"
  #include "tc-i386.h"
}
#include "gen-opcodes.h"
#include "irlink.h"

#include "MaoDebug.h"
#include "MaoTypes.h"

// Forward declarations
class MaoUnit;
class DirectiveEntry;
class InstructionEntry;
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
  virtual enum flag_code GetFlag() const;

  const EntryID id() const { return id_; }
  void set_id(const EntryID id) {id_ = id;}

  bool IsInstruction() const { return Type() == INSTRUCTION; }
  bool IsLabel() const { return Type() == LABEL; }
  bool IsDirective() const { return Type() == DIRECTIVE; }

  InstructionEntry *AsInstruction();
  LabelEntry *AsLabel();
  DirectiveEntry *AsDirective();

  void set_next(MaoEntry *entry) {next_ = entry;}
  void set_prev(MaoEntry *entry) {prev_ = entry;}

  MaoEntry *next() const {return next_;}
  MaoEntry *prev() const {return prev_;}

  InstructionEntry *nextInstruction();
  InstructionEntry *prevInstruction();

  void Unlink();
  void Unlink(MaoEntry *last_in_chain);
  // Take 'entry' and link it in before/after current instruction
  void LinkBefore(MaoEntry *entry);
  void LinkAfter(MaoEntry *entry);

  // Align an entry via .p2align
  void AlignTo(int power_of_2_alignment,  // 0:1, 1:2, 2:4, 3:8, 4:16, 5:32
               int fill_value = -1,  // -1: no param, leave empty in .p2align
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

  const std::string &ExpressionToStringDisp(
      const expressionS *expr,
      std::string *out,
      const enum bfd_reloc_code_real *reloc) const;
  const std::string &ExpressionToStringImmediate(
      const expressionS *expr,
      std::string *out,
       const enum bfd_reloc_code_real *reloc) const;
  const std::string &ExpressionToString(const expressionS *expr,
                                        std::string *out) const;

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

  const char *OpToString(operatorT op) const;

  const std::string &ExpressionToStringImp(
      const expressionS *expr,
      std::string *out,
      bool immediate,
      const enum bfd_reloc_code_real *reloc) const;

  // Return the last entry in a chain.
  MaoEntry *GetLastEntry(MaoEntry *entry) const;
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
    SUBSECTION,
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
    EQV,
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
    HIDDEN,
    FILL,
    STRUCT,
    INCBIN,
    SYMVER,
    LOC_MARK_LABELS,
    CFI_STARTPROC,
    CFI_ENDPROC,
    CFI_DEF_CFA,
    CFI_DEF_CFA_REGISTER,
    CFI_DEF_CFA_OFFSET,
    CFI_ADJUST_CFA_OFFSET,
    CFI_OFFSET,
    CFI_REL_OFFSET,
    CFI_REGISTER,
    CFI_RETURN_COLUMN,
    CFI_RESTORE,
    CFI_UNDEFINED,
    CFI_SAME_VALUE,
    CFI_REMEMBER_STATE,
    CFI_RESTORE_STATE,
    CFI_WINDOW_SAVE,
    CFI_ESCAPE,
    CFI_SIGNAL_FRAME,
    CFI_PERSONALITY,
    CFI_LSDA,
    CFI_VAL_ENCODED_ADDR,
    NUM_OPCODES  // Used to get the size of the array
  };
#define NUM_DATA_DIRECTIVES  8
  static const Opcode data_directives[NUM_DATA_DIRECTIVES];


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

  bool IsDataDirective() {
    int i;
    for (i=0; i<NUM_DATA_DIRECTIVES; i++) {
      if (data_directives[i] == op_)
        return true;
    }
    return false;
  }

  int NumOperands() const { return operands_.size(); }
  const Operand *GetOperand(int num) { return operands_[num]; }

  virtual std::string &ToString(std::string *out) const;
  virtual void PrintEntry(::FILE *out = stderr) const;
  virtual void PrintIR(::FILE *out) const;
  virtual MaoEntry::EntryType  Type() const;
  virtual char GetDescriptiveChar() const {return 'D';}

  bool IsJumpTableEntry() const;
  bool IsCFIDirective() const;
  bool IsAlignDirective() const;

  // For indirect jumps, this instruction finds out the
  // label identifying the jump table used. If no such label,
  // can be found, it return NULL.
  LabelEntry *GetJumpTableTarget();

 private:
  const std::string &OperandsToString(std::string *out,
                                      const char *separator) const;
  const std::string &OperandToString(const Operand &operand,
                                     std::string *out) const;

  const char *GetOperandSeparator() const;

  // op_ holds the type of directive
  const Opcode op_;

  // operands_ hold the operands of the directive
  OperandVector operands_;

  bool IsDebugDirective() const;

  bool IsInList(const Opcode opcode, const Opcode list[],
                const unsigned int number_of_elements) const;
};


// An Entry of the type Instruction.
class InstructionEntry : public MaoEntry {
 public:
  static const unsigned int kMaxRegisterNameLength = MAX_REGISTER_NAME_LENGTH;

  InstructionEntry(i386_insn* instruction, enum flag_code flag_code,
                   unsigned int line_number, const char* line_verbatim,
                   MaoUnit *maounit);
  ~InstructionEntry();
  virtual std::string &ToString(std::string *out) const;
  virtual void PrintEntry(FILE *out = stderr) const;
  virtual void PrintIR(FILE *out) const;
  virtual MaoEntry::EntryType  Type() const;

  const char *GetTarget() const;
  virtual char GetDescriptiveChar() const {return 'I';}

  bool HasPrefix(unsigned char prefix) const;
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
  bool IsThunkCall() const;
  bool IsReturn() const;
  bool IsAdd() const;
  bool IsOpMov() const { return op() == OP_mov || op() == OP_movq; }
  bool IsLock() const { return op() == OP_lock; }
  bool IsPredicated() const;

  int NumOperands() const {
    return instruction()->operands;
  }
  bool IsMemOperand(const unsigned int op_index) const {
    return IsMemOperand(instruction(), op_index);
  }
  bool IsMem8Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.disp8 ||
        instruction_->types[op_index].bitfield.unspecified;
  }
  bool IsMem16Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.disp16 ||
        instruction_->types[op_index].bitfield.unspecified;
  }
  bool IsMem32Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.disp32 ||
        instruction_->types[op_index].bitfield.disp32s ||
        instruction_->types[op_index].bitfield.unspecified;
  }
  bool IsMem64Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.disp64 ||
        instruction_->types[op_index].bitfield.unspecified;
  }
  bool IsImmediateOperand(const unsigned int op_index) const {
    return IsImmediateOperand(instruction(), op_index);
  }
  bool IsRegisterOperand(const unsigned int op_index) const {
    return IsRegisterOperand(instruction(), op_index);
  }
  bool IsRegister8Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.reg8 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.byte);
  }
  bool IsRegister16Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.reg16 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.word);
  }
  bool IsRegister32Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.reg32 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.dword);
  }
  bool IsRegister64Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.reg64 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.qword);
  }
  bool IsRegisterFloatOperand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.floatreg;
  }
  bool IsRegisterXMMOperand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.regxmm;
  }
  bool IsStringOperation() {
    return instruction_->tm.opcode_modifier.isstring;
  }

  bool HasDisplacement(const int op_index) const {
    MAO_ASSERT(op_index < NumOperands());
    return instruction_->op[op_index].disps != NULL;
  }

  expressionS *GetDisplacement(const int op_index) const {
    MAO_ASSERT(HasDisplacement(op_index));
    return instruction_->op[op_index].disps;
  }

  bool CompareMemOperand(int op1, InstructionEntry *i2, int op2) const;
  void SetOperand(int op1, InstructionEntry *i2, int op2);

  i386_insn   *instruction() const { return instruction_; }

  const char  *GetRegisterOperandStr(const unsigned int op_index) const {
    return instruction_->op[op_index].regs->reg_name;
  }
  const reg_entry *GetRegisterOperand(const unsigned int op_index) const {
    return instruction_->op[op_index].regs;
  }
  const enum bfd_reloc_code_real GetReloc(int num) const {
    return instruction_->reloc[num];
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
  // Used to determine type of operand.
  static bool IsMemOperand(const i386_insn *instruction,
                           const unsigned int op_index);
  static bool IsImmediateOperand(const i386_insn *instruction,
                                 const unsigned int op_index);
  static bool  IsRegisterOperand(const i386_insn *instruction,
                                 const unsigned int op_index);

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
  std::string &MemoryOperandToString(std::string *out,
                                     int op_index) const;

  int StripRexPrefix(int prefix) const;

  std::string &PrintRexPrefix(std::string *out, int prefix) const;
  unsigned char GetRexPrefix() const;

  // Returns the instruction op string, including any suffix.
  std::string &GetAssemblyInstructionName(std::string *out) const;
};

class EntryIterator {
  // This class uses the prev/next of the Entries to move.
 public:
  explicit EntryIterator(MaoEntry *entry);
  MaoEntry *operator *() {return current_entry_;}
  EntryIterator &operator ++();
  EntryIterator &operator --();
  bool operator ==(const EntryIterator &other) const;
  bool operator !=(const EntryIterator &other) const;
 private:
  MaoEntry *current_entry_;  // NULL is used for signalling the end.
};


class ReverseEntryIterator {
  // This class uses the prev/next of the Entries to move.
 public:
  explicit ReverseEntryIterator(MaoEntry *entry);
  MaoEntry *operator *() {return current_entry_;}
  ReverseEntryIterator &operator ++();
  ReverseEntryIterator &operator --();
  bool operator ==(const ReverseEntryIterator &other) const;
  bool operator !=(const ReverseEntryIterator &other) const;
 private:
  MaoEntry *current_entry_;  // NULL is used for signalling the end.
};


#endif // MAOENTRY_H_
