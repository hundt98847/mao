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

#include <stdio.h>
#include <map>


#include "MaoLoopAlign.h"


// Pass that finds paths in inner loops that fits in 4 16-byte lines
// and alligns all (chains of) basicb locks within the paths.
class LoopAlignPass : public MaoPass {
 public:
  explicit LoopAlignPass(MaoUnit *mao, LoopStructureGraph *loop_graph,
                         MaoRelaxer::SizeMap *sizes);
  void DoLoopAlign();

 private:
  // Path is a list of basic blocks that form a way through the loop
  typedef std::vector<BasicBlock *> Path;
  typedef std::vector<Path *> Paths;

  MaoUnit  *mao_unit_;
  // This is the result found by the loop sequence detector.
  LoopStructureGraph *loop_graph_;
  // This is the sizes of all the instructions in the section
  // as found by the relaxer.
  MaoRelaxer::SizeMap *sizes_;

  // TODO(martint): find a useful parameter to pass into the pass
  int maximum_loop_size_;

  // Collect stat during the pass?
  bool collect_stat_;

  // Returns the size of the entries in the loop.
  int LoopSize(const SimpleLoop *loop) const;
  // Returns the sizes of the entries in the path.
  int PathSize(const Path *path) const;

  // Used to find inner loops
  void FindInner(const SimpleLoop *loop, MaoRelaxer::SizeMap *offsets);

  // Get all the possible paths found in a loop. Return the paths in the
  // paths variable.
  void GetPaths(const SimpleLoop *loop, Paths *paths);

  // Helper function for GetPaths
  void GetPathsImp(const SimpleLoop *loop, BasicBlock *current,
                    Paths *paths, Path *path);

  // Adds a path to the paths collection. The path given is copied.
  void AddPath(Paths *paths, const Path *path);
  // Free all the memory allocated for paths.
  void FreePaths(Paths *paths);

  // Get the number of 16-byte lines needed to store the path.
  int NumLines(const Path *path, std::map<BasicBlock *, int> *chain_sizes);

  // Once a path is identified, align it if it fits in the lsd.
  void ProcessPath(Path *path);

  // Return the size of the basic block.
  int BasicBlockSize(BasicBlock *BB) const;

  // A chain is a set of basic blocks that are stored next to eachother.
  // Given the first basic block in the chain, return the size of the whole
  // chain.
  int ChainSize(BasicBlock *basicblock,
                std::map<BasicBlock *, BasicBlock *> *connections);
  // Try to align the paths in a given inner loop.
  void ProcessInnerLoop(const SimpleLoop *loop,
                        MaoRelaxer::SizeMap *offsets);
  void ProcessInnerLoopOld(const SimpleLoop *loop);

  // Add the align directive to the given basic block.
  void AlignBlock(BasicBlock *basicblock);
};


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(LOOPALIGN, 2) {
  OPTION_INT("loop_size", 64, "Maximum size for loops to be "
                              "considered for alignment."),
  OPTION_BOOL("stat", false, "Collect and print(trace) "
                             "statistics about loops."),
};

LoopAlignPass::LoopAlignPass(MaoUnit *mao, LoopStructureGraph *loop_graph,
                             MaoRelaxer::SizeMap *sizes)
    : MaoPass("LOOPALIGN", mao->mao_options(), MAO_OPTIONS(LOOPALIGN), false),
      mao_unit_(mao),
      loop_graph_(loop_graph),
      sizes_(sizes) {

  maximum_loop_size_ = GetOptionInt("loop_size");

  collect_stat_ = GetOptionBool("stat");
  if (collect_stat_) {
    mao->stat()->LoopAlignRegister();
  }
}

