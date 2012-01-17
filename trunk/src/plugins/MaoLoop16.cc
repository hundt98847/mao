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
// iteration. This seems to cause an 9% degradation
// on SPEC 2000 252.eon with gcc 4.4 over gcc 4.2.1
//
#include "Mao.h"
#include <map>

namespace {
PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(LOOP16, "Aligns short loops at 16 byte boundaries", 3) {
  OPTION_INT("max_fetch_lines",  2,
             "Seek to align loops with size <= max_fetch_lines*fetchline_size"),
  OPTION_INT("fetch_line_size", 16, "Fetchline size"),
  OPTION_INT("limit", -1, "Limit tranformation invocations")
};

// --------------------------------------------------------------------
// Align Tiny Loops to 16 Bytes
// --------------------------------------------------------------------
class AlignTinyLoops16 : public MaoFunctionPass {

  // Helper data structure to maintain candidate loops
  //
  class AlignCandidate {
   public:
    AlignCandidate(const SimpleLoop *loop,
                   const BasicBlock *min_bb,
                   const BasicBlock *max_bb) :
      loop_(loop), min_bb_(min_bb), max_bb_(max_bb) {}

    const BasicBlock *min_bb() { return min_bb_; }
    const BasicBlock *max_bb() { return max_bb_; }
    const SimpleLoop *loop()   { return loop_; }
   protected:
    const SimpleLoop *loop_;
    const BasicBlock *min_bb_, *max_bb_;
  };
  typedef std::list<AlignCandidate *> LoopList;

 public:
  AlignTinyLoops16(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("LOOP16", options, mao, function) {
    fetchline_size_  = GetOptionInt("fetch_line_size");
    max_fetch_lines_ = GetOptionInt("max_fetch_lines");
    limit_ = GetOptionInt("limit");
  }

  // Find Candidates for loop alignment. Candidates are all
  // very short loops, basically all loops with size < max_loop_size
  //
  // Candidates are maintained in a sorted list, sorted by increasing
  // address. Later we iterate over this list from top to bottom,
  // knowing that re-relaxation should only affect lower loops.
  //
  // This is actually an oversimplification. If we would actually
  // insert bytes, we would have to rerun the whole process over and
  // over again until it reaches a fixed points. However, we're
  // not inserting bytes, but .p2align directives, which should ensure
  // that the candidate inner loops remain - at least - aligned.
  //
  void FindCandidates(const SimpleLoop  *loop,
                      MaoEntryIntMap    *offsets,
                      MaoEntryIntMap    *sizes) {
    MAO_ASSERT(offsets);
    MAO_ASSERT(sizes);

    // Find inner loops only
    //
    if (!loop->nesting_level() &&   // Leaf node = Inner loop
        !loop->is_root()) {         // not root
      MAO_ASSERT(loop->NumberOfChildren() == 0);
      MAO_ASSERT(loop->bottom());

      // Determine basic block with lowest and highest addresses.
      // CFGs and loops can have the weirdest shapes, so we have
      // to do this explicit search.
      //
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

      // Compute start and end address of loop
      //
      int end_off = (*offsets)[max_bb->last_entry()] +
                    (*sizes)[max_bb->last_entry()];
      int start_off   = (*offsets)[min_bb->first_entry()];
      int size = end_off - start_off;

      // Add this loop to list of candidates if it passes the
      // filter. Sort by starting offset.
      //
      if (size <= max_fetch_lines_*fetchline_size_) {
        bool linked_in = false;
        for (LoopList::iterator iter = candidates_.begin();
             iter != candidates_.end(); ++iter) {
          if ((*offsets)[(*iter)->min_bb()->first_entry()] > start_off) {
            candidates_.insert(iter, new AlignCandidate(loop, min_bb, max_bb));
            linked_in = true;
            break;
          }
        }
        if (!linked_in)
          candidates_.push_back(new AlignCandidate(loop, min_bb, max_bb));
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

    // Initial relaxation
    //
    sizes = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
    offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());

    // Find candidates, inner loops
    //
    FindCandidates(loop, offsets, sizes);

    // Iterate the sorted list of loop candidates.
    // If a loop is re-aligned, we have to re-relax and
    // check for opportunities at loops with higher
    // addresses.
    //
    for (LoopList::iterator iter = candidates_.begin();
         iter != candidates_.end(); ++iter) {
      int end_off = (*offsets)[(*iter)->max_bb()->last_entry()] +
                    (*sizes)[(*iter)->max_bb()->last_entry()];
      int start_off   = (*offsets)[(*iter)->min_bb()->first_entry()];
      int size = end_off - start_off;

      int start_fetch = start_off / fetchline_size_;
      int start_used  = fetchline_size_ - start_off % fetchline_size_;
      int end_fetch   = end_off / fetchline_size_;
      int end_used    = end_off % fetchline_size_;
      int lines       = end_fetch - start_fetch + 1;

      // Report on all inner loops
      //
      Trace(2, "func-%d, loop-%d, size: %d, start: %d, end: %d, %d fetch lines",
            function_->id(),
            (*iter)->loop()->counter(),
            size, start_off, end_off, lines);
      Trace(2, "  Fetch line %d bytes used, end: %d bytes used",
            start_used, end_used);

      // Found an inner loop where alignment would save a
      // fetchline. For this, there must be more bytes available
      // at the end of the bottom fetchline then there are used
      // in the top fetchline.
      //
      //   |0123456789012345|
      //   |.........BBBBBBB|   these bytes are used by loop
      //   |XXXXXXXXXXXXXXXX|*  any number of filled lines
      //   |EEEEEEE---------|   there need to be more -'s than B's
      //
      if (lines > 1 && start_used < (fetchline_size_ - end_used)) {
        Trace(0, "  -> Alignment possible, up %d bytes, save 1/%d fetch lines",
              start_used,
              end_fetch - start_fetch + 1);

        // These are the simplistic heuristics.
        //
        // It is expected that for loops that are longer than
        // 1 fetchline, the instruction decoding will no longer
        // be the bottleneck, as some of the instructions in the
        // loop will have some latency.
        //
        // Subject to further tuning.
        //
        if (lines <= max_fetch_lines_) {
          Trace(0, "  -> Alignment DONE");
          (*iter)->min_bb()->first_entry()->AlignTo(4,-1,15);

          // After alignment, we have to re-relax in order to
          // check how alignment changed for loops at higher
          // addresses (after this current loop in the list)
          //
          MaoRelaxer::InvalidateSizeMap(function_->GetSection());
          sizes = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
          offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());
        }
        else {
          Trace(0, "  -> no transformation, limit is %d lines",
                max_fetch_lines_);
        }
      }
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

    AlignInner(loop_graph_->root(), function_);
    return true;
  }

 private:
  LoopStructureGraph *loop_graph_;
  LoopList  candidates_;
  int       fetchline_size_;
  int       max_fetch_lines_;
  int       limit_;
};

REGISTER_PLUGIN_FUNC_PASS("LOOP16", AlignTinyLoops16)
}  // namespace
