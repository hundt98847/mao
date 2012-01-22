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

#include "Mao.h"
#include <list>

// This pass addresses prefetcher load aliasing
//
// From http://www.intel.com/technology/itj/2007/v11i4/1-inside/7-code-gen.htm
//
// Intel Core micro-architecture features a data prefetcher to speculatively
// load data into the caches. The L2 to L1 cache prefetcher uses a 256-entry
// table to map loads to load address predictors. This table is indexed by
// the lower eight bits of the instruction pointer (IP) address of the load.
// Since there is only one table entry per index, two loads offset by a
// multiple of 256 bytes cannot both reside in the table. If a conflict
// occurs in a loop and involves a predictable load, the effectiveness of
// the data prefetcher can be drastically reduced. In a critical loop, this
// can cause a significant reduction in overall application performance.
//
namespace {
PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(PREFALIAS, "Find loads that might alias in the "
                   "prefetcher tables ", 0)
{};

// --------------------------------------------------------------------
// Pass
// --------------------------------------------------------------------
class PrefAlias : public MaoFunctionPass {
 public:
  PrefAlias(MaoOptionMap *options, MaoUnit *mao, Function *function)
    : MaoFunctionPass("PREFALIAS", options, mao, function) {
  }

  struct Load {
    int level;
    InstructionEntry *insn;
    Load(int l, InstructionEntry *i) :
      level(l), insn(i) {}
  };

  void ScanLoop(const SimpleLoop  *loop,
                MaoEntryIntMap    *offsets,
                int                level) {
    MAO_ASSERT(offsets);
    MAO_ASSERT(loop);

    if (!loop->is_root()) {         // not root
      // Iterate over Basic Blocks, populate alias map
      //
      for (SimpleLoop::BasicBlockSet::const_iterator iter =
             loop->ConstBasicBlockBegin();
           iter != loop->ConstBasicBlockEnd(); ++iter) {
        bb_map[(*iter)] = true;
        FORALL_BB_ENTRY(iter, entry) {
          if (!entry->IsInstruction()) continue;
          InstructionEntry *insn = entry->AsInstruction();

          // Find loads from mem and find aliases indexed by lower 8 bits
          //
          if (insn->IsPrefetchLoad() &&
              insn->IsMemOperand(0))
            indexed_list[ (*offsets)[insn] % 256].push_back(
              new Load(level, insn));
        }
      }
    }

    // Recursively find inner loops
    //
    for (SimpleLoop::LoopSet::const_iterator liter = loop->ConstChildrenBegin();
         liter != loop->ConstChildrenEnd(); ++liter) {
      ScanLoop(*liter, offsets, level+1);
    }
  }

  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);
    if (!cfg->IsWellFormed()) return true;

    MaoEntryIntMap *offsets;
    MaoRelaxer::InvalidateSizeMap(function_->GetSection());
    offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());

    FORALL_CFG_BB(cfg, it)
      bb_map[(*it)] = false;

    LoopStructureGraph *loop_graph = LoopStructureGraph::GetLSG(unit_, function_);
    if (loop_graph && loop_graph->NumberOfLoops())
      ScanLoop(loop_graph->root(), offsets, 0);

    // pick up all other, non-loop BB's
    FORALL_CFG_BB(cfg, it) {
      if (bb_map[(*it)]) continue;
      FORALL_BB_ENTRY(it, iter) {
        if (!iter->IsInstruction()) continue;
        InstructionEntry *insn = iter->AsInstruction();

        // Find loads from mem and find aliases indexed by lower 8 bits
        //
        if (insn->IsPrefetchLoad() &&
            insn->IsMemOperand(0))
          indexed_list[ (*offsets)[insn] % 256].push_back(
            new Load(0, insn));
      } // FORALL_ENTRY's
    }


    // Iterate over the alias array and find aliasing entries
    //
    for (int i = 0; i < 256; i++) {
      int n = indexed_list[i].size();
      if (n > 0) {
        Trace(1, "Found %d loads at offset %d%s",
              n, i, n > 1 ? ": ALIAS" : "" );
      }
      if (n > 1) {
        for (std::list<Load *>::iterator iter = indexed_list[i].begin();
             iter != indexed_list[i].end(); ++iter) {
          TraceC(1, "  Level: %d ", (*iter)->level);
          (*iter)->insn->PrintEntry(stderr);
          //if ((*iter)->level > 3) {
          //  InstructionEntry *nop = unit_->CreateNop(function_);
          //  (*iter)->insn->LinkBefore(nop);
          // }
        }
      }
    }

    // TODO(rhundt): Once we know there are aliases
    //     (and there are many) - Now What?!
    //
    return true;
  }
 private:
  std::list<Load *> indexed_list[256];
  std::map<BasicBlock *, bool> bb_map;
};

REGISTER_PLUGIN_FUNC_PASS("PREFALIAS", PrefAlias )
} // namespace
