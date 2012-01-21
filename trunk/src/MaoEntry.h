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
//
// Mao Entry - contains classes that implements entries which represent a line
// in the assembly file
// Classes:
//   MaoEntry - the base classe of all entries.
//   LabelEntry - Represents a label in assembly code.
//   DirectiveEntry - Represents assembler directives.
//   InstructionEntry - Represents an assembly instruction.
//   EntryIterator - An iterator over a list of entries.
//   ReverseEntryIterator - An iterator over list of entries that traverses from
//                          tail to head.
//
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

// Forward declarations.
class MaoUnit;
class DirectiveEntry;
class InstructionEntry;
class LabelEntry;

// Base class for all types of entries in the MaoUnit. Example of entries
// are Labels, Directives, and Instructions.
class MaoEntry {
 public:
  // Types of possible entries.
  enum EntryType {
    UNDEFINED = 0,
    INSTRUCTION,
    LABEL,
    DIRECTIVE
  };

  MaoEntry(unsigned int line_number, const char *const line_verbatim,
           MaoUnit *maounit);
  virtual ~MaoEntry();

  // Returns a string representation of the entry into out.
  virtual std::string &ToString(std::string *out) const = 0;
  // Prints the entry to FILE *out.
  virtual void PrintEntry(FILE *out = stderr) const = 0;
  // Prints the information corresponding to this entry in the input assembly
  // file.
  std::string &SourceInfoToString(std::string *out) const;
  // Prints the internal representation of this entry.
  virtual void PrintIR(FILE *out = stderr) const = 0;
  // Returns a character that describes the type of this entry (L for label, D
  // for directive and I for instruction).
  virtual char GetDescriptiveChar() const = 0;
  // Returns the type of this entry.
  virtual EntryType Type() const = 0;
  // Returns a flag that denotes if this 64, 32 or 16 bit code.
  virtual enum flag_code GetFlag() const;
  // Returns the id of this entry.
  const EntryID id() const { return id_; }
  // Sets the id of this entry.
  void set_id(const EntryID id) {id_ = id;}

  // Is this an instruction entry?
  bool IsInstruction() const { return Type() == INSTRUCTION; }
  // Is this a label entry?
  bool IsLabel() const { return Type() == LABEL; }
  // Is this a directive entry?
  bool IsDirective() const { return Type() == DIRECTIVE; }

  // Casts this pointer into InstructionEntry *. Aborts if this pointer is not
  // an InstructionEntry *.
  InstructionEntry *AsInstruction();
  // Casts this pointer into LabelEntry*. Aborts if this pointer is not an
  // LabelEntry *.
  LabelEntry *AsLabel();
  // Casts this pointer into DirectiveEntry*. Aborts if this pointer is not a
  // DirectiveEntry *.
  DirectiveEntry *AsDirective();

  // Sets the entry following this entry in the list of MAO entries in this unit.
  void set_next(MaoEntry *entry) {next_ = entry;}
  // Sets the entry preceding this entry in the list of MAO entries in this unit.
  void set_prev(MaoEntry *entry) {prev_ = entry;}

  // Returns the next entry pointer.
  MaoEntry *next() const {return next_;}
  // Returns the previous entry pointer.
  MaoEntry *prev() const {return prev_;}

  // Returns the next pointer in the entries list that is of type
  // InstructionEntry or NULL if there is no such entry.
  InstructionEntry *nextInstruction();
  // Returns the previous pointer in the entrieslist that is of type
  // InstructionEntry or NULL if there is no such entry.
  InstructionEntry *prevInstruction();

  // Removes this entry from the list of entries in the unit.
  void Unlink();
  // Removes  a list of entries, staring from this entry till last_in_chain
  // (both inclusive) from the list of entries in the unit.
  void Unlink(MaoEntry *last_in_chain);
  // Takes 'entry' and link it in before current instruction.
  void LinkBefore(MaoEntry *entry);
  // Takes 'entry' and link it in after current instruction.
  void LinkAfter(MaoEntry *entry);

  // Aligns an entry via .p2align.
  void AlignTo(int power_of_2_alignment,  // 0:1, 1:2, 2:4, 3:8, 4:16, 5:32
               int fill_value = -1,  // -1: no param, leave empty in .p2align
               int max_bytes_to_skip = 0);
  // Returns the line number of this entry in the assembly file.
  unsigned int line_number() const { return line_number_; }
  // Returns the original assembly line verbatim.
  const char *const line_verbatim() const { return line_verbatim_; }

