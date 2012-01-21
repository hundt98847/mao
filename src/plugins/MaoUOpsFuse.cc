//
// Copyright 2012 Google Inc.
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

// Search for various cases where u-ops fusion is prevented
// for one reason or the other.
//
// Case 1:
//    cmp
//    cond-jump
//
//    can be fused, but not if the instructions cross a cache-line.
//
// Solution:
//    push cmp down with nops (or push BB down - TBD)
//
#include "Mao.h"
#include <list>

namespace {
PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(UOPSCMPJMP, "Enable fusion of cmp/cond-jump in case "
                   "they overlap cache line boundary", 3)
{
  OPTION_INT("cache_line_size", 32, "Cacheline size"),
  OPTION_INT("offset_min", 30, "If cmp insn start at this offset or higher, "
             "align it to the next cache lines via nops."),
  OPTION_BOOL("align_cmp", false, "If set to true, insert nops right in front"
              " of the cmp insn. If set to false, the pass will seek to"
              " align down the full BB")
};

// --------------------------------------------------------------------
// Pass
// --------------------------------------------------------------------
class UOpsCmpJmp : public MaoFunctionPass {
 public:
  UOpsCmpJmp(MaoOptionMap *options, MaoUnit *mao, Function *function)
    : MaoFunctionPass("UOPSCMPJMP", options, mao, function) {
    cacheline_size_  = GetOptionInt("cache_line_size");
    offset_min_ = GetOptionInt("offset_min");
    align_cmp_ = GetOptionBool("align_cmp");
  }

  // Look for these patterns:
  //     cmp
  //     cond-jump
  // and see whether the instructions cross a cache-line boundary
  //
  bool Go() {
    MaoEntryIntMap *sizes, *offsets;
    bool changed;  // iterate until no more changes happened.
    CFG *cfg = CFG::GetCFG(unit_, function_);

    // Relax and compute offsets
    //
    MaoRelaxer::InvalidateSizeMap(function_->GetSection());
    sizes = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
    offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());

    // Sort BB's by offset, as we want to go from to bottom,
    // finding patterns, and inserting nops
    //
    std::list<BasicBlock*> bb_list;

    FORALL_CFG_BB(cfg,it) {
      if (!bb_list.size())
      {
        bb_list.push_back(*it);
        continue;
      }
      bool inserted = false;
      for (std::list<BasicBlock*>::iterator iter = bb_list.begin();
           iter != bb_list.end(); ++iter) {
        if ((*offsets)[(*it)->first_entry()] <
            (*offsets)[(*iter)->first_entry()]) {
          bb_list.insert( iter, *it );
          inserted = true;
          break;
        }
      }
      if (!inserted)
        bb_list.push_back(*it);
    }

    // Now repeatadly iterate over the BB's in program order
    // and find and align patterns.
    //
    do {
      changed = false;

      // Relax and compute offsets
      //
      MaoRelaxer::InvalidateSizeMap(function_->GetSection());
      sizes = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
      offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());

      for (std::list<BasicBlock*>::iterator iter = bb_list.begin();
           iter != bb_list.end(); ++iter) {
        FORALL_BB_ENTRY(iter,entry) {
          if (!entry->IsInstruction()) continue;
          InstructionEntry *insn = entry->AsInstruction();

          // Find the cmp/cond-jump pattern
          //
          if (insn->op() == OP_cmp &&
              insn->next() &&
              insn->next()->IsInstruction() &&
              insn->next()->AsInstruction()->IsCondJump()) {
            InstructionEntry *n = insn->next()->AsInstruction();

            int start  = (*offsets)[insn];
            int size   = (*sizes)[insn] + (*sizes)[n];
            int offset = start % cacheline_size_;
            Trace(1, "Found CMP/JMP, at: %d (offset: %d), size: %d%s",
                  start, offset, size,
                  offset + size > cacheline_size_ ?
                  ": Crossing cacheline" : "");

            // If the beginning of the cmp instruction is close
            // enough to the cacheline boundary, we insert nops
            // to push this instruction down.
            //
            // This changes the IR, we set 'changed' to true and
            // iterate one more time.
            //
            if (offset >= offset_min_ &&
                offset + size > cacheline_size_) {
              changed = true;
              MaoEntry *insert = insn;
              if (!align_cmp_) {
                // Critical step:
                // We want to insert nops as high as possible in
                // or before this basic block. However, we also don't
                // want to cross any align directives or any other
                // directives. So we search upwards and pass by
                // insn and labels.
                //
                while (insert->prev() &&
                       (insert->prev()->IsInstruction() ||
                        insert->prev()->IsLabel()) &&
                       !(insert->prev()->IsInstruction() &&
                         insert->prev()->AsInstruction()->HasTarget()))
                  insert = insert->prev();
              }

              Trace(1, "Insert Nops Before:");
              if (tracing_level() >= 1)
                insert->PrintEntry(stderr);

              for (int i = offset; i < cacheline_size_; i++) {
                MaoEntry *nop = unit_->CreateNop(function_);
                insert->LinkBefore(nop);
              }
            }
          }
        } // FORALL_ENTRY's
      } // FORALL_BB
    } while(changed); // iterate until we're done and nothing's changed.

    return true;
  }

 private:
  int cacheline_size_;
  int offset_min_;
  bool align_cmp_;
};

REGISTER_PLUGIN_FUNC_PASS("UOPSCMPJMP", UOpsCmpJmp )
} // namespace
