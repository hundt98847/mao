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
// along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

// Control Flow Graph - Calculates the control flow graph for a function.
//
// Classes:
//   BasicBlockEdge - Represent edges in the CFG
//   BasicBlock     - Basic blocks are the nodes in the CFG.
//   CFG            - Holds a Control Flow Graph for one function.
//   CFGBuilder     - Used for building a CFG from a function.
//
// To get the CFG for a function, use the static method so that
// you benefit from the caching of the CFGs. Also, make sure
// to invalidate the CFG if the IR is updated.
//  // To get the CFG for a function
//  CFG *cfg = CFG::GetCFG(unit_, function_);
//
//  // Iterate over all instructions in all basic blocks
//  FORALL_CFG_BB(cfg,it) {
//    MaoEntry *first = (*it)->GetFirstInstruction();
//    if (!first) continue;
//
//    FORALL_BB_ENTRY(it,entry) {
//      if (!(*entry)->IsInstruction()) continue;
//      InstructionEntry *insn = (*entry)->AsInstruction();
//      // TODO: Process instruction here
//    }
//  }
//
//  // Invalidate the CFG
//  CFG::InvalidateCFG(function_);

#ifndef MAOCFG_H_
#define MAOCFG_H_

#include <stdio.h>
#include <list>
#include <map>
#include <set>
#include <vector>

#include "MaoDebug.h"
#include "MaoPasses.h"
#include "MaoUtil.h"
#include "MaoStats.h"

class MaoEntry;
class InstructionEntry;
class EntryIterator;
class ReverseEntryIterator;
class BasicBlock;
class LabelEntry;

//
// CFG - Control Flow Graph
//

typedef int BasicBlockID;

// A basic block edge is a possible path between two basic blocks.
class BasicBlockEdge {
 public:
  // Creates an edge between source and destination. fall_through
  // means that the edge is not created by an explicit control
  // transfer instruction.
  BasicBlockEdge(BasicBlock *source, BasicBlock *dest, bool fall_through)
      : source_(source), dest_(dest), fall_through_(fall_through) { }

  bool fall_through() { return fall_through_; }

  // Accessors for source and destination.
  BasicBlock *source() { return source_; }
  void set_source(BasicBlock *source) { source_ = source; }
  BasicBlock *dest() { return dest_; }
  void set_dest(BasicBlock *dest) { dest_ = dest; }

 private:
  BasicBlock *source_;
  BasicBlock *dest_;
  const bool fall_through_;
};


// A basic block holds a series of entries which have one entry point
// (the first entry), and one exit point (last entry).
class BasicBlock {
 public:
  typedef std::vector<BasicBlockEdge *> EdgeList;
  typedef EdgeList::iterator EdgeIterator;
  typedef EdgeList::const_iterator ConstEdgeIterator;

  // Creates a basic block with the name 'label', which should be the
  // first label of the basic block. Id and label must be unique within
  // the CFG.
  BasicBlock(BasicBlockID id, const char *label)
  : id_(id), label_(label),
    first_entry_(NULL),
    last_entry_(NULL),
    chained_indirect_jump_target_(false),
    has_data_directives_(false) {
  }
  ~BasicBlock() {
    for (EdgeIterator iter = out_edges_.begin();
         iter != out_edges_.end(); ++iter) {
      delete *iter;
    }
  }

  // Returns the unique ID.
  BasicBlockID id() const { return id_; }
  // Returns the label associated with the basic block.
  const char *label() const { return label_; }

  // Edge methods

  // Iterators
  EdgeIterator      BeginInEdges ()       { return in_edges_.begin(); }
  ConstEdgeIterator BeginInEdges () const { return in_edges_.begin(); }
  EdgeIterator      EndInEdges   ()       { return in_edges_.end(); }
  ConstEdgeIterator EndInEdges   () const { return in_edges_.end(); }
  EdgeIterator      BeginOutEdges()       { return out_edges_.begin(); }
  ConstEdgeIterator BeginOutEdges() const { return out_edges_.begin(); }
  EdgeIterator      EndOutEdges  ()       { return out_edges_.end(); }
  ConstEdgeIterator EndOutEdges  () const { return out_edges_.end(); }

  // Methods for adding and removing edges.
  void AddInEdge(BasicBlockEdge *edge) {
    MAO_ASSERT(edge->dest() == this);
    in_edges_.push_back(edge);
  }

  void AddOutEdge(BasicBlockEdge *edge) {
    MAO_ASSERT(edge->source() == this);
    out_edges_.push_back(edge);
  }