  // Returns the symbols name from an expressionS *.
  const char *GetSymbolnameFromExpression(expressionS *expr) const;

 protected:
  // Helper function to indent.
  void Spaces(unsigned int n, FILE *outfile) const;
  // Gets symbolname. Understands the temporary symbolname
  // that should be translated to a dot.
  std::string GetDotOrSymbol(symbolS *symbol) const;

  // Returns a string representation of a reloc.
  const std::string &RelocToString(const enum bfd_reloc_code_real reloc,
                                   std::string *out) const;

  MaoUnit *maounit_;

  // Converts a displacement expression into a string.
  const std::string &ExpressionToStringDisp(
      const expressionS *expr,
      std::string *out,
      const enum bfd_reloc_code_real *reloc) const;
  // Converts an immediate expression into a string.
  const std::string &ExpressionToStringImmediate(
      const expressionS *expr,
      std::string *out,
       const enum bfd_reloc_code_real *reloc) const;
  // Converts an expression into a string.
  const std::string &ExpressionToString(const expressionS *expr,
                                        std::string *out) const;

 private:
  EntryID id_;

  // Section-local pointers to previous and next entry.
  // Null-value in prev/next means first/last entry in section.
  MaoEntry *next_;
  MaoEntry *prev_;

  // Line number assembly was found in the original file.
  const unsigned int line_number_;
  // A verbatim copy of the assembly instruction this entry is
  // generated from. Might be NULL for some entries.
  const char *line_verbatim_;

  // This flag is true for entries that have been added
  // by mao.
  // For labels, this means that there is NO corresponding
  // entry in the gas symbol table.s.
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

// Class to represent a label in an assembly file.
class LabelEntry : public MaoEntry {
 public:
  LabelEntry(const char *const name,
             unsigned int line_number,
             const char *const line_verbatim,
             MaoUnit *maounit)
      : MaoEntry(line_number, line_verbatim, maounit),
        name_(strdup(name)), from_assembly_(true) { }
  ~LabelEntry() { delete name_; }

  // Returns the string form of this label.
  virtual std::string &ToString(std::string *out) const;
  // Prints the label entry.
  virtual void PrintEntry(FILE *out = stderr) const;
  // Prints the internal representation of this label entry.
  virtual void PrintIR(FILE *out) const;
  virtual EntryType Type() const { return LABEL; }
  // Returns the label name.
  const char *name() { return name_; }
  virtual char GetDescriptiveChar() const { return 'L'; }
  // Returns true if this label is present in the original assembly file.
  bool from_assembly() const { return from_assembly_; }
  // Sets a flag indicating that the label is present in the input assembly
  // file.
  void set_from_assembly(bool value) { from_assembly_ = value; }

 private:
  const char *const name_;
  // Only labels that are read from the assembly is in the gas symbol table.
  // Local labels generated in mao are currently not inserted into the gas
  // symbol table. Labels can be generated when splitting basic blocks,
  // and for indirect jump patterns.
  bool from_assembly_;
};

// Class to represent assembler directives.
class DirectiveEntry : public MaoEntry {
  // examples of directives:
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
    NUM_OPCODES  // Used to get the size of the array.
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

  // Returns the opcode of this directive entry.
  Opcode op() const { return op_; }
  const char *GetOpcodeName() const {
    return kOpcodeNames[op_];
  }

  // Returns true if this is a data directive.
  bool IsDataDirective() {
    int i;
    for (i=0; i<NUM_DATA_DIRECTIVES; i++) {
      if (data_directives[i] == op_)
        return true;
    }
    return false;
  }

  // Returns the number of operands to this directive.
  int NumOperands() const { return operands_.size(); }
  // Returns the 'num'th  operand of this directive. Operand indeices start
  // from 0.
  const Operand *GetOperand(int num) { return operands_[num]; }

  // Returns a string representation of this directive entry.
  virtual std::string &ToString(std::string *out) const;
  // Prints this directive entry to FILE *out.
  virtual void PrintEntry(::FILE *out = stderr) const;
  // Prints this directive entry to FILE *out.
  virtual void PrintIR(::FILE *out) const;
  virtual MaoEntry::EntryType  Type() const;
  virtual char GetDescriptiveChar() const {return 'D';}

  // Returns if this is an entry in a jump table.
  bool IsJumpTableEntry() const;
  // Returns if this is a .cfi directive.
  bool IsCFIDirective() const;
  // Returns if this is an alignment directive.
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

