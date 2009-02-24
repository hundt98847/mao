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
#include <list>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

#include "MaoLoopAlign.h"


class LoopAlignPass : public MaoPass {
 public:
  explicit LoopAlignPass(MaoUnit *mao, LoopStructureGraph *loop_graph);
  void FindInner(SimpleLoop *loop);
  void DoLoopAlign();

 private:
  MaoUnit  *mao_unit_;
  LoopStructureGraph *loop_graph_;
};


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(LOOPALIGN, 1) {
  OPTION_BOOL("loop_size", false, "Maximum size for loops to be aligned."),
};

LoopAlignPass::LoopAlignPass(MaoUnit *mao, LoopStructureGraph *loop_graph)
    : MaoPass("LOOPALIGN", mao->mao_options(), MAO_OPTIONS(LOOPALIGN), true),
      mao_unit_(mao),
      loop_graph_(loop_graph) {
}


void LoopAlignPass::DoLoopAlign() {
  printf("%d loops.\n", loop_graph_->NumberOfLoops());
  FindInner(loop_graph_->root());
  // Loop over basic block to get their sizes!
  return;
}


void LoopAlignPass::FindInner(SimpleLoop *loop) {
  if (loop->nesting_level() == 0) {
    // TODO(martint): Process inner loop
    printf("Process inner loop.\n");
  }
  for (SimpleLoop::LoopSet::iterator liter = loop->GetChildren()->begin();
       liter != loop->GetChildren()->end(); ++liter) {
    FindInner(*liter);
  }
}



// External Entry Point
//
void DoLoopAlign(MaoUnit *mao, LoopStructureGraph *loop_graph) {
  LoopAlignPass align(mao, loop_graph);
  return align.DoLoopAlign();
}
