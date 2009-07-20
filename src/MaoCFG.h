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
class SectionEntryIterator;
class BasicBlock;
class LabelEntry;

//
// CFG - Control Flow Graph
//

typedef int BasicBlockID;

class BasicBlockEdge {
 public:
  BasicBlockEdge(BasicBlock *source, BasicBlock *dest, bool fall_through)
      : source_(source), dest_(dest), fall_through_(fall_through) { }

  bool fall_through() { return fall_through_; }
  BasicBlock *source() { return source_; }
  void set_source(BasicBlock *source) { source_ = source; }

  BasicBlock *dest() { return dest_; }
  void set_dest(BasicBlock *dest) { dest_ = dest; }

 private:
  BasicBlock *source_;
  BasicBlock *dest_;
  const bool fall_through_;
};


class BasicBlock {
 public:
  // Iterators
  //
  typedef std::vector<BasicBlockEdge *> EdgeList;
  typedef EdgeList::iterator EdgeIterator;
  typedef EdgeList::const_iterator ConstEdgeIterator;

  BasicBlock(BasicBlockID id, const char *label)
  : id_(id), label_(label),
    first_entry_(NULL),
    last_entry_(NULL),
    chained_indirect_jump_target_(false) {
  }
  ~BasicBlock() {
    for (EdgeIterator iter = out_edges_.begin();
         iter != out_edges_.end(); ++iter) {
      delete *iter;
    }
  }

  // Getters
  //
  BasicBlockID id() const { return id_; }
  const char *label() const { return label_; }

  // Edge methods
  //
  EdgeIterator      BeginInEdges ()       { return in_edges_.begin(); }
  ConstEdgeIterator BeginInEdges () const { return in_edges_.begin(); }
  EdgeIterator      EndInEdges   ()       { return in_edges_.end(); }
  ConstEdgeIterator EndInEdges   () const { return in_edges_.end(); }
  EdgeIterator      BeginOutEdges()       { return out_edges_.begin(); }
  ConstEdgeIterator BeginOutEdges() const { return out_edges_.begin(); }
  EdgeIterator      EndOutEdges  ()       { return out_edges_.end(); }
  ConstEdgeIterator EndOutEdges  () const { return out_edges_.end(); }

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

  // Entry methods
  SectionEntryIterator EntryBegin();
  SectionEntryIterator EntryEnd();
  void AddEntry(MaoEntry *entry);

  // Is this basic block located directly before basicblock in the section.
  bool DirectlyPreceeds(const BasicBlock *basicblock) const;

  // Is this basic block located directly after basicblock in the section.
  bool DirectlyFollows(const BasicBlock *basicblock) const;

  // Caution: This routine iterates through a bb to count entries
  int       NumEntries();

  // Find 1st, last instruction, can be NULL
  InstructionEntry *GetFirstInstruction() const;
  InstructionEntry *GetLastInstruction() const;

  // Dump BasicBlock until/including an entry.
  // If NULL, dump the whole basic block
  void Print(FILE *f, MaoEntry *last = NULL);
  void Print(FILE *f, MaoEntry *from, MaoEntry *to);

  MaoEntry *first_entry() const { return first_entry_;}
  MaoEntry *last_entry() const { return last_entry_;}
  void set_first_entry(MaoEntry *entry)  { first_entry_ = entry;}
  void set_last_entry(MaoEntry *entry)  { last_entry_ = entry;}

  void set_chained_indirect_jump_target(bool value) {
    chained_indirect_jump_target_ = value;
  }

  bool chained_indirect_jump_target() const {
    return chained_indirect_jump_target_;
  }

 private:
  const BasicBlockID id_;
  const char *label_;

  EdgeList in_edges_;
  EdgeList out_edges_;

  // Pointers to the first and last entry of the basic block.
  MaoEntry *first_entry_;
  MaoEntry *last_entry_;

  // Some indirect jumps relies on that some basic blocks
  // are placed after eachother. These blocks may not be
  // changed and reordered without changing the corresponding
  // jumps. This flags shows that the current basic block
  // is such a basic block.
  bool chained_indirect_jump_target_;
};

