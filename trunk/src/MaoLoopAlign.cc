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


class LoopAlignPass : public MaoPass {
 public:
  explicit LoopAlignPass(MaoUnit *mao, LoopStructureGraph *loop_graph,
                         MaoRelaxer::SizeMap *sizes);
  void FindInner(SimpleLoop *loop);
  void DoLoopAlign();
  int GetBassicBlockSize(BasicBlock *BB);

 private:
  MaoUnit  *mao_unit_;
  LoopStructureGraph *loop_graph_;
  MaoRelaxer::SizeMap *sizes_;
  int maximum_loop_size_;

  bool collect_stat_;
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
    : MaoPass("LOOPALIGN", mao->mao_options(), MAO_OPTIONS(LOOPALIGN), true),
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
    FindInner(loop_graph_->root());
  }
  return;
}


// TODO(martint): make this const
int LoopAlignPass::GetBassicBlockSize(BasicBlock *BB) {
  int size = 0;
  for (SectionEntryIterator iter = BB->EntryBegin();
       iter != BB->EntryEnd();
       ++iter) {
    MaoEntry *entry = *iter;
    size += (*sizes_)[entry];
  }
  Trace(3, "Size for bb[%3d] is %d.",
        BB->id(),
        size);
  return size;
}



// Function called recurively to find the inner loops that are candidates
// for alignment.
void LoopAlignPass::FindInner(SimpleLoop *loop) {
  if (loop->nesting_level() == 0 &&   // Leaf node = Inner loop
      !loop->is_root()) {             // Make sure its not the root node
    // Found an inner loop
    Trace(2, "Process inner loop...");
    int size = 0;
    // Loop over basic block to get their sizes!
    for (SimpleLoop::BasicBlockSet::iterator bbiter = loop->BasicBlockBegin();
         bbiter != loop->BasicBlockEnd(); ++bbiter) {
      BasicBlock *BB = *bbiter;
      size += GetBassicBlockSize(BB);
    }
    if (size <= maximum_loop_size_) {
      // We have found an inner loop that we should align
      // TODO(martint): add code here to add alignment
      for (SimpleLoop::BasicBlockSet::iterator bb_iter_i =
               loop->BasicBlockBegin();
           bb_iter_i != loop->BasicBlockEnd(); ++bb_iter_i) {
        BasicBlock *bb_i = *bb_iter_i;
        SimpleLoop::BasicBlockSet::iterator bb_iter_j = bb_iter_i;
        ++bb_iter_j;
        for (;
             bb_iter_j != loop->BasicBlockEnd(); ++bb_iter_j) {
          BasicBlock *bb_j = *bb_iter_j;
          // Check:
          if (bb_i->IsArgumentBeforeInSection(bb_j)) {
            Trace(3, "BB %d  is after BB %d",
                  bb_i->id(),
                  bb_j->id());
          }
          if (bb_i->IsArgumentAfterInSection(bb_j)) {
            Trace(3, "BB %d  is before BB %d",
                  bb_i->id(),
                  bb_j->id());
          }
        }
      }
    }
    if (collect_stat_) {
      mao_unit_->stat()->LoopAlignAddInnerLoop(size,
                                               size <= maximum_loop_size_);
    }
  }
  for (SimpleLoop::LoopSet::iterator liter = loop->GetChildren()->begin();
       liter != loop->GetChildren()->end(); ++liter) {
    FindInner(*liter);
  }
}



// External Entry Point
//
void DoLoopAlign(MaoUnit *mao,
                 Function *function) {
  // Make sure the analysis have been run on this function
  MAO_ASSERT(function->cfg()   != NULL);
  MAO_ASSERT(function->lsg()   != NULL);
  MAO_ASSERT(function->sizes() != NULL);
  LoopAlignPass align(mao, function->lsg(), function->sizes());
  align.set_timed();
  align.DoLoopAlign();
  return;
}
