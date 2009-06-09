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

// Align backbranches for 2-deep loop nests.
// The idea is to improve branch prediction for the back-edges,
// in particular, for short running loops.
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
MAO_OPTIONS_DEFINE(BACKBRALIGN, 2) {
  OPTION_INT("align_limit", 32, "Align to cross this byte boundary"),
  OPTION_INT("limit", -1, "Limit tranformation invocations")
};

// Align back branches of 2-deep loop nests, such
// that the branch locations cross a 32-byte boundary.
//
class BackBranchAlign : public MaoFunctionPass {

  // Helper data structure to maintain candidate loop nests.
  // Maintain an inner/outer loop pair.
  //
  class AlignCandidate {
   public:
    AlignCandidate( const SimpleLoop *inner_loop,
                    const BasicBlock *inner_min_bb,
                    const BasicBlock *inner_max_bb,
                    const SimpleLoop *outer_loop,
                    const BasicBlock *outer_min_bb,
                    const BasicBlock *outer_max_bb) :
      inner_loop_(inner_loop),
      inner_min_bb_(inner_min_bb),
      inner_max_bb_(inner_max_bb),
      outer_loop_(outer_loop),
      outer_min_bb_(outer_min_bb),
      outer_max_bb_(outer_max_bb) {}

    const BasicBlock *inner_min_bb() { return inner_min_bb_; }
    const BasicBlock *inner_max_bb() { return inner_max_bb_; }
    const SimpleLoop *inner_loop()   { return inner_loop_; }
    const BasicBlock *outer_min_bb() { return outer_min_bb_; }
    const BasicBlock *outer_max_bb() { return outer_max_bb_; }
    const SimpleLoop *outer_loop()   { return outer_loop_; }
   protected:
    const SimpleLoop *inner_loop_;
    const BasicBlock *inner_min_bb_, *inner_max_bb_;
    const SimpleLoop *outer_loop_;
    const BasicBlock *outer_min_bb_, *outer_max_bb_;
  };
  typedef std::list<AlignCandidate *> LoopList;