// Convenience Macros for Entry iteration
#define FORALL_BB_ENTRY(it, entry) \
      for (SectionEntryIterator entry = (*it)->EntryBegin(); \
           entry != (*it)->EntryEnd(); ++entry)


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

  // Get the CFG for the given function, build
  // if it necessary.
  static CFG *GetCFG(MaoUnit *mao, Function *function);
  static CFG *GetCFGIfExists(const MaoUnit *mao, Function *function);
  static void InvalidateCFG(Function *function);

  int  GetNumOfNodes() const { return basic_blocks_.size(); }
  void DumpVCG(const char *fname) const;
  void Print() const { Print(stdout); }
  void Print(FILE *out) const;

  // Construction methods
  void AddBasicBlock(BasicBlock *bb) { basic_blocks_.push_back(bb); }
  void MapBasicBlock(BasicBlock *bb) {
    MAO_ASSERT(basic_block_map_.insert(std::make_pair(bb->label(), bb)).second);
  }

  // Getter methods
  BasicBlock *GetBasicBlock(BasicBlockID id) { return basic_blocks_[id]; }
  BasicBlock *FindBasicBlock(const char *label) {
    LabelToBBMap::iterator bb = basic_block_map_.find(label);
    if (bb == basic_block_map_.end())
      return NULL;
    return bb->second;
  }
  BBVector::iterator Begin() { return basic_blocks_.begin(); }
  BBVector::const_iterator Begin() const { return basic_blocks_.begin(); }

  BBVector::iterator End() { return basic_blocks_.end(); }
  BBVector::const_iterator End() const { return basic_blocks_.end(); }

  BasicBlock *Start() const { return basic_blocks_[0]; }
  BasicBlock *Sink() const { return basic_blocks_[1]; }

  // Lists the targets found in a jump table.
  typedef std::set<LabelEntry *> JumpTableTargets;
  typedef std::map<LabelEntry *, JumpTableTargets *> LabelsToJumpTableTargets;

  // Given a label, return all the targets found in the corresponing
  // jump table. The function return True all entries could be understood,
  // and False otherwise. The results are stored in out_targets.
  bool GetJumptableTargets(LabelEntry *jump_table,
                           CFG::JumpTableTargets **out_targets);


  // Unresolved indirect jumps - Indirect jumps in the function where the
  //                             possible destinations are unknown.
  // External jumps            - Jump to labels not found in the current
  //                             function.

  // True if function has unresolved jumps
  bool HasUnresolvedIndirectJump() const {
    return false;
  }

  // True if functuion has external jumps
  bool HasExternalJump() const {
    return num_external_jumps_ > 0;
  }

  // True if all jumps are resolved, and to labels within the function.
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

 private:
  MaoUnit *mao_unit_;
  LabelToBBMap basic_block_map_;
  BBVector basic_blocks_;

  int num_external_jumps_;  // Number of branches that have labels not defined
                            // in the current function.

  int num_unresolved_indirect_jumps_;  // Number of indirect jumps that
                                       // are unresolved

  // This holds the jumptables we so far have identified, and maps the
  // label that identifies them to the list of targets in the table.
  // This is populated on demand. When we reach an indirect jump, we
  // find which label is used to identify the jump table (possibly by
  // moving back from the register used). Once the label is found,
  // the table is parsed and the results cached here.
  LabelsToJumpTableTargets labels_to_jumptargets_;
};

// Convenience Macros for BB iteration
#define FORALL_CFG_BB(cfg, it) \
    for (CFG::BBVector::const_iterator it = cfg->Begin(); \
           it != cfg->End(); ++it)

class CFGBuilder : public MaoFunctionPass {
 public:
  CFGBuilder(MaoUnit *mao_unit, Function *function, CFG *CFG);
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
  bool      dump_vcg_ : 1;


  class CFGStat : public Stat {
   public:
    CFGStat() : number_of_direct_jumps_(0), number_of_indirect_jumps_(0),
                number_of_jump_table_patterns_(0), number_of_vaarg_patterns_(0),
                number_of_tail_calls_(0)
    {;}
    ~CFGStat() {;}
    void FoundDirectJump()        { ++number_of_direct_jumps_; }
    void FoundIndirectJump()      { ++number_of_indirect_jumps_; }
    void FoundJumpTablePattern()  { ++number_of_jump_table_patterns_; }
    void FoundVaargPattern()      { ++number_of_vaarg_patterns_; }
    void FoundTailCall()          { ++number_of_tail_calls_; }


    virtual void Print(FILE *out) {
      fprintf(out, "Direct  jumps:      %7d\n", number_of_direct_jumps_);
      fprintf(out, "Indirect jumps:     %7d\n", number_of_indirect_jumps_);
      fprintf(out, "Jump table patterns:%7d\n", number_of_jump_table_patterns_);
      fprintf(out, "VA_ARG patterns    :%7d\n", number_of_vaarg_patterns_);
      fprintf(out, "Tail calls         :%7d\n", number_of_tail_calls_);
    }

   private:
    int number_of_direct_jumps_;
    int number_of_indirect_jumps_;
    int number_of_jump_table_patterns_;
    int number_of_vaarg_patterns_;
    int number_of_tail_calls_;
  };

  bool collect_stat_;
  CFGStat *cfg_stat_;
};

void CreateCFG(MaoUnit *mao_unit, Function *function, CFG *cfg);

#endif  // MAOCFG_H_
