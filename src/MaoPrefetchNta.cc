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
MAO_OPTIONS_DEFINE(PREFNTA, 0) {
};

// Insert prefetch.nta before loads
//
class PrefetchNtaPass : public MaoFunctionPass {
 public:
  PrefetchNtaPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("PREFNTA", options, mao, function) {
  }

  // Main entry point
  //
  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);
    if (!cfg->IsWellFormed()) return true;

    FORALL_CFG_BB(cfg,it) {
      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();

        // Look for loads from memory
        //
        if (insn->IsOpMov() &&
            insn->IsMemOperand(0)) {
          InstructionEntry *pref = unit_->CreatePrefetch(function_, 0, insn, 0);
          insn->LinkBefore(pref);
        }
      }
    }

    return true;
  }

};

// External Entry Point
//
void InitPrefetchNta() {
  RegisterFunctionPass(
      "PREFNTA",
      MaoFunctionPassManager::GenericPassCreator<PrefetchNtaPass>);
}
