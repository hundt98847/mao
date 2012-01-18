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

  bool Go() {
    MaoEntryIntMap *offsets;
    std::list <InstructionEntry *> indexed_list[256];

    MaoRelaxer::InvalidateSizeMap(function_->GetSection());
    offsets = MaoRelaxer::GetOffsetMap(unit_, function_->GetSection());

    FORALL_FUNC_ENTRY(function_, entry) {
      if (!entry->IsInstruction()) continue;
      InstructionEntry *insn = entry->AsInstruction();

      // Find loads from mem and find aliases indexed by lower 8 bits
      //
      if (insn->IsPrefetchLoad() &&
          insn->IsMemOperand(0))
        indexed_list[ (*offsets)[insn] % 256].push_back(insn);
    } // FORALL_ENTRY's

    // Iterate over the array and find aliasing entries
    //
    for (int i = 0; i < 256; i++) {
      int n = indexed_list[i].size();
      if (n > 0) {
        Trace(1, "Found %d loads at offset %d%s",
              n, i, n > 1 ? ": ALIAS" : "" );
      }
    }

    // TODO(rhundt): Once we know there are aliases
    //     (and there are many) - Now What?!
    //
    return true;
  }

};

REGISTER_PLUGIN_FUNC_PASS("PREFALIAS", PrefAlias )
} // namespace
