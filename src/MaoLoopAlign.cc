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
  int GetBassicBlockSize(const BasicBlock *BB) const;

 private:
  struct LoopStatistics {
    int number_of_inner_loops;
    std::map<int, int> inner_loop_size_distribution;
    int number_of_aligned_loops;
  } loop_statistics;
  void DumpLoopAlignStatistics();
  bool collect_stat_;

  MaoUnit  *mao_unit_;
  LoopStructureGraph *loop_graph_;
  MaoRelaxer::SizeMap *sizes_;
  int maximum_loop_size_;
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
    loop_statistics.number_of_inner_loops = 0;
    loop_statistics.number_of_aligned_loops = 0;
    loop_statistics.inner_loop_size_distribution.clear();
  }
}

void LoopAlignPass::DumpLoopAlignStatistics() {
  // Dump statistics
  Trace(2, "Loop Alignment distribution");
  Trace(2, "  # Inner   loops : %d", loop_statistics.number_of_inner_loops);
  Trace(2, "  # Aligned loops : %d", loop_statistics.number_of_aligned_loops);
  // iterate over distribution
  Trace(2, "   Size : # loops");
  for (std::map<int, int>::const_iterator iter =
           loop_statistics.inner_loop_size_distribution.begin();
       iter != loop_statistics.inner_loop_size_distribution.end();
       ++iter) {
    Trace(2, "   %4d : %4d", iter->first, iter->second);
  }
}

void LoopAlignPass::DoLoopAlign() {
  Trace(2, "%d loops.", loop_graph_->NumberOfLoops());
  FindInner(loop_graph_->root());

  if (collect_stat_) {
    DumpLoopAlignStatistics();
  }
  return;
}

int LoopAlignPass::GetBassicBlockSize(const BasicBlock *BB) const {
  int size = 0;
  // Iterate over entry in BB
  for (MaoUnit::ConstEntryIterator bbentry_iter = BB->BeginEntries();
       bbentry_iter != BB->EndEntries();
       ++bbentry_iter) {
    MaoEntry *entry = *bbentry_iter;
    size += (*sizes_)[entry];
  }
  Trace(3, "Size for bb[%3d] is %d.",
        BB->id(),
        size);
  return size;
}

void LoopAlignPass::FindInner(SimpleLoop *loop) {
  if (loop->nesting_level() == 0) {
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
    } else {
      // The inner loop is to large to be aligned
    }
    if (collect_stat_) {
      ++loop_statistics.number_of_inner_loops;
      ++loop_statistics.inner_loop_size_distribution[size];
      if (size <= maximum_loop_size_) {
        ++loop_statistics.number_of_aligned_loops;
      }
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
                 LoopStructureGraph *loop_graph,
                 MaoRelaxer::SizeMap *sizes) {
  LoopAlignPass align(mao, loop_graph, sizes);
  align.DoLoopAlign();
  return;
}
