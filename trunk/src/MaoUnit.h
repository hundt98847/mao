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

#include "./gen-opcodes.h"
#include "./ir-gas.h"
#include "./irlink.h"
#include "./MaoOptions.h"
#include "./MaoUtil.h"
#include "./SymbolTable.h"

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

class MaoUnit {
 public:
  struct ltstr {
    bool operator()(const char* s1, const char* s2) const {
      return strcmp(s1, s2) < 0;
    }
  };
  typedef std::vector<MaoEntry *>      EntryVector;
  typedef EntryVector::iterator        EntryIterator;
  typedef EntryVector::const_iterator  ConstEntryIterator;

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
//   bool AddEntry(MaoEntry *entry, bool build_sections,
//                 bool create_default_section,
//                 std::list<MaoEntry *>::iterator list_iter);
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

  void PrintMaoUnit() const;
  void PrintMaoUnit(FILE *out) const;
  void PrintIR(bool print_entries, bool print_sections,
               bool print_subsections) const;
  void PrintIR(FILE *out, bool print_entries, bool print_sections,
               bool print_subsections) const;


  // Returns the section matching the given name. Returns
  // NULL if no match is found.
  Section * GetSection(const std::string &section_name) const;

  // Simple class for generating unique names for mao-created labels.
  class BBNameGen {
   public:
    static const char *GetUniqueName();
   private:
    static int i;
  };

  MaoOptions *mao_options() { return mao_options_; }

  // Iterate over the sections
  SectionIterator SectionBegin();
  SectionIterator SectionEnd();
  ConstSectionIterator ConstSectionBegin() const;
  ConstSectionIterator ConstSectionEnd() const;

 private:
  // Create the section section_name if it does not already exists. Returns a
  // pointer the section.
  std::pair<bool, Section *> FindOrCreateAndFind(const char *section_name);

  // Vector of the entries in the unit. The id of the entry
  // is also the index into this vector.
  // TODO(martint): fix the code that handles prev/next pointers.
  EntryVector entry_vector_;

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

  // Maps label-names to the corresponding label entry.
  std::map<const char *, LabelEntry *, ltstr> labels_;


  // Maps an entry to the corresponding Function and SubSection.
  std::map<MaoEntry *, Function *>   entry_to_function_;
  std::map<MaoEntry *, SubSection *> entry_to_subsection_;

  MaoOptions *mao_options_;
};

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




// Base class for all types of entries in the MaoUnit. Example of entries
// are Labels, Directives, and Instructions
class MaoEntry {
 public:
  // Types of possible entries
  enum EntryType {
    UNDEFINED = 0,
    INSTRUCTION,
    LABEL,
    DIRECTIVE,
    DEBUG,
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

  // Property methods
  virtual bool HasFallThrough() const = 0;
  virtual bool IsControlTransfer() const = 0;
  virtual bool IsCall() const = 0;
  virtual bool IsReturn() const = 0;

  void set_next(MaoEntry *entry) {next_ = entry;}
  void set_prev(MaoEntry *entry) {prev_ = entry;}

  MaoEntry *next() {return next_;}
  MaoEntry *prev() {return prev_;}

  unsigned int line_number() const { return line_number_; }
  const char *const line_verbatim() const { return line_verbatim_; }

 protected:
  // Helper function to indent
  void Spaces(unsigned int n, FILE *outfile) const;

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

  const MaoUnit *maounit_;
};


// An Entry of the type Label
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

  // Property methods
  virtual bool HasFallThrough() const { return true; }
  virtual bool IsControlTransfer() const { return false; }
  virtual bool IsCall() const { return false; }
  virtual bool IsReturn() const { return false; }

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

  Opcode op() { return op_; }
  const char *GetOpcodeName() const {
    return kOpcodeNames[op_];
  }

  int NumOperands() { return operands_.size(); }
  const Operand *GetOperand(int num) { return operands_[num]; }