 public:
  BackBranchAlign(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("BACKBRALIGN", options, mao, function) {
    limit_ = GetOptionInt("limit");
    align_limit_ = GetOptionInt("align_limit");
  }


  // FindMinMaxBB
  //
  // Helper function to find min and max addresses in a loop.
  // Iterate over all basic blocks and compare addresses.
  //
  void FindMinMaxBB(SimpleLoop *loop,
                    MaoEntryIntMap *offsets,
                    BasicBlock    **min_bb,
                    BasicBlock    **max_bb) {
    *min_bb = loop->header();
    *max_bb = *min_bb;
    for (SimpleLoop::BasicBlockSet::const_iterator iter =
           loop->ConstBasicBlockBegin();
         iter != loop->ConstBasicBlockEnd(); ++iter) {
      if ((*offsets)[(*iter)->first_entry()] <
          (*offsets)[(*min_bb)->first_entry()])
        *min_bb = (*iter);
      if ((*offsets)[(*iter)->first_entry()] >
          (*offsets)[(*max_bb)->first_entry()])
        *max_bb = (*iter);
      }
  }


  // Find Candidates for loop alignment. Candidates are two-deep
  // loop nests.
  //
  // Candidates are maintained in a sorted list, sorted by increasing
  // address of the outer-loop header. Later we iterate over this list
  // from top to bottom, knowing that re-relaxation should only affect
  // lower loops.
  //
  // This is actually an oversimplification. Since we're actually
  // insert bytes, we would have to rerun the whole process over and
  // over again, until it reaches a fixed point. Yet, good enough for
  // a start.
  //
  void FindCandidates(SimpleLoop  *loop,
                      MaoEntryIntMap    *offsets,
                      MaoEntryIntMap    *sizes) {
    MAO_ASSERT(offsets);
    MAO_ASSERT(sizes);

    // Find inner loops only
    //
    if (loop->nesting_level() == 1 &&
        loop->NumberOfChildren() == 1 &&
        !loop->is_root()) {               // Outer loop of 2-deep nest
      Trace(0, "Found 2-deep loop nest");

      BasicBlock *inner_min_bb, *inner_max_bb;
      BasicBlock *outer_min_bb, *outer_max_bb;

      FindMinMaxBB(loop, offsets, &outer_min_bb, &outer_max_bb);
      MAO_ASSERT(outer_min_bb);
      MAO_ASSERT(outer_max_bb);
      InstructionEntry *outer_last = outer_max_bb->GetLastInstruction();
      if (!outer_last) {
        Trace(0, "WARNING: Outer Loop: Basic Block with no last instruction found");
        return;
      }

      SimpleLoop::LoopSet::iterator first_child =
        loop->GetChildren()->begin();
      SimpleLoop *inner = (*first_child);
      FindMinMaxBB(inner, offsets, &inner_min_bb, &inner_max_bb);
      MAO_ASSERT(inner_min_bb);
      MAO_ASSERT(inner_max_bb);
      InstructionEntry *inner_last =
        inner_max_bb->GetLastInstruction();
      if (!inner_last) {
        Trace(0, "WARNING: Inner Loop: Basic Block with no last instruction found");
        return;
      }
      if (!inner_last->HasTarget() ||
          !outer_last->HasTarget()) {
        Trace(0, "Unsupported end instructions");
        if (tracing_level() > 0) {
          inner_last->PrintEntry();
          outer_last->PrintEntry();
        }
        return;
      }

      int outer_offset = (*offsets)[outer_last];
      int inner_offset = (*offsets)[inner_last];

      if (outer_offset < inner_offset) {
        Trace(0, "Unexpected control flow, bail");
        return;
          }
      Trace(0, "Offset for back-branches, "
            "inner: %d, outer: %d, %s", inner_offset, outer_offset,
            outer_offset - inner_offset < align_limit_ ?
            "NEED ALIGNMENT" : "TOO FAR");

      if (outer_offset - inner_offset < align_limit_) {
        bool linked_in = false;
        for (LoopList::iterator iter = candidates_.begin();
             iter != candidates_.end(); ++iter) {
          if ((*offsets)[(*iter)->outer_min_bb()->first_entry()] >
              outer_offset) {
            candidates_.insert(
              iter,
              new AlignCandidate(inner, inner_min_bb, inner_max_bb,
                                 loop, outer_min_bb, outer_max_bb));
            linked_in = true;
            break;
          }
        }
        if (!linked_in)
          candidates_.push_back(
            new AlignCandidate(inner, inner_min_bb, inner_max_bb,
                               loop, outer_min_bb, outer_max_bb));
      }

      return;
    }

    // Recursively find inner loops
    //
    for (SimpleLoop::LoopSet::const_iterator liter = loop->ConstChildrenBegin();
         liter != loop->ConstChildrenEnd(); ++liter) {
      FindCandidates(*liter, offsets, sizes);
    }
  }


  // Align Back Branches.
  //
  // After each re-alignment, a new relaxation pass is needed.
  //
  void AlignBackBranches(SimpleLoop  *loop,
                  Function    *function) {
    MAO_ASSERT(function);
    MaoEntryIntMap *sizes, *offsets;

    // Initial relaxation
    //
    sizes = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
    offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());

    // Find candidates, 2-deep loop nests with back-branches closer
    // together than 32 bytes.
    //
    FindCandidates(loop, offsets, sizes);

    // Iterate the sorted list of candidate loop nests.
    // See whether they can/should be aligned.
    //
    // If re-alignment occured, we have to re-relax and
    // check for opportunities at loops with higher
    // addresses.
    //
    for (LoopList::const_iterator iter = candidates_.begin();
         iter != candidates_.end(); ++iter) {
      int inner_offset =
        (*offsets)[(*iter)->inner_max_bb()->GetLastInstruction()];
      int outer_offset =
        (*offsets)[(*iter)->outer_max_bb()->GetLastInstruction()];

      if (inner_offset / align_limit_ !=
          outer_offset / align_limit_) {
        Trace(0, "back-branches are cross-aligned");
        continue;
      }

      if ((*offsets)[(*iter)->outer_min_bb()->GetFirstInstruction()] % 8) {
        (*iter)->outer_min_bb()->first_entry()->AlignTo(3,-1,7);
        MaoRelaxer::InvalidateSizeMap(function_->GetSection());
        sizes = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
        offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());

        inner_offset =
          (*offsets)[(*iter)->inner_max_bb()->GetLastInstruction()];
        outer_offset =
          (*offsets)[(*iter)->outer_max_bb()->GetLastInstruction()];

        Trace(1, "Aligned top of loop nest to 8 byte, offsets: %d, %d",
              inner_offset, outer_offset);

        if (inner_offset / align_limit_ !=
            outer_offset / align_limit_) {
          Trace(0, "Align to 8 did the trick");
          continue;
        }
      }

      // See how far we have to push this loop down...
      int diff = (outer_offset / 32 + 1) * 32 - outer_offset;
      Trace(0, "Inserting %d nops (outer: %d)", diff, outer_offset);
      for (int i = 0; i < diff; i++) {
        InstructionEntry *nop = unit_->CreateNop(function_);
        (*iter)->outer_min_bb()->first_entry()->LinkBefore(nop);
      }

      MaoRelaxer::InvalidateSizeMap(function_->GetSection());
      sizes = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
      offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());

      inner_offset =
        (*offsets)[(*iter)->inner_max_bb()->GetLastInstruction()];
      outer_offset =
        (*offsets)[(*iter)->outer_max_bb()->GetLastInstruction()];
      if (inner_offset / align_limit_ !=
          outer_offset / align_limit_) {
        Trace(0, "Inserting %d nops did the trick, %d, %d",
              diff, inner_offset, outer_offset);
        continue;
      }

      Trace(0, "Failed to cross-align the back branches, %d, %d",
            inner_offset, outer_offset);
    }
  }


  // Main entry point
  //
  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);
    if (!cfg->IsWellFormed()) return true;

    loop_graph_ =  LoopStructureGraph::GetLSG(unit_, function_);
    if (!loop_graph_ ||
        !loop_graph_->NumberOfLoops()) return true;

    AlignBackBranches(loop_graph_->root(), function_);
    return true;
  }

 private:
  LoopStructureGraph *loop_graph_;
  LoopList  candidates_;
  int       limit_;
  int       align_limit_;
};

// External Entry Point
//
void InitBackBranchAlign() {
  RegisterFunctionPass(
      "BACKBRALIGN",
      MaoFunctionPassManager::GenericPassCreator<BackBranchAlign>);
}