void LoopAlignPass::DoLoopAlign() {
  Trace(2, "%d loop(s).", loop_graph_->NumberOfLoops()-1);
  if (loop_graph_->NumberOfLoops() > 1) {
    MaoRelaxer::SizeMap offsets;
    // iterate over the entries in the section to solve this..
    Section *text_section = mao_unit_->GetSection(".text");
    MAO_ASSERT(text_section);
    int offset = 0;
    for (SectionEntryIterator iter = text_section->EntryBegin();
         iter != text_section->EntryEnd();
         ++iter) {
      offsets[*iter] = offset;
      offset += (*sizes_)[*iter];
    }
    // TODO(martint): send in the offsetmap
    FindInner(loop_graph_->root(),  &offsets);
  }
  return;
}


int LoopAlignPass::BasicBlockSize(BasicBlock *BB) const {
  int size = 0;
  for (SectionEntryIterator iter = BB->EntryBegin();
       iter != BB->EntryEnd();
       ++iter) {
    MaoEntry *entry = *iter;
    size += (*sizes_)[entry];
  }
  return size;
}


// Given a path, create a newly allocated copy and put it in the collection
// of found paths
void LoopAlignPass::AddPath(Paths *paths, const Path *path) {
  Path *new_path = new Path();
  for (Path::const_iterator iter = path->begin(); iter != path->end(); ++iter) {
    new_path->push_back(*iter);
  }
  paths->push_back(new_path);
}


void LoopAlignPass::FreePaths(Paths *paths) {
  for (Paths::iterator iter = paths->begin(); iter != paths->end(); ++iter) {
    free(*iter);
  }
}

// Given an inner loop, find all possible paths and place them in paths
void LoopAlignPass::GetPaths(const SimpleLoop *loop, Paths *paths) {
  // iterate over successors that are in the loop
  BasicBlock *head = loop->header();
  Path path;
  path.push_back(head);
  for (BasicBlock::ConstEdgeIterator iter = head->BeginOutEdges();
       iter != head->EndOutEdges();
       ++iter) {
    BasicBlock *dest = (*iter)->dest();
    // Dont process basic blocks that are not part of the loop
    if (loop->Includes(dest)) {
      GetPathsImp(loop, dest, paths, &path);
    }
  }
}

void LoopAlignPass::GetPathsImp(const SimpleLoop *loop,
                                BasicBlock *current,
                                Paths *paths, Path *path) {
  // Needs to be an inner loop
  MAO_ASSERT(loop->NumberOfChildren() == 0);
  if (current == loop->header()) {
    // Add the path the list of found paths
    // Memory allocated here must be freed later.
    AddPath(paths, path);
  } else {
    // Since this is an inner loop, current is not in the path yet. add it
    path->push_back(current);
    // loop over the children
    for (BasicBlock::ConstEdgeIterator iter = current->BeginOutEdges();
         iter != current->EndOutEdges();
         ++iter) {
      BasicBlock *dest = (*iter)->dest();
      if (loop->Includes(dest)) {
        GetPathsImp(loop, dest, paths, path);
      }
    }
    path->pop_back();
  }
}