  EdgeIterator EraseInEdge(EdgeIterator pos) {
    return in_edges_.erase(pos);
  }

  EdgeIterator EraseOutEdge(EdgeIterator pos) {
    return out_edges_.erase(pos);
  }

  // Entry iterators.
  EntryIterator EntryBegin() const;
  EntryIterator EntryEnd() const;
  ReverseEntryIterator RevEntryBegin() const;
  ReverseEntryIterator RevEntryEnd() const;

  // Adds an entry to the basic block. Basic blocks are stored using a
  // start and an end pointer, so it is sufficient to add only the
  // first and last entry.
  void AddEntry(MaoEntry *entry);

  // Is this basic block located directly before basicblock in the section.
  bool DirectlyPreceeds(const BasicBlock *basicblock) const;

  // Is this basic block located directly after basicblock in the section.
  bool DirectlyFollows(const BasicBlock *basicblock) const;

  // Caution: This routine iterates through a bb to count entries
  int       NumEntries();

  // Find and last instruction. Returns NULL if there is no
  // instruction in the basic block.
  InstructionEntry *GetFirstInstruction() const;
  InstructionEntry *GetLastInstruction() const;

  // Dump BasicBlock until/including an entry.
  // If NULL, dump the whole basic block
  void Print(FILE *f, MaoEntry *last = NULL);
  void Print(FILE *f, MaoEntry *from, MaoEntry *to);

  // Accessors for the first and last entry in the basic block.
  MaoEntry *first_entry() const { return first_entry_;}
  MaoEntry *last_entry() const { return last_entry_;}
  void set_first_entry(MaoEntry *entry)  { first_entry_ = entry;}
  void set_last_entry(MaoEntry *entry)  { last_entry_ = entry;}

  // GCC relies on the fact that some basic blocks are stored after
  // each-other when generating code for methods with variable number
  // of arguments. For such basic blocks, chained_indirect_jump_target()
  // is set to true. It is not safe to move these blocks unless the
  // corresponding jumps are updated.
  void set_chained_indirect_jump_target(bool value) {
    chained_indirect_jump_target_ = value;
  }

  bool chained_indirect_jump_target() const {
    return chained_indirect_jump_target_;
  }

  // Basic block contains at least one .data directive.
  void FoundDataDirectives() {
    has_data_directives_ = true;
  }

  bool HasDataDirectives() {
    return has_data_directives_;
  }

 private:
  const BasicBlockID id_;
  const char *label_;

  EdgeList in_edges_;
  EdgeList out_edges_;

  // Pointers to the first and last entry of the basic block.
  MaoEntry *first_entry_;
  MaoEntry *last_entry_;

  bool chained_indirect_jump_target_;

  bool has_data_directives_;
};

// Convenience Macros for Entry iteration
#define FORALL_BB_ENTRY(it, entry) \
      for (EntryIterator entry = (*it)->EntryBegin(); \
           entry != (*it)->EntryEnd(); ++entry)


// Control Flow Graph
// Use CFG::GetCFG() to get the CFG for a function. The CFG is cached,
// and using this method avoids unnecessary regenerations. Invalidate
// the CFG using CFG::InvalidateCFG() if necessary.
//
// All CFGs also have two special blocks called "<SOURCE>" and
// "<SINK>". The source is always the first basic block of the CFG and
// all exit points lead to the sink.
//
class CFG {
 public:
  typedef std::vector<BasicBlock *> BBVector;
  typedef std::map<const char *, BasicBlock *, ltstr> LabelToBBMap;
  explicit CFG(MaoUnit *mao_unit) : mao_unit_(mao_unit),
                                    num_external_jumps_(0),
                                    num_unresolved_indirect_jumps_(0) {
    labels_to_jumptargets_.clear();
  }
  ~CFG() {
    for (BBVector::iterator iter = basic_blocks_.begin();
         iter != basic_blocks_.end(); ++iter) {
      delete *iter;
    }
    for (LabelsToJumpTableTargets::iterator iter =
             labels_to_jumptargets_.begin();
         iter != labels_to_jumptargets_.end(); ++iter) {
      delete iter->second;
    }
  }

