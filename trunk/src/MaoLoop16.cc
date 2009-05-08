//
// Copyright 2008 Google Inc.
//
// This program is free software; you can redistribute it and/or to
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
//   51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.

// Align tiny loops to 16 byte boundaries to avoid
// having to fetch two instruction lines for every
// iteration. These seems to cause an 9% degradation
// on SPEC 2000 252.eon.
//
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"
#include "MaoLoops.h"
#include "MaoRelax.h"

#include <map>

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(LOOP16, 0) {
};

// Align Tiny Loops to 16 Bytes
//
class AlignTinyLoops16 : public MaoPass {
 public:
  AlignTinyLoops16(MaoUnit *mao, Function *function) :
    MaoPass("LOOP16", mao->mao_options(), MAO_OPTIONS(LOOP16), false),
    mao_(mao), function_(function) {
  }


  void AlignInner(const SimpleLoop *loop,
                  MaoRelaxer::SizeMap *offsets) {
    MAO_ASSERT(offsets);

    if (!loop->nesting_level() &&   // Leaf node = Inner loop
        !loop->is_root()) {         // not root
      // Found an inner loop
      MAO_ASSERT(loop->NumberOfChildren() == 0);

      // Currently we see if there any paths in the inner loops that are
      // candiates for alignment!
      MAO_ASSERT(loop->bottom());

      BasicBlock *min_bb = loop->header(), *max_bb = loop->bottom();
      for (SimpleLoop::BasicBlockSet::const_iterator iter =
             loop->ConstBasicBlockBegin();
           iter != loop->ConstBasicBlockEnd(); ++iter) {
        if ((*offsets)[(*iter)->first_entry()] <
            (*offsets)[min_bb->first_entry()])
          min_bb = (*iter);
        if ((*offsets)[(*iter)->first_entry()] >
            (*offsets)[max_bb->first_entry()])
          max_bb = (*iter);
      }


      int end_off = (*offsets)[max_bb->last_entry()];
      int start_off   = (*offsets)[min_bb->first_entry()];
      int size = end_off - start_off;

      int start_fetch = start_off / 16;
      int start_used  = 16 - start_off % 16;
      int end_fetch   = end_off / 16;
      int end_used    = end_off % 16;
      int lines       = end_fetch - start_fetch + 1;

      Trace(0, "Loop, size: %d, start: %d, end: %d, %d fetch lines",
            size, start_off, end_off, lines);
      Trace(0, "  Fetch line %d bytes used, end: %d bytes used",
            start_used, end_used);

      if (lines > 1 &&
          start_used < (16 - end_used)) {
        Trace(0, "  -> Alignment possible, up %d bytes, save 1/%d fetch lines",
              start_used,
              end_fetch - start_fetch + 1);
      }
      return;
    }

    // Recursive call in order to find all
    for (SimpleLoop::LoopSet::const_iterator liter = loop->ConstChildrenBegin();
         liter != loop->ConstChildrenEnd(); ++liter) {
      AlignInner(*liter, offsets);
    }
  }


  void DoAlign() {
    loop_graph_ =  LoopStructureGraph::GetLSG(mao_, function_);
    if (!loop_graph_ ||
        !loop_graph_->NumberOfLoops()) return;

    MaoRelaxer::GetSizeMap(mao_, function_->GetSection());
    AlignInner(loop_graph_->root(), function_->GetSection()->offsets());
  }

 private:
  LoopStructureGraph *loop_graph_;
  MaoUnit *mao_;
  Function *function_;
};

// External Entry Point
//
void PerformAlignTinyLoops16(MaoUnit *mao, Function *function) {
  AlignTinyLoops16 tiny(mao, function);
  if ( tiny.enabled()) {
    tiny.set_timed();
    tiny.DoAlign();
  }
}
