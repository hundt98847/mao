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

// Zero Extension Elimination
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(ADDADD, 0) {
};

class AddAddElimPass : public MaoPass {
 public:
  AddAddElimPass(MaoUnit *mao, Function *function) :
    MaoPass("ADDADD", mao->mao_options(), MAO_OPTIONS(ADDADD), false),
    mao_(mao), function_(function) {
  }

  bool IsAddI(InstructionEntry *insn) {
    return ((insn->op() == OP_add || insn->op() == OP_sub)  &&
            insn->IsImmediateOperand(0) &&
            insn->IsRegisterOperand(1));
  }

  // Add add  pattern finder:
  //     add/sub rX, IMM1
  //     add/sub rX, IMM2
  void DoElim() {
    set_cfg(CFG::GetCFG(mao_, function_));

    FORALL_CFG_BB(cfg(),it) {
      MaoEntry *first = (*it)->GetFirstInstruction();
      if (!first) continue;

      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();
        if (first == insn) continue;

        if (IsAddI(insn)) {
          BitString imask = GetRegisterDefMask(insn);

          InstructionEntry *prev = insn->prevInstruction();
          while (prev) {
            BitString pmask = GetRegisterDefMask(prev);
            if (pmask.IsUndef()) {  // insn with unknown side effects, break
              break;
            }

            // if this is an add and the same registers..
            // then we can fix this.
            if (IsAddI(prev)) {
              if (insn->GetRegisterOperand(1) == prev->GetRegisterOperand(1)) {
                Trace(1, "Found two immediate adds");
                if (tracing_level() >= 1) {
                  (*it)->Print(stderr, prev, insn);
                }
                break;
              }
            }

            // Make sure there is no def conflict
            if (!(pmask & imask).IsNull()) {
              break;
            }

            if (prev->IsPredicated() ||  // bail on cmoves...
                prev->op() == OP_bswap ||
                prev->op() == OP_call  ||
                prev->op() == OP_lcall) { // bail on these, don't understand em
              break;
            }
            if (prev == first)  // reached top of basic block
              break;
            prev = prev->prevInstruction();
          } // prev
        } // IsAdd()
      }  // Entries
    }  // BB
  }

 private:
  MaoUnit   *mao_;
  Function  *function_;
};


// External Entry Point
//
void PerformAddAddElimination(MaoUnit *mao, Function *function) {
  AddAddElimPass addadd(mao, function);
  if (addadd.enabled()) {
    addadd.set_timed();
    addadd.DoElim();
  }
}
