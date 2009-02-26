#ifndef MAO_LOOPS_H_INCLUDED_
#define MAO_LOOPS_H_INCLUDED_
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
//    SimpleLoop - a class representing a single loop in a routine
//    LoopStructureGraph - a class representing the nesting relationships
//                 of all loops in a routine.
//
#include <set>
#include <list>
#include <algorithm>

#include "MaoDebug.h"
#include "MaoCFG.h"

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


  SimpleLoop() : header_(NULL), parent_(NULL), is_root_(false), is_reducible_(true),
                 depth_level_(0), nesting_level_(0) {
  }

  void AddNode(BasicBlock *bb) {
    MAO_ASSERT(bb);
    basic_blocks_.insert(bb);
  }

  void AddChildLoop(SimpleLoop *loop) {
    MAO_ASSERT(loop);
    children_.insert(loop);
  }

  void Dump() {
    if (is_root())
      fprintf(stderr, "<root>");
    else
      fprintf(stderr, "loop-%d", counter_);
  }

  void DumpLong() {
    Dump();
    if (!is_reducible())
      fprintf(stderr, "*IRREDUCIBLE* ");

    fprintf(stderr, " depth: %d, nest: %d ",
            depth_level(), nesting_level());

    if (parent_) {
      fprintf(stderr, "Parent: ");
      parent_->Dump();
      fprintf(stderr, " ");
    }

    if (basic_blocks_.size()) {
      fprintf(stderr, "BBs: ");
      for (BasicBlockSet::iterator bbiter = basic_blocks_.begin();
           bbiter != basic_blocks_.end(); ++bbiter) {
        BasicBlock *BB = *bbiter;
        fprintf(stderr, "BB%d%s ", BB->id(),
                BB == header() ? "<head> " : "");
      }
    }

    if (children_.size()) {
      fprintf(stderr, "Children: ");
      for(std::set<SimpleLoop *>::iterator citer =  children_.begin();
          citer != children_.end(); ++citer) {
        SimpleLoop *loop = *citer;
        loop->Dump();
        fprintf(stderr, " ");
      }
    }
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
  bool         is_reducible() { return is_reducible_; }
  BasicBlock  *header() { return header_; }

  void set_parent(SimpleLoop *parent) {
    MAO_ASSERT(parent);
    parent_ = parent;
    parent_->AddChildLoop(this);
  }
  void set_header(BasicBlock *header, bool add_node = true) {
    MAO_ASSERT(header);

    if (add_node) AddNode(header);
    header_ = header;
  }

  void set_is_root() { is_root_ = true; }
  void set_counter(unsigned int value) { counter_ = value; }
  void set_nesting_level(unsigned int level) {
    nesting_level_ = level;
    if (level==0)
      set_is_root();
  }
  void set_depth_level(unsigned int level) { depth_level_ = level; }
  void set_is_reducible(bool val) { is_reducible_ = val; }


  // Iterators
  BasicBlockSet::iterator BasicBlockBegin() {return basic_blocks_.begin();}
  BasicBlockSet::iterator BasicBlockEnd() {return basic_blocks_.end();}

  private:
  BasicBlockSet          basic_blocks_;
  BasicBlock            *header_;
  std::set<SimpleLoop *> children_;
  SimpleLoop            *parent_;

  bool         is_root_: 1;
  bool         is_reducible_ : 1;
  unsigned int counter_;
  unsigned int depth_level_;
  unsigned int nesting_level_;
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
    root_->set_nesting_level(0);  // make it root node
    root_->set_counter(loop_counter_++);
    AddLoop(root_);
  }

  ~LoopStructureGraph() {
    KillAll();
  }

  SimpleLoop *CreateNewLoop() {
    SimpleLoop *loop = new SimpleLoop();
    loop->set_counter(loop_counter_++);
    return loop;
  }

  void      KillAll() {
    loops_.clear();
  }

  void AddLoop(SimpleLoop *loop) {
    MAO_ASSERT(loop);
    loops_.push_back(loop);
  }

  void Dump() {
    DumpRec(root_, 0);
  }

  void DumpRec(SimpleLoop *loop, unsigned int indent) {
    for (unsigned int i = 0; i < indent; i++)
      fprintf(stderr, "    ");
    loop->DumpLong();
    fprintf(stderr, "\n");

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
      if (!loop->parent())
        loop->set_parent(root_);
    }

    // recursively traverse the tree and assign levels
    CalculateNestingLevelRec(root_, 0);
  }


  void CalculateNestingLevelRec(SimpleLoop *loop, unsigned int depth) {
    MAO_ASSERT(loop);

    loop->set_depth_level(depth);
    for (SimpleLoop::LoopSet::iterator liter = loop->GetChildren()->begin();
         liter != loop->GetChildren()->end(); ++liter) {
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



// External Entry Points
LoopStructureGraph *PerformLoopRecognition(MaoUnit *mao, const CFG *CFG,
                                           const char *cfg_name);


#endif // MAO_LOOPS_H_INCLUDED_