  // Gets the CFG for the given function and builds if it not
  // cached.  A conservative CFG treats all labels as the start of a
  // new basic block.
  static CFG *GetCFG(MaoUnit *mao, Function *function,
                     bool conservative = false);
  // Returns a cached CFG if it exists in our cache, otherwise NULL.
  static CFG *GetCFGIfExists(const MaoUnit *mao, Function *function);
  // Invalidates the CFG in the cache.
  static void InvalidateCFG(Function *function);
  // Returns the number of nodes in the CFG. Each node represent one
  // basic block.
  int  GetNumOfNodes() const { return basic_blocks_.size(); }
  // Prints a graphical representation of the CFG as a VCG graph to a file.
  void DumpVCG(const char *fname) const;
  // Prints a textual representation of the CFG.
  void Print() const { Print(stdout); }
  void Print(FILE *out) const;

  // Adds a basic block to the CFG.
  void AddBasicBlock(BasicBlock *bb) { basic_blocks_.push_back(bb); }
  // Puts the basic block in the map from label to basic block.
  // TODO(martint): Should be done automatically when creating a basic block.
  void MapBasicBlock(BasicBlock *bb) {
    MAO_RASSERT(basic_block_map_.insert(std::make_pair(bb->label(),
                                                       bb)).second);
  }

  // Returns the basic block of a given id. Assumes that an basic block with
  // the id exists.
  BasicBlock *GetBasicBlock(BasicBlockID id) { return basic_blocks_[id]; }
  // Finds a basic block for a given label, or returns NULL if one has not
  // already exists.
  BasicBlock *FindBasicBlock(const char *label) {
    LabelToBBMap::iterator bb = basic_block_map_.find(label);
    if (bb == basic_block_map_.end())
      return NULL;
    return bb->second;
  }

  // Iterators for the basic blocks.
  BBVector::iterator Begin() { return basic_blocks_.begin(); }
  BBVector::const_iterator Begin() const { return basic_blocks_.begin(); }

  BBVector::iterator End() { return basic_blocks_.end(); }
  BBVector::const_iterator End() const { return basic_blocks_.end(); }

  BBVector::reverse_iterator RevBegin() { return basic_blocks_.rbegin(); }
  BBVector::const_reverse_iterator RevConstBegin() {
    return basic_blocks_.rbegin();
  }
  BBVector::reverse_iterator RevEnd() { return basic_blocks_.rend(); }
  BBVector::const_reverse_iterator RevConstEnd() {
    return basic_blocks_.rend();
  }

  // Returns the source node in the CFG
  BasicBlock *Source() const { return basic_blocks_[0]; }
  // Returns the sink node in the CFG
  BasicBlock *Sink() const { return basic_blocks_[1]; }

  // A jump-table is stored as a set of target-labels.
  typedef std::set<LabelEntry *> JumpTableTargets;
  // Maps labels to the jump-table targets stored at that label.
  typedef std::map<LabelEntry *, JumpTableTargets *> LabelsToJumpTableTargets;

  // Given a label, returns all the targets found in the corresponding
  // jump table. The function returns True if the jump_table can be
  // successfully parsed. The results are stored in out_targets.
  bool GetJumptableTargets(LabelEntry *jump_table,
                           CFG::JumpTableTargets **out_targets);


  // Unresolved indirect jumps - Indirect jumps in the function where the
  //                             possible destinations are unknown.
  // External jumps            - Jump to labels not found in the current
  //                             function.

  // True if function has unresolved jumps.
  bool HasUnresolvedIndirectJump() const {
    return num_unresolved_indirect_jumps_ > 0;
  }

  // Returns True if function contains jumps to labels not found in
  // the current function.
  bool HasExternalJump() const {
    return num_external_jumps_ > 0;
  }

  // Returnes True if all jumps are resolved, and all targets are
  // labels within the function.
  bool IsWellFormed() const {
    return !HasExternalJump() && !HasUnresolvedIndirectJump();
  }

  // Updates the number of found external jumps
  void IncreaseNumExternalJumps() {
    ++num_external_jumps_;
  }

  // Updates the number of found unresolved indirect jumps.
  void IncreaseNumUnresolvedJumps() {
    ++num_unresolved_indirect_jumps_;
  }

  // Was the CFG built with the conservative flag.
  // In a conservative CFG, each label starts a new basic block.
  bool conservative() const {
    return conservative_;
  }

  void set_conservative(bool value) {
    conservative_ = value;
  }

 private:
  MaoUnit *mao_unit_;
  LabelToBBMap basic_block_map_;
  BBVector basic_blocks_;

  int num_external_jumps_;  // Number of branches that have labels not defined
                            // in the current function.