  // op_ holds the type of directive.
  const Opcode op_;

  // operands_ hold the operands of the directive.
  OperandVector operands_;

  bool IsDebugDirective() const;

  bool IsInList(const Opcode opcode, const Opcode list[],
                const unsigned int number_of_elements) const;
};


// Class to represent an assembly instruction.
class InstructionEntry : public MaoEntry {
 public:
  static const unsigned int kMaxRegisterNameLength = MAX_REGISTER_NAME_LENGTH;

  InstructionEntry(i386_insn* instruction, enum flag_code flag_code,
                   unsigned int line_number, const char* line_verbatim,
                   MaoUnit *maounit);
  ~InstructionEntry();
  // Returns a string representation of this instruction entry.
  virtual std::string &ToString(std::string *out) const;
  // Prints this instruction  entry into FILE *out.
  virtual void PrintEntry(FILE *out = stderr) const;
  // Prints the internal representation of this instruction.
  virtual void PrintIR(FILE *out) const;
  virtual MaoEntry::EntryType  Type() const;

  // Returns the target label of this instruction. Returns "<UNKNOWN>" if the
  // instruction does not have a label operand.
  const char *GetTarget() const;
  virtual char GetDescriptiveChar() const {return 'I';}

  // Checks if this instruction has an opcode prefix.
  bool HasPrefix(unsigned char prefix) const;
  // Returns if the lock prefix has to be suppressed during printing (if the
  // lock is implicit).
  bool SuppressLockPrefix() const;

  // Returns a string representation of this instruction entry and store that
  // representation in out.
  std::string &InstructionToString(std::string *out) const;
  // Converts the profile information in this instruction to a string and
  // returns it in *out as well as the return value.
  // Returns a string representation of the profile information in this
  // instruction entry and store that representation in out.
  std::string &ProfileToString(std::string *out) const;

  // Returns the opcode of this instruction as a string.
  const char *op_str() const;
  // Returns the opcode of this instruction.
  MaoOpcode   op() const { return op_; }
  // Sets the opcode of this instruction.
  void        set_op(MaoOpcode op) { op_ = op; }

  // Property methods.
  //
  // Returns if this instruction has a target label.
  bool HasTarget() const;
  // Returns if this instruction has a fallthrough (another instruction that
  // follows it to which control can get transfered after this instruction).
  bool HasFallThrough() const;
  // Returns if this is a control transfer instruction.
  bool IsControlTransfer() const {
    return HasTarget() || IsCall() || IsReturn();
  }
  // Returns if this is an indirect jump instruction.
  bool IsIndirectJump() const;
  // Returns if this is a conditional jump instruction.
  bool IsCondJump() const;
  // Returns if this is a jump instruction.
  bool IsJump() const;
  // Returns if this is a call instruction.
  bool IsCall() const;
  // Returns if this is a 'thunk call' (one used to find the current IP).
  bool IsThunkCall() const;
  // Returns if this is a return instruction.
  bool IsReturn() const;
  // Returns if this is an add instruction.
  bool IsAdd() const;
  // Returns if this is a move instruction.
  bool IsOpMov() const { return op() == OP_mov || op() == OP_movq; }
  // Returns if this is a load which indexes/triggers the prefetcher(s)
  bool IsPrefetchLoad() const { return IsOpMov(); }
  // Returns if this is a lock instruction.
  bool IsLock() const { return op() == OP_lock; }
  // Returns if this is a predicated instruction (conditional moves).
  bool IsPredicated() const;

