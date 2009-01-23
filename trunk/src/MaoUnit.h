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
#include <utility>

#include "ir-gas.h"
#include "irlink.h"
#include "SymbolTable.h"

#define DEFAULT_SECTION_NAME "text"
#define DEFAULT_SECTION_CREATION_OP "\t.text"

// Maps the label to a block, and the offset into this block
struct ltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};

typedef unsigned int entry_index_t;
typedef unsigned int subsection_index_t;
typedef unsigned int basicblock_index_t;
typedef std::map<const char *, std::pair<basicblock_index_t, unsigned int>,ltstr> labels2basicblock_map_t;

class BasicBlock;
class BasicBlockEdge;
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
  // The entry is added at the end of the list, unless an iterator to the list
  // is given.
  //  build_section - signals if the section building should be triggered.
  //                  this is true only when building the initial structures,
  //                  typically not when adding extra entries later one.
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
  void SetSubSection(const char *section_name, unsigned int subsection_number,
                     const char *creation_op);
  // Create the section section_name if it does not already exists. Returns a
  // pointer the section.
  Section *FindOrCreateAndFind(const char *section_name);

  void AddLabel(const char *label_name, entry_index_t index);

  void BuildCFG();

  void PrintMaoUnit() const;
  void PrintMaoUnit(FILE *out) const;
  void PrintIR() const;
  void PrintIR(FILE *out) const;
  void PrintCFG();
  void PrintCFG(FILE *out);
  void PrintBasicBlocks();
  void PrintBasicBlocks(FILE *out);
  void PrintBasicBlockEdges();
  void PrintBasicBlockEdges(FILE *out);

 private:

  // Used by the STL-maps of sections and subsections.

  // Vector of the entries in the unit. The id of the entry
  // is also the index into this vector.
  std::vector<MaoUnitEntryBase *> entry_vector_;
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

  // Basicblocks. The id of the basic block is also the index into the vector.
  std::vector<BasicBlock *> bb_vector_;
  std::list<BasicBlock *> bb_list_;

  // Edges between basic blocks
  std::vector<BasicBlockEdge *> bb_edges_;

  // Returns an entity given the ID.
  BasicBlock *GetBasicBlockByID(const basicblock_index_t id) const;
  MaoUnitEntryBase *GetEntryByID(basicblock_index_t id) const;

  // If the entry with the given id is a label, return the name, otherwise
  // return 0.
  const char *GetCurrentLabelIfAny(const entry_index_t index);


  // Go through all entries in a basic block, and add all found labels
  // and map them with the id of the basic block and the offset into the
  // basic block
  void UpdateLabel2BasicblockMap(labels2basicblock_map_t *labels2bb,
                                 BasicBlock *bb);

  // When splitting a basic block, some labels will change basic block
  // Here all labels that belongs to bb are removed from the map.
  void ClearOldLabel2BasicblockMap(labels2basicblock_map_t *labels2bb,
                                   BasicBlock *bb);

  // Create a new basic block edge and return it. Does not register it with any
  // other data structures.
  BasicBlockEdge *CreateBasicBlockEdge(const basicblock_index_t source_index,
                                       const basicblock_index_t target_index,
                                       const bool fallthrough);

  // Register the given edge to the source- and or the target-basicblock
  void RegisterBasicBlockEdge(BasicBlockEdge *bbe,
                              const bool register_source,
                              const bool register_target);

  // Change the source of the edge to a new one. Updates the edge and the
  // basic blocks.
  void MoveBasicBlockEdgeSource(BasicBlockEdge *bbe,
                           const basicblock_index_t uynew_basicblock_source_index);

  // Createa a new basic block and insert it into the basic block vector.
  // Return a pointer to the newly created block
  BasicBlock *CreateAndAddBasicblock(const char *label,
                                     std::list<MaoUnitEntryBase *>::iterator
                                     start_iterator,
                                     subsection_index_t end_index);

  // Creates a new basic block edge, inserts it into the list of all edges
  // and updates the effected basic blocks.
  BasicBlockEdge *AddBasicBlockEdge(basicblock_index_t source,
                                    basicblock_index_t target,
                                    bool fallthrough);
  // Looks up the basic block a given label is in. Returns id and offset into
  // basic block in the parameter. The function returns true if the label is
  // found, false otherwise.
  bool GetBasicblockFromLabel(const char * label,
                 const labels2basicblock_map_t *labels2bb,
                 std::pair<basicblock_index_t, unsigned int> &out_pair);

  // Split the given basic block at the given offset. Note that labels might
  // change basic block. Therefor the labels2bb map is also updated
  void SplitBasicBlock(basicblock_index_t basicblock_index, unsigned int offset,
                       std::map<const char *, std::pair<basicblock_index_t,
                                                        unsigned int>, ltstr>
                       &labels2bb);

  // Prints out the map.
  void PrintLabels2BasicBlock(labels2basicblock_map_t &labels2bb);
  void PrintLabels2BasicBlock(FILE *out,
                              labels2basicblock_map_t &labels2bb);

  // Simple class for generating unique names for mao-created labels.
  class BBNameGen {
   public:
    static const char *GetUniqueName();
   private:
    static int i;
  };
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
  bool HasFallthrough() const {return false;}
  entry_index_t index() const {return index_;}
  void set_index(entry_index_t index) {index_ = index;}

 protected:
  // Line number assembly was found in the original file
  unsigned int line_number_;
  // A verbatim copy of the assembly instruction this entry is
  // generated from. Might be NULL for some entries.
  const char *line_verbatim_;
  // Helper function to indent
  void Spaces(unsigned int n, FILE *outfile) const;

  entry_index_t index_;
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
  bool BelongsInBasicBlock() const {return true;}
  bool EndsBasicBlock() const {return false;}
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
  const char *key() const;
  const char *value() const;
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
  const char *GetOp() const;
  bool EndsBasicBlock() const;
  bool HasFallthrough() const;
  bool HasTarget() const;

  const char *GetTarget() const;
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
  bool IsInList(const char *string, const char *list[],
                const unsigned int number_of_elements) const;
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
  const char *GetOp() const  {return instruction_->GetOp();}
  const char *GetTarget() const  {return instruction_->GetTarget();}
  char GetDescriptiveChar() const {return 'I';}
  bool BelongsInBasicBlock() const {return true;}
  bool EndsBasicBlock() const {return instruction_->EndsBasicBlock();}
  bool HasFallthrough() const {return instruction_->HasFallthrough();}
  bool HasTarget() const {return instruction_->HasTarget();}
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

  entry_index_t first_entry_index() const { return first_entry_index_;}
  entry_index_t last_entry_index() const { return last_entry_index_;}
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
  std::vector<subsection_index_t> *GetSubSectionIndexes() {
    return &sub_section_indexes_;
  }
 private:
  char *name_;  // .text -> "text", .data -> "data"
  std::vector<subsection_index_t> sub_section_indexes_;
};

