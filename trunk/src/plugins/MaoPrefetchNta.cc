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

// Insert prefetch.nta hints before every load.
// The idea is to keep data out of the caches as
// much as possible.
//
#include "Mao.h"
#include <map>

namespace {

PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(PREFNTA, "Inserts prefetches before loads and stores", 2) {
  OPTION_INT("offset",  0, "Offset added to prefetch addresses"),
  OPTION_INT("ptype",   0, "Type of prefetch (0: nta, ..., 3: t2)"),
};

// Insert prefetch.nta before loads and stores
//
class PrefetchNtaPass : public MaoFunctionPass {
 public:
  PrefetchNtaPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
    : MaoFunctionPass("PREFNTA", options, mao, function),
      insertions_(0) {
    offset_ = GetOptionInt("offset");
    ptype_ = GetOptionInt("ptype");
  }

  // Main entry point
  //
  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);
    if (!cfg->IsWellFormed()) {
      Trace(3, "Function \"%s\" does not have a well formed CFG",
            function_->name().c_str() );
      return true;
    }

    FORALL_CFG_BB(cfg,it) {
      FORALL_BB_ENTRY(it,entry) {
        if (!entry->IsInstruction()) continue;
        InstructionEntry *insn = entry->AsInstruction();

        // Look for loads and stores from memory
        //
        if (insn->NumOperands()>1 &&
            insn->IsMemOperand(0)) {
          InstructionEntry *pref = unit_->CreatePrefetch(function_,
                                                         ptype_,
                                                         insn,
                                                         0, /* operand */
                                                         offset_);
          insn->LinkBefore(pref);
          ++insertions_;
        }
        if (insn->NumOperands()>1 &&
            insn->IsMemOperand(1)) {
          InstructionEntry *pref = unit_->CreatePrefetch(function_,
                                                         ptype_,
                                                         insn,
                                                         1, /* operand */
                                                         offset_);
          insn->LinkBefore(pref);
          ++insertions_;
        }
      }
    }

    // Provide simple log message to indicate progress
    //
    if (insertions_) {
      const char *pstring[] = {
        "nta", "t0", "t1", "t2"
      };

      Trace(1, "Inserted %d prefetch%s's, offset: %d",
            insertions_, pstring[ptype_], offset_);
    }
    return true;
  }

 private:
  int offset_;
  int ptype_;
  int insertions_;
};


REGISTER_PLUGIN_FUNC_PASS("PREFNTA", PrefetchNtaPass)
}  // namespace