  // Returns the number of operands to this instruction.
  int NumOperands() const {
    return instruction()->operands;
  }
  // Returns if the operand with index op_index is a memory operand.
  bool IsMemOperand(const unsigned int op_index) const {
    return IsMemOperand(instruction(), op_index);
  }
  // Returns if the operand with index op_index is a memory operand accessing a
  // byte.
  bool IsMem8Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.disp8 ||
        instruction_->types[op_index].bitfield.unspecified;
  }
  // Returns if the operand with index op_index is a memory operand accessing a
  // word.
  bool IsMem16Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.disp16 ||
        instruction_->types[op_index].bitfield.unspecified;
  }
  // Returns if the operand with index op_index is a memory operand accessing a
  // double word.
  bool IsMem32Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.disp32 ||
        instruction_->types[op_index].bitfield.disp32s ||
        instruction_->types[op_index].bitfield.unspecified;
  }
  // Returns if the operand with index op_index is a memory operand accessing a
  // quad word.
  bool IsMem64Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.disp64 ||
        instruction_->types[op_index].bitfield.unspecified;
  }
  // Returns if the operand with index op_index is an immediate operand.
  bool IsImmediateOperand(const unsigned int op_index) const {
    return IsImmediateOperand(instruction(), op_index);
  }
  // Returns if the operand with index op_index is an immediate integer operand.
  bool IsImmediateIntOperand(const unsigned int op_index) const {
    return IsImmediateIntOperand(instruction(), op_index);
  }
  // Returns if the operand with index op_index is a register operand.
  bool IsRegisterOperand(const unsigned int op_index) const {
    return IsRegisterOperand(instruction(), op_index);
  }
  // Returns if the operand with index op_index is a register operand with a 8
  // byte register.
  bool IsRegister8Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.reg8 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.byte);
  }
  // Returns if the operand with index op_index is a register operand with a 16
  // byte register.
  bool IsRegister16Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.reg16 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.word);
  }
  // Returns if the operand with index op_index is a register operand with a 32
  // byte register.
  bool IsRegister32Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.reg32 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.dword);
  }
  // Returns if the operand with index op_index is a register operand with a 64
  // byte register.
  bool IsRegister64Operand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.reg64 ||
      (instruction_->types[op_index].bitfield.acc &&
       instruction_->types[op_index].bitfield.qword);
  }
  // Returns if the operand with index op_index is a register operand with a
  // floating point register.
  bool IsRegisterFloatOperand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.floatreg;
  }
  // Returns if the operand with index op_index is a register operand with a
  // xmm (SSE2) register.
  bool IsRegisterXMMOperand(const unsigned int op_index) const {
    return instruction_->types[op_index].bitfield.regxmm;
  }
  // Returns if this instruction is a string operation (movs, stos, etc).
  bool IsStringOperation() {
    return instruction_->tm.opcode_modifier.isstring;
  }

  // Returns if this instruction has a displacement field.
  bool HasDisplacement(const int op_index) const {
    MAO_ASSERT(op_index < NumOperands());
    return instruction_->op[op_index].disps != NULL;
  }

  // Returns the displacement field of this instruction.
  expressionS *GetDisplacement(const int op_index) const {
    MAO_ASSERT(HasDisplacement(op_index));
    return instruction_->op[op_index].disps;
  }
  // Returns if the op1 operand of this instruction and the op2 operand of
  // instruction i2 are both memory operands and they are equal.
  bool CompareMemOperand(int op1, InstructionEntry *i2, int op2) const;
  // Sets the op1 operand of this instruction to the op2 operand of instruction
  // i2.
  void SetOperand(int op1, InstructionEntry *i2, int op2);

  // Returns a pointer to the binutils i386_insn structure wrapped by this
  // instruction entry.
  i386_insn   *instruction() const { return instruction_; }

  // Returns the register name of the register operand at index op_index.
  const char  *GetRegisterOperandStr(const unsigned int op_index) const {
    return instruction_->op[op_index].regs->reg_name;
  }
  // Returns the register operand at index op_index.
  const reg_entry *GetRegisterOperand(const unsigned int op_index) const {
    return instruction_->op[op_index].regs;
  }
  // Returns the 'num'th relocation of this entry. The reloction index starts
  // from 0.
  const enum bfd_reloc_code_real GetReloc(int num) const {
    return instruction_->reloc[num];
  }

  // Returns if this instruction has base register.
  bool HasBaseRegister() const;
  // Returns if this instruction has index register.
  bool HasIndexRegister() const;

  // Returns the register name of the base register.
  const char  *GetBaseRegisterStr() const;
  // Returns the register name of the index register.
  const char  *GetIndexRegisterStr() const;
  // Returns the base register.
  const reg_entry *GetBaseRegister() const;
  // Returns the index register.
  const reg_entry *GetIndexRegister() const;

  // The scaled index addressing mode takes a scaling factor 's'. The index is
  // multiplied by 2^s before being added to the base. This method returns
  // 's' if this instruction uses the scaled index addressing mode.
  unsigned int GetLog2ScaleFactor();

  // Increments the execution count of this instruction by 'increment'.
  // Execution count is usually supplied by a profile annotation pass and is
  // equal (or proportional) to the number of times this instruction was
  // executed during a profile run.
  void IncrementExecutionCount(long increment) {
    if (!execution_count_valid_) {
      execution_count_valid_ = true;
      execution_count_ = 0;
    }
    execution_count_ += increment;
  }

  // Sets the execution count of this instruction to 'count'.
  void SetExecutionCount(long count) {
    execution_count_valid_ = true;
    execution_count_ = count;
  }

  // Returns if this instruction has a valid execution count.
  bool HasExecutionCount() {
    return execution_count_valid_;
  }

  // Returns the execution count of this instruction.
  long GetExecutionCount() {
    return execution_count_valid_ ? execution_count_ : -1;
  }

  // Returns the flag that indicates if this is a 64 bit, 32 bit or 16 bit code.
  enum flag_code GetFlag() const { return code_flag_; }

  // Returns 0 if attempting to add a prefix where one from the same
  // class already exists, 1 if non rep/repne added, 2 if rep/repne
  // added.
  int AddPrefix(unsigned int prefix);
  // Returns if the 'op_index'th operand of instruction is a memory operand.
  static bool IsMemOperand(const i386_insn *instruction,
                           const unsigned int op_index);
  // Returns if the 'op_index'th operand of instruction is an immediate
  // operand. Operands can be constants, or symbols defined via .comm
  static bool IsImmediateOperand(const i386_insn *instruction,
                                 const unsigned int op_index);

  // Returns if the 'op_index'th operand of instruction is an immediate
  // operand and an actual integer. The form above also allows symbols
  // declared with .comm
  static bool IsImmediateIntOperand(const i386_insn *instruction,
                                    const unsigned int op_index);
  // Returns if the 'op_index'th operand of instruction is a register operand.
  static bool  IsRegisterOperand(const i386_insn *instruction,
                                 const unsigned int op_index);

  // If operand is an immediate operand, return its integer value.
  offsetT GetImmediateIntValue(const unsigned int op_index);

  // Make an operand an Int Immediate operand
  void SetImmediateIntOperand(const unsigned int op_index,
                              int bit_size,
                              int value);

 private:
  i386_insn *instruction_;
  MaoOpcode  op_;

  // This flag states which code-mode the instruction is in. This is changed
  // in the assembly file using the .codeXX directives.
  // Values can be CODE_16BIT, CODE_32BIT, CODE_64BIT.
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
  // in the instruction.
  expressionS *CreateExpressionCopy(expressionS *in_exp);
  bool EqualExpressions(expressionS *expression1,
                        expressionS *expression2) const;



  reg_entry *CopyRegEntry(const reg_entry *in_reg);
  // Frees all allocated memory for this instruction.
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
// An iterator over list of entries. This class uses the prev/next of the
// entries to move.
class EntryIterator {
 public:
  explicit EntryIterator(MaoEntry *entry);
  // Overloads the * operator to return the current entry pointer.
  MaoEntry *operator *() {return current_entry_;}
  MaoEntry *operator ->(){return current_entry_;}
  // Overloads the ++ operator to move the iterator to the next entry.
  EntryIterator &operator ++();
  // Overloads the -- operator to move the iterator to the previous entry.
  EntryIterator &operator --();
  // Overloads the == operator to check if the current entry of this iterator is
  // the same as that of the other iterator.
  bool operator ==(const EntryIterator &other) const;
  // Overloads the != operator to check if the current entry of this iterator is
  // not the same as that of the other iterator.
  bool operator !=(const EntryIterator &other) const;
 private:
  MaoEntry *current_entry_;  // NULL is used for signalling the end.
};


// An iterator over list of entries that traverses in the reverse order. This
// class uses the prev/next of the entries to move.
class ReverseEntryIterator {
 public:
  explicit ReverseEntryIterator(MaoEntry *entry);
  // Overloads the * operator to return the current entry pointer.
  MaoEntry *operator *() {return current_entry_;}
  // Overloads the ++ operator to move the iterator to the next entry in the
  // reverse order.
  ReverseEntryIterator &operator ++();
  // Overloads the -- operator to move the iterator to the previous entry in the
  // reverse order.
  ReverseEntryIterator &operator --();
  // Overloads the == operator to check if the current entry of this iterator is
  // the same as that of the other iterator.
  bool operator ==(const ReverseEntryIterator &other) const;
  // Overloads the != operator to check if the current entry of this iterator is
  // not the same as that of the other iterator.
  bool operator !=(const ReverseEntryIterator &other) const;
 private:
  MaoEntry *current_entry_;  // NULL is used for signalling the end.
};


#endif // MAOENTRY_H_