// Find out how many lines a 16 bytes are needed
// to store the set of basic blocks in the path.
// No assumptions about the order of the paths.
// For each set of consecutive basic block, see how many
// lines they require. Add up the total number of lines.
//
//  Assume that BB1, BB2, .. are stored in memory after each other
//  Assume that BB1, BB2, BB  are in the loop
//  Then the result will look like this
//      {BB1, BB2} -> size(BB1, BB2)
//      {BB4}      -> size(BB4)
//
// With this information, we can calculate how many lines required to
// hold this loop path.
void LoopAlignPass::ProcessPath(Path *path) {
  Trace(3, "Process path:");
  // Keeps track of what blocks we have already processed
  std::map<BasicBlock *, BasicBlock *> f_connections;
  std::map<BasicBlock *, BasicBlock *> b_connections;

  for (Path::iterator iiter = path->begin(); iiter !=  path->end(); ++iiter) {
    for (Path::iterator jiter = path->begin(); jiter !=  path->end(); ++jiter) {
      if (*iiter != *jiter) {
        if ((*iiter)->DirectlyPreceeds(*jiter)) {
          // Connect them!
          MAO_ASSERT(f_connections.find(*iiter) == f_connections.end() ||
                     f_connections[*iiter] == *jiter);
          MAO_ASSERT(b_connections.find(*jiter) == b_connections.end() ||
                     b_connections[*jiter] == *iiter);
          f_connections[*iiter] = *jiter;
          b_connections[*jiter] = *iiter;
        }
        if ((*jiter)->DirectlyPreceeds(*iiter)) {
          // Connect them!
          MAO_ASSERT(b_connections.find(*iiter) == b_connections.end() ||
                     b_connections[*iiter] == *jiter);
          MAO_ASSERT(f_connections.find(*jiter) == f_connections.end() ||
                     f_connections[*jiter] == *iiter);
          f_connections[*jiter] = *iiter;
          b_connections[*iiter] = *jiter;
        }
      }
    }
  }

  // Maps the startblock of the chain the size of the chain.
  std::map<BasicBlock *, int> chain_sizes;
  // Now we are ready to check for sizes.
  for (Path::iterator iter = path->begin(); iter !=  path->end(); ++iter) {
    // is it a start of a chain?
    if (b_connections.find(*iter) == b_connections.end()) {
      chain_sizes[*iter] = ChainSize(*iter, &f_connections);
    }
  }

  int lines = NumLines(path, &chain_sizes);
  MAO_ASSERT(16*lines >= PathSize(path));

  if (lines <= 4) {
    // Align path!
    for (Path::iterator iter = path->begin(); iter !=  path->end(); ++iter) {
      // is it a start of a chain?
      if (b_connections.find(*iter) == b_connections.end()) {
        // This is the start of a chain which should be aligned!
        Trace(3, "Aligned block :%d", (*iter)->id());
        //  AlignBlock(*iter);
      }
    }
  }
}


// Add an alignment directive at start of this basicblock
// TODO(martint): add it after label, and only add it if its not already
// aligned.
void LoopAlignPass::AlignBlock(BasicBlock *basicblock) {
  MAO_ASSERT(basicblock);
  MaoEntry *entry = basicblock->first_entry();

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(6));
  DirectiveEntry *alignment_directive =
      new DirectiveEntry(DirectiveEntry::P2ALIGN, operands,
                         0, NULL, mao_unit_);
  // Now we have the new entry! the following needs to be updated
  // 1. the entries themselves!

  MaoEntry *prev_entry = entry->prev();
  prev_entry->set_next(alignment_directive);
  alignment_directive->set_prev(prev_entry);

  entry->set_prev(alignment_directive);
  alignment_directive->set_next(entry);

  // 2. the basic block
  basicblock->set_first_entry(alignment_directive);

  // TODO: update sections/functions?
}



int LoopAlignPass::ChainSize(BasicBlock *basicblock,
                           std::map<BasicBlock *, BasicBlock *> *connections) {
  int size = 0;
  while (connections->find(basicblock) != connections->end()) {
    size += BasicBlockSize(basicblock);
    basicblock = (*connections)[basicblock];
  }
  size += BasicBlockSize(basicblock);
  return size;
}


void LoopAlignPass::ProcessInnerLoop(const SimpleLoop *loop,
                                     MaoRelaxer::SizeMap *offsets) {
  // Find out if all the blocks are aligned after each other!

  // Keeps track of what blocks we have already processed
  // Could be solved by using the sizemap and create offsets
  // into the section!
  // TODO(martint)
  // now we can find the largest and smallest value!

  BasicBlock *first = *(loop->ConstBasicBlockBegin());
  BasicBlock *last = *(loop->ConstBasicBlockBegin());
  for (SimpleLoop::BasicBlockSet::iterator iter = loop->ConstBasicBlockBegin();
       iter != loop->ConstBasicBlockEnd();
       ++iter) {
    if ((*offsets)[(*iter)->first_entry()] >
        (*offsets)[last->first_entry()]) {
      last = *iter;
    }
    if ((*offsets)[(*iter)->first_entry()] <
        (*offsets)[first->first_entry()]) {
      first = *iter;
    }
  }
  int loop_size = (*offsets)[last->last_entry()] + (*sizes_)[last->last_entry()]
      - (*offsets)[first->first_entry()];
  if (loop_size <= 64) {
    // Add align directive
    AlignBlock(first);
  }
  Trace(3, "Loop size is: %d", loop_size);
}