// Basic block
// A basic block stores an iterator to point out the first entry in
// the entry_list_ and the ID of the last entry.
// TODO(martint): Make it easier to loop over the entries in the basic block
//                using an iterator.
// A basic block also keeps to vectors (in_edges_ and out_edges_) with pointers
// to all the edges going in to and out from this basic block.
//
// Each basic block has one label as its first entry. This name is also set
// in the label_ member.
class BasicBlock {
 public:
  explicit BasicBlock(const char *label);
  ~BasicBlock() {}

  std::list<MaoUnitEntryBase *>::iterator first_iter() const {
    return first_iter_;
  }
  void set_first_iter(std::list<MaoUnitEntryBase *>::iterator iter) {
    first_iter_ = iter;
  }

  entry_index_t last_entry_index() const { return last_entry_index_;}
  void set_last_entry_index(entry_index_t index) {last_entry_index_ = index;}

  void AddInEdge(BasicBlockEdge *edge);
  void RemoveInEdge(BasicBlockEdge *edge);
  void AddOutEdge(BasicBlockEdge *edge);
  void RemoveOutEdge(BasicBlockEdge *edge);

  // Index is the ID of the basic block.
  basicblock_index_t index() const {return index_;}
  void set_index(basicblock_index_t index) {index_ = index;}
  bool source() const {return source_;}
  void set_source(bool value) {source_ = value;}
  bool sink() const {return sink_;}
  void set_sink(bool value) {sink_ = value;}

  unsigned int NumberOfEntries() const;

  const char *label() const;
  const std::vector<BasicBlockEdge *> &GetInEdges() const { return in_edges_;}
  const std::vector<BasicBlockEdge *> &GetOutEdges() const { return out_edges_;}

  void PrintInfo() const;
  void PrintInfo(FILE *out) const;

  int GetNumPred() { return in_edges_.size(); }
  int GetNumSucc() { return out_edges_.size(); }

 private:

  // Identifies to the first and last entry in the basic block
  std::list<MaoUnitEntryBase *>::iterator first_iter_;
  entry_index_t last_entry_index_;

  // Pointers to in/out-edges to/from this basic block.
  std::vector<BasicBlockEdge *> in_edges_;
  std::vector<BasicBlockEdge *> out_edges_;

  // TODO(martint): more efficient representation using bits
  bool source_;
  bool sink_;

  // The ID of the basic block
  basicblock_index_t index_;
  // The label-name for this basic block
  const char *label_;
};

// A basic block edge.
// The source and the target are represented using basic block indexes.
class BasicBlockEdge {
 public:
  BasicBlockEdge() {}
  ~BasicBlockEdge() {}
  basicblock_index_t source_index() const { return source_index_;}
  basicblock_index_t target_index() const { return target_index_;}
  void set_source_index(const basicblock_index_t index) {source_index_ = index;}
  void set_target_index(const basicblock_index_t index) {target_index_ = index;}
  bool fallthrough() const {return fallthrough_;}
  void set_fallthrough(bool value) { fallthrough_ = value;}

 private:
  basicblock_index_t source_index_;
  basicblock_index_t target_index_;

  bool fallthrough_;
};

#endif  // MAOUNIT_H_
