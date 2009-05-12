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
MAO_OPTIONS_DEFINE(LOOP16, 2) {
  OPTION_INT("max_fetch_lines",  10,
             "Seek to align loops with size < max_fetch_lines*fetchline_size"),

  OPTION_INT("fetch_line_size", 16, "Fetchline size")
};

// Align Tiny Loops to 16 Bytes
//
class AlignTinyLoops16 : public MaoFunctionPass {
  class AlignCandidate {
   public:
    AlignCandidate(const SimpleLoop *loop,
                   const BasicBlock *min_bb,
                   const BasicBlock *max_bb) :
      loop_(loop), min_bb_(min_bb), max_bb_(max_bb) {}

    const BasicBlock *min_bb() { return min_bb_; }
    const BasicBlock *max_bb() { return max_bb_; }
   protected:
    const SimpleLoop *loop_;
    const BasicBlock *min_bb_, *max_bb_;
  };
  typedef std::list<AlignCandidate *> LoopList;

 public:
  AlignTinyLoops16(MaoUnit *mao, Function *function) :
    MaoFunctionPass("LOOP16", mao->mao_options(), MAO_OPTIONS(LOOP16),
                    mao, function) {
    fetchline_size_  = GetOptionInt("fetch_line_size");
    max_fetch_lines_ = GetOptionInt("max_fetch_lines");
  }

  // Find Candidates for loop alignment. Candidates are all
  // very short loops, basically all loops with size < max_loop_size
  //
  void FindCandidates(const SimpleLoop  *loop,
                      MaoEntryIntMap    *offsets,
                      MaoEntryIntMap    *sizes) {
    MAO_ASSERT(offsets);
    MAO_ASSERT(sizes);

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

      int end_off = (*offsets)[max_bb->last_entry()] +
                    (*sizes)[max_bb->last_entry()];
      int start_off   = (*offsets)[min_bb->first_entry()];
      int size = end_off - start_off;

      // add this loop to list of candidates, sorted by
      // starting offset.
      //
      if (size < max_fetch_lines_*fetchline_size_) {
        bool linked_in = false;
        for (LoopList::iterator iter = candidates_.begin();
             iter != candidates_.end(); ++iter) {
          if ((*offsets)[(*iter)->min_bb()->first_entry()] > start_off) {
            candidates_.insert(iter, new AlignCandidate(loop, min_bb, max_bb));
            linked_in = true;
          }
        }
        if (!linked_in)
          candidates_.push_back(new AlignCandidate(loop, min_bb, max_bb));
      }
      return;
    }

    // Recursive call in order to find all
    for (SimpleLoop::LoopSet::const_iterator liter = loop->ConstChildrenBegin();
         liter != loop->ConstChildrenEnd(); ++liter) {
      FindCandidates(*liter, offsets, sizes);
    }
  }


  // Align loops. Iterate over loops in top down address order,
  // if a loop is alignable, and sizes and offsets fit into the simple
  // heuristics outlines below, then insert a .p2align directive.
  //
  // After each re-alignment, a new relaxation pass is needed.
  //
  void AlignInner(const SimpleLoop  *loop,
                  Function          *function) {
    MAO_ASSERT(function);
    MaoEntryIntMap *sizes, *offsets;

    sizes = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
    offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());

    FindCandidates(loop, offsets, sizes);

    for (LoopList::iterator iter = candidates_.begin();
         iter != candidates_.end(); ++iter) {
      int end_off = (*offsets)[(*iter)->max_bb()->last_entry()] +
                    (*sizes)[(*iter)->max_bb()->last_entry()];
      int start_off   = (*offsets)[(*iter)->min_bb()->first_entry()];
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

      if (lines > 1 && start_used < (16 - end_used)) {
        Trace(0, "  -> Alignment possible, up %d bytes, save 1/%d fetch lines",
              start_used,
              end_fetch - start_fetch + 1);

        // These are the heuristics
        //
        if ((lines <= 4) ||
            (lines == 5 && start_used < 13) ||
            (lines == 6 && start_used < 11) ||
            (lines == 7 && start_used < 9)  ||
            (lines  > 7 && start_used < 5)) {
          Trace(0, "  -> Alignment DONE");
          (*iter)->min_bb()->first_entry()->AlignTo(16);

          MaoRelaxer::InvalidateSizeMap(function_->GetSection());
          sizes = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
          offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());
        }
      }
    }
  }


  bool Go() {
    loop_graph_ =  LoopStructureGraph::GetLSG(unit_, function_);
    if (!loop_graph_ ||
        !loop_graph_->NumberOfLoops()) return true;

    AlignInner(loop_graph_->root(), function_);
    return true;
  }

 private:
  LoopStructureGraph *loop_graph_;
  LoopList  candidates_;
  int       fetchline_size_;
  int       max_fetch_lines_;
};

// External Entry Point
//
void InitAlignTinyLoops16() {
  RegisterFunctionPass(
      "LOOP16",
      MaoFunctionPassManager::GenericPassCreator<AlignTinyLoops16>);
}