// Given an inner loop, get all the paths that go from the
// header, back to the header. If any paths fit in the
// loop buffer, consider it for alignment
void LoopAlignPass::ProcessInnerLoopOld(const SimpleLoop *loop) {
  // Hold all the paths found in the loop
  Paths paths;
  // Get all the possible paths in this loop
  GetPaths(loop, &paths);

  // Process the paths
  for (Paths::iterator iter = paths.begin();
       iter != paths.end();
       ++iter) {
    // Process the given path
    ProcessPath(*iter);
  }
  // Return memory
  FreePaths(&paths);
}

int LoopAlignPass::NumLines(const Path *path,
                            std::map<BasicBlock *, int> *chain_sizes) {
  int lines = 0;
  for (Path::const_iterator iter = path->begin();
       iter != path->end();
       ++iter) {
    if (chain_sizes->find(*iter) != chain_sizes->end()) {
      // This assert would trigger if it found
      // basic blocks without any size!
      MAO_ASSERT((*chain_sizes)[*iter] >= 0);
      int c_lines = ((*chain_sizes)[*iter]-1)/16+1;
      lines += c_lines;
    }
  }
  return lines;
}

int LoopAlignPass::LoopSize(const SimpleLoop *loop) const {
  int size = 0;
  // Loop over basic block to get their sizes!
  for (SimpleLoop::BasicBlockSet::const_iterator iter =
           loop->ConstBasicBlockBegin();
       iter != loop->ConstBasicBlockEnd();
       ++iter) {
    size += BasicBlockSize(*iter);
  }
  return size;
}


int LoopAlignPass::PathSize(const Path *path) const {
  int size = 0;
  // Loop over basic block to get their sizes!
  for (Path::const_iterator iter = path->begin();
       iter != path->end();
       ++iter) {
    size += BasicBlockSize(*iter);
  }
  return size;
}


// Function called recurively to find the inner loops that are candidates
// for alignment.
void LoopAlignPass::FindInner(const SimpleLoop *loop,
                              MaoRelaxer::SizeMap *offsets) {
  if (loop->nesting_level() == 0 &&   // Leaf node = Inner loop
      !loop->is_root()) {             // Make sure its not the root node
    // Found an inner loop
    MAO_ASSERT(loop->NumberOfChildren() == 0);
    Trace(3, "Found an inner loop...");

    // Currently we see if there any paths in the inner loops that are
    // candiates for alignment!
    int size = LoopSize(loop);

    if (size <= 64) {
      ProcessInnerLoop(loop, offsets);
    }

    // Statistics gathering
    if (collect_stat_) {
      mao_unit_->stat()->LoopAlignAddInnerLoop(size,
                                               size <= maximum_loop_size_);
    }
  }

  // Recursive call in order to find all
  for (SimpleLoop::LoopSet::const_iterator liter = loop->ConstChildrenBegin();
       liter != loop->ConstChildrenEnd(); ++liter) {
    FindInner(*liter, offsets);
  }
}

// External Entry Point
//
void DoLoopAlign(MaoUnit *mao,
                 Function *function) {
  // Make sure the analysis have been run on this function
  LoopAlignPass align(mao,
                      function->lsg(),
                      MaoRelaxer::GetSizeMap(mao, function));
  if (align.enabled()) {
    align.set_timed();
    align.DoLoopAlign();
  }
  return;
}
