//
// Copyright 2009 and later Google Inc.
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
//   Free Software Foundation Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// Interfaces to loop related functionality in MAO
//
#include <set>
#include <list>
#include <algorithm>

#include "MaoDebug.h"

// Forwards
class BasicBlock;

//
// SimpleLoop
//
// Basic representation of loops, a loop has an entry point,
// one or more exit edges, a set of basic blocks, and potentially
// an outer loop - a "parent" loop.
//
// Furthermore, it can have any set of properties, e.g.,
// it can be an irreducible loop, have control flow, be
// a candidate for transformations, and what not.
//
//
class SimpleLoop {
  public:
  typedef std::set<BasicBlock *> BasicBlockSet;
  typedef std::set<SimpleLoop *> LoopSet;


  SimpleLoop() : is_root_(false), nesting_level_(0), depth_level_(0) {
  }

  void AddNode(BasicBlock *bb) {
    MAO_ASSERT(bb);
    basic_blocks_.insert(bb);
  }

  void AddChildLoop(SimpleLoop *loop) {
    MAO_ASSERT(loop);
    children_.insert(loop);
    loop->set_parent(this);
  }

  void Dump() {
    fprintf(MaoDebug::trace_file(), "loop-%d", counter_);
  }

  void DumpLong() {
    Dump();
    fprintf(MaoDebug::trace_file(), " d: %d, n: %d ",
            depth_level(), nesting_level());

    for (BasicBlockSet::iterator bbiter = basic_blocks_.begin();
         bbiter != basic_blocks_.end(); ++bbiter)
      fprintf(MaoDebug::trace_file(), "BB%d ", 0);

    fprintf(MaoDebug::trace_file(), "\n");
  }

  LoopSet *GetChildren() {
    return &children_;
  }

  // Getters/Setters
  SimpleLoop  *parent() { return parent_; }
  unsigned int nesting_level() { return nesting_level_; }
  unsigned int depth_level() { return depth_level_; }
  unsigned int counter() { return counter_; }
  bool         is_root() { return is_root_; }

  void set_parent(SimpleLoop *parent) {
    MAO_ASSERT(parent);
    parent_ = parent;
    parent->AddChildLoop(this);
  }

  void set_is_root() { is_root_ = true; }
  void set_counter(unsigned int value) { counter_ = value; }
  void set_nesting_level(unsigned int level) { nesting_level_ = level; }
  void set_depth_level(unsigned int level) { depth_level_ = level; }

  private:
  BasicBlockSet          basic_blocks_;
  std::set<SimpleLoop *> children_;
  SimpleLoop            *parent_;

  bool         is_root_: 1;
  unsigned int counter_;
  unsigned int nesting_level_;
  unsigned int depth_level_;
};



//
// LoopStructureGraph
//
// Maintain loop structure for a given CFG
//
// Two values are maintained for this loop graph, depth, and nesting level.
// For example:
//
// loop        nesting level    depth
//----------------------------------------
// loop-0      2                0
//   loop-1    1                1
//   loop-3    1                1
//     loop-2  0                2
//
class LoopStructureGraph {
  public:
  typedef std::list<SimpleLoop *> LoopList;

  LoopStructureGraph() : root_(new SimpleLoop()),
                         loop_counter_(0) {
    root_->set_nesting_level(0);
    root_->set_counter(loop_counter_++);
  }

  SimpleLoop *CreateNewLoop() {
    SimpleLoop *loop = new SimpleLoop();
    loop->set_counter(loop_counter_++);
    return loop;
  }

  void      KillAll() {
    // TODO(rhundt): Free up resources
  }

  void AddLoop(SimpleLoop *loop) {
    MAO_ASSERT(loop);
    loops_.push_back(loop);
  }

  void Dump() {
    DumpRec(root_, 0);
  }

  void DumpRec(SimpleLoop *loop, unsigned int indent) {
    if (!loop->is_root()) {
      for (unsigned int i = 0; i < indent; i++)
        fprintf(MaoDebug::trace_file(), "  ");
      loop->Dump();
    }
    for (SimpleLoop::LoopSet::iterator liter = loop->GetChildren()->begin();
         liter != loop->GetChildren()->end(); ++liter)
      DumpRec(*liter,  indent+1);
  }

  void CalculateNestingLevel() {
    // link up all 1st level loops to artificial root node
    for (LoopList::iterator liter = loops_.begin();
         liter != loops_.end(); ++liter) {
      SimpleLoop *loop = *liter;
      if (loop->is_root()) continue;
      if (!loop->parent()) loop->set_parent(root_);
    }

    // recursively traverse the tree and assign levels
    CalculateNestingLevelRec(root_, 0);
  }


  void CalculateNestingLevelRec(SimpleLoop *loop, unsigned int depth) {
    MAO_ASSERT(loop);

    loop->set_depth_level(depth);
    for (LoopList::iterator liter = loops_.begin();
         liter != loops_.end(); ++liter) {
      CalculateNestingLevelRec(*liter, depth+1);

      loop->set_nesting_level(std::max(loop->nesting_level(),
                                       1+(*liter)->nesting_level()));
    }
  }

  unsigned int NumberOfLoops() { return loops_.size(); }

  SimpleLoop *root() { return root_; }

  private:
  SimpleLoop   *root_;
  LoopList      loops_;
  unsigned int  loop_counter_;
};