  int num_unresolved_indirect_jumps_;  // Number of indirect jumps that
                                       // are unresolved

  // This holds the jump-tables we so far have identified, and maps the
  // label that identifies them to the list of targets in the table.
  // This is populated on demand. When we reach an indirect jump, we
  // find which label is used to identify the jump table (possibly by
  // moving back from the register used). Once the label is found,
  // the table is parsed and the results cached here.
  LabelsToJumpTableTargets labels_to_jumptargets_;

  bool conservative_;  // CFG build with conservative flag.
};

// Convenience Macros for BB iteration
#define FORALL_CFG_BB(cfg, it) \
    for (CFG::BBVector::const_iterator it = cfg->Begin(); \
           it != cfg->End(); ++it)

#define FORALL_REV_CFG_BB(cfg, it) \
    for (CFG::BBVector::const_reverse_iterator it = cfg->RevBegin(); \
           it != cfg->RevEnd(); ++it)

// Class used when building a CFG.
class CFGBuilder : public MaoFunctionPass {
 public:
  CFGBuilder(MaoUnit *mao_unit, Function *function, CFG *CFG,
             bool conservative = false);
  bool Go();

 private:
  BasicBlock *CreateBasicBlock(const char *label) {
    BasicBlock *bb = new BasicBlock(next_id_, label);
    CFG_->AddBasicBlock(bb);
    next_id_++;
    return bb;
  }

  static bool BelongsInBasicBlock(const MaoEntry *entry);
  bool EndsBasicBlock(MaoEntry *entry);

  static BasicBlockEdge *Link(BasicBlock *source, BasicBlock *dest,
                              bool fallthrough) {
    BasicBlockEdge *edge = new BasicBlockEdge(source, dest, fallthrough);
    source->AddOutEdge(edge);
    dest->AddInEdge(edge);
    return edge;
  }

  BasicBlock *BreakUpBBAtLabel(BasicBlock *bb, LabelEntry *label);

  template <class OutputIterator>
  void GetTargets(MaoEntry *entry, OutputIterator iter, bool *va_arg_targets);

  bool IsTablePattern1(InstructionEntry *entry,
                       LabelEntry **out_label) const;
  bool IsTablePattern2(InstructionEntry *entry,
                       LabelEntry **out_label) const;
  bool IsTablePattern3(InstructionEntry *entry,
                       LabelEntry **out_label) const;
  bool IsTablePattern4(InstructionEntry *entry,
                       LabelEntry **out_label) const;
  bool IsTableBasedJump(InstructionEntry *entry,
                        LabelEntry **out_label) const;
  bool IsVaargBasedJump(InstructionEntry *entry,
                        std::vector<MaoEntry *> *pattern) const;
  bool IsTailCall(InstructionEntry *entry) const;

  CFG      *CFG_;
  BasicBlockID  next_id_;
  CFG::LabelToBBMap label_to_bb_map_;
  bool      split_basic_blocks_ : 1;
  bool      respect_orig_labels_ : 1;
  bool      dump_vcg_ : 1;

  // Class for holding statistics about a CFG. The state is kept
  // between CFG objects, and the stats is presented at the end of the
  // MAO run. Only used when the stat option is given.
  class CFGStat : public Stat {
   public:
    CFGStat() : number_of_direct_jumps_(0), number_of_indirect_jumps_(0),
                number_of_jump_table_patterns_(0), number_of_vaarg_patterns_(0),
                number_of_tail_calls_(0), number_of_unresolved_jumps_(0)
    {;}
    ~CFGStat() {;}
    void FoundDirectJump()        { ++number_of_direct_jumps_; }
    void FoundIndirectJump()      { ++number_of_indirect_jumps_; }
    void FoundJumpTablePattern()  { ++number_of_jump_table_patterns_; }
    void FoundVaargPattern()      { ++number_of_vaarg_patterns_; }
    void FoundTailCall()          { ++number_of_tail_calls_; }
    void FoundUnresolvedJump()    { ++number_of_unresolved_jumps_; }

    virtual void Print(FILE *out);

   private:
    int number_of_direct_jumps_;
    int number_of_indirect_jumps_;
    int number_of_jump_table_patterns_;
    int number_of_vaarg_patterns_;
    int number_of_tail_calls_;
    int number_of_unresolved_jumps_;
  };

  CFGStat *cfg_stat_;
};

void CreateCFG(MaoUnit *mao_unit, Function *function, CFG *cfg,
               bool conservative = false);

#endif  // MAOCFG_H_