  virtual void PrintEntry(::FILE *out) const;
  virtual void PrintIR(::FILE *out) const;
  virtual MaoEntry::EntryType  Type() const;
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

  const char *GetDotOrSymbol(symbolS *symbol) const;


  // op_ holds the type of directive
  const Opcode op_;

  // operands_ hold the operands of the directive
  OperandVector operands_;
};

// An Entry of the type Debug
// Used for debug directives.
class DebugEntry : public MaoEntry {
 public:
  DebugEntry(const char *key, const char* value, unsigned int line_number,
             const char* line_verbatim, const MaoUnit *maounit)
      : MaoEntry(line_number, line_verbatim, maounit),
        key_(key), value_(value) { }
  virtual void PrintEntry(FILE *out) const;
  virtual void PrintIR(FILE *out) const;
  virtual MaoEntry::EntryType  Type() const;
  virtual char GetDescriptiveChar() const {return 'g';}

  // Property methods
  virtual bool HasFallThrough() const { return false; }
  virtual bool IsControlTransfer() const { return false; }
  virtual bool IsCall() const { return false; }
  virtual bool IsReturn() const { return false; }

 private:
  // key_ holds the name of the directive.
  const std::string key_;
  // value holds the arguments to the directive.
  const std::string value_;
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
  const char *GetOp() const;
  const char *GetTarget() const;
  virtual char GetDescriptiveChar() const {return 'I';}

  void PrintInstruction(FILE *out) const;
  static const unsigned int kMaxRegisterNameLength = MAX_REGISTER_NAME_LENGTH;

  MaoOpcode   op() const { return op_; }
  bool HasTarget() const;

  // Property methods.
  virtual bool HasFallThrough() const;
  virtual bool IsControlTransfer() const {
    return HasTarget() || IsCall() || IsReturn();
  }
  virtual bool IsCall() const;
  virtual bool IsReturn() const;

  i386_insn *instruction() { return instruction_; }
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
                      const char *name)
      : number_(subsection_number),
        name_(name),
        id_(id),
        first_entry_(NULL),
        last_entry_(NULL),
        start_section_(false) { }

  unsigned int number() const { return number_; }
  const std::string &name() const { return name_; }

  MaoEntry *first_entry() const { return first_entry_;}
  MaoEntry *last_entry() const { return last_entry_;}
  void set_first_entry(MaoEntry *entry) { first_entry_ = entry;}
  void set_last_entry(MaoEntry *entry);
  SubSectionID id() const { return id_;}

  void set_start_section(bool value) {start_section_ = value;}
  bool start_section() const {return start_section_;}

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
};

// Function class
// TODO(martint): Complete this class
class Function {
 public:
  explicit Function(const std::string &name, const FunctionID id) :
      name_(name), id_(id) {}
 private:
  // Name of the function, as given by the function symbol.
  const std::string name_;

  // Pointers to the first and last entry of the function.
  MaoEntry *first_entry_;
  MaoEntry *last_entry_;

  // The uniq id of this function.
  const FunctionID id_;

  // Pointer to subsection that this function starts in.
  SubSection *subsection_;
};


// A section
class Section {
 public:
  // Memory for the name is allocated in the constructor and freed
  // in the destructor.
  explicit Section(const char *name, const SectionID id) :
      name_(name), id_(id) {}
  ~Section() {}
  std::string name() const {return name_;}
  SectionID id() const {return id_;}

  void AddSubSection(SubSection *subsection);

  SectionEntryIterator EntryBegin();
  SectionEntryIterator EntryEnd();

  std::vector<SubSectionID> GetSubsectionIDs() const;

  // Return the last subsection in the section or NULL if section is empty.
  SubSection *GetLastSubSection() const;

 private:

  const std::string name_;  // e.g. ".text", ".data", etc.
  const SectionID id_;

  std::vector<SubSection *> subsections_;
};


#endif  // MAOUNIT_H_
