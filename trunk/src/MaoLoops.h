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
// Classes:
//   SimpleLoop - a class representing a single loop in a routine.
//   LoopStructureGraph - a class representing the nesting relationships
//                        of all loops in a routine.
//
// To get the loop structure graph for a function, call
// LoopStructureGraph *lsg = PerformLoopRecognition(mao_unit, function);
//
//
#include <set>
#include <list>
#include <algorithm>

#include "Mao.h"

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


  SimpleLoop() : header_(NULL), bottom_(NULL), parent_(NULL), is_root_(false),
                 is_reducible_(true), depth_level_(0), nesting_level_(0) {
  }

  // Adds a basic block to the loop.
  void AddNode(BasicBlock *bb) {
    MAO_ASSERT(bb);
    basic_blocks_.insert(bb);
  }

  // Adds a child loop (nested inside the current loop).
  void AddChildLoop(SimpleLoop *loop) {
    MAO_ASSERT(loop);
    children_.insert(loop);
  }

  // Dumps the loop to stderr.
  void Dump() const {
    if (is_root())
      fprintf(stderr, "<root>");
    else
      fprintf(stderr, "loop-%d", counter_);
  }

  // Dumps the loop with more details to stderr.
  void DumpLong() const {
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
        fprintf(stderr, "BB%d", BB->id());
        if (BB == header())
          fprintf(stderr, "<head>");
        if (BB == bottom())
          fprintf(stderr, "<bottom>");
        fprintf(stderr, " ");
      }
    }

    if (children_.size()) {
      fprintf(stderr, "Children: ");
      for (std::set<SimpleLoop *>::iterator citer =  children_.begin();
          citer != children_.end(); ++citer) {
        SimpleLoop *loop = *citer;
        loop->Dump();
        fprintf(stderr, " ");
      }
    }
  }

  // Returns the set of child loops of this loop.
  LoopSet *GetChildren() {
    return &children_;
  }

  // Returns the number of children.
  int NumberOfChildren() const {
    return children_.size();
  }



  // Checks if a particular basic block is part of this loop.
  bool Includes(BasicBlock *basicblock) const {
    return (basic_blocks_.find(basicblock) != basic_blocks_.end());
  }

  // Getters/Setters
  // Returns the parent loop of this loop.
  SimpleLoop  *parent() const { return parent_; }
  // Returns the nesting level of the current loop. The nesting level of a loop
  // is 1 greater than that of the child loop with the highest nesting level.
  unsigned int nesting_level() const { return nesting_level_; }
  // Returns the depth level of the current loop. The depth level of a loop
  // is 1 greater than that of its parent loop.
  unsigned int depth_level() const { return depth_level_; }
  // Returns a number that uniquely identifies this loop in the LSG.
  unsigned int counter() const { return counter_; }
  // Returns true if this is at the root of the loop structure graph.
  bool         is_root() const { return is_root_; }
  // Returns true if this loop is reducible.
  bool         is_reducible() const { return is_reducible_; }
  // Returns the header basic block of the loop.
  BasicBlock  *header() const { return header_; }
  // Returns the basic block containing the source of the back edge.
  BasicBlock  *bottom() const { return bottom_; }

  // Sets the parent of the current loop.
  void set_parent(SimpleLoop *parent) {
    MAO_ASSERT(parent);
    parent_ = parent;
    parent_->AddChildLoop(this);
  }
  // Sets the loop header basic block. add_node tells whether the BB has to be
  // added to the loop.
  void set_header(BasicBlock *header, bool add_node = true) {
    MAO_ASSERT(header);

    if (add_node) AddNode(header);
    header_ = header;
  }
  // Sets the basic block containing the source of the back edge.
  void set_bottom(BasicBlock *bottom) {
    MAO_ASSERT(bottom);
    bottom_ = bottom;
  }
  // Sets a flag indicating the loop is at the root of the LSG.
  void set_is_root() { is_root_ = true; }
  // Sets the value of a counter that uniquely identifies the loop.
  void set_counter(unsigned int value) { counter_ = value; }
  // Sets the nesting level.
  void set_nesting_level(unsigned int level) {
    nesting_level_ = level;
    if (level == 0)
      set_is_root();
  }
  // Sets the depth level.
  void set_depth_level(unsigned int level) { depth_level_ = level; }
  // Sets if the loop is reducible.
  void set_is_reducible(bool val) { is_reducible_ = val; }


  // Iterators over the set of basic blocks in this loop.
  BasicBlockSet::iterator BasicBlockBegin() {return basic_blocks_.begin();}
  BasicBlockSet::iterator BasicBlockEnd() {return basic_blocks_.end();}
  BasicBlockSet::const_iterator ConstBasicBlockBegin() const {
    return basic_blocks_.begin();
  }
  BasicBlockSet::const_iterator ConstBasicBlockEnd() const {
    return basic_blocks_.end();
  }

  // Iterators over the set of children loops.
  LoopSet::iterator ChildrenBegin() {return children_.begin();}
  LoopSet::iterator ChildrenEnd() {return children_.end();}
  LoopSet::const_iterator ConstChildrenBegin() const {
    return children_.begin();
  }
  LoopSet::const_iterator ConstChildrenEnd() const {
    return children_.end();
  }



  private:
  BasicBlockSet          basic_blocks_;
  BasicBlock            *header_;
  BasicBlock            *bottom_;
  LoopSet                children_;
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

  static LoopStructureGraph *GetLSG(MaoUnit *mao,
                                    Function *function,
                                    bool conservative = false);

  LoopStructureGraph() : root_(new SimpleLoop()),
                         loop_counter_(0) {
    root_->set_nesting_level(0);  // make it root node
    root_->set_counter(loop_counter_++);
    AddLoop(root_);
  }

  ~LoopStructureGraph() {
    KillAll();
  }

  // Creates an empty new loop.
  SimpleLoop *CreateNewLoop() {
    SimpleLoop *loop = new SimpleLoop();
    loop->set_counter(loop_counter_++);
    return loop;
  }

  // Deletes all loops in this LSG.
  void      KillAll() {
    loops_.clear();
  }

  // Adds a new loop to the LSG.
  void AddLoop(SimpleLoop *loop) {
    MAO_ASSERT(loop);
    loops_.push_back(loop);
  }

  // Dumps the LSG.
  void Dump() {
    DumpRec(root_, 0);
  }

  // Calculates the nesting level of all loops in the LSG.
  void CalculateNestingLevel() {
    // link up all 1st level loops to artificial root node
    for (LoopList::iterator liter = loops_.begin();
         liter != loops_.end(); ++liter) {
      SimpleLoop *loop = *liter;
      if (loop->is_root()) continue;
      if (!loop->parent())
        loop->set_parent(root_);
    }

    // Recursively traverses the tree and assigns levels.
    CalculateNestingLevelRec(root_, 0);
  }

  // Returns number of loops, excluding the artificial root node.
  unsigned int NumberOfLoops() { return loops_.size()-1; }

  // Returns the root loop.
  SimpleLoop *root() { return root_; }

  private:
  SimpleLoop   *root_;
  LoopList      loops_;
  unsigned int  loop_counter_;

  // Helper method for dumping the LSG.
  void DumpRec(SimpleLoop *loop, unsigned int indent) {
    for (unsigned int i = 0; i < indent; i++)
      fprintf(stderr, "    ");
    loop->DumpLong();
    fprintf(stderr, "\n");

    for (SimpleLoop::LoopSet::iterator liter = loop->GetChildren()->begin();
         liter != loop->GetChildren()->end(); ++liter)
      DumpRec(*liter,  indent+1);
  }

  // Helper method to calculate the nesting level.
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
};



// External entry point.
// Computes and returns the loop structure graph for the given function.
LoopStructureGraph *PerformLoopRecognition(MaoUnit *mao, Function *function);


#endif  // MAO_LOOPS_H_INCLUDED_
