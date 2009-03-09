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
MAO_OPTIONS_DEFINE(ZEE, 0) {
};

class ZeroExtentElimPass : public MaoPass {
 public:
  ZeroExtentElimPass(MaoUnit *mao, const CFG *cfg) :
    MaoPass("ZEE", mao->mao_options(), MAO_OPTIONS(ZEE), false, cfg),
    mao_(mao) {
  }

  // Redundant zero extend elimination. Find pattern:
  //     movl reg32, same-reg32
  //
  // then search in same basic block for a sign
  // extending def reg32
  //
  void DoElim() {
    if (!enabled()) return;
    std::list<InstructionEntry *> redundants;

    FORALL_CFG_BB(cfg(),it) {
      MaoEntry *first = NULL;
      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();
        if (!first) {
          first = (*entry);
          continue;
        }

        if (insn->IsOpMov() &&
            insn->IsRegister32Operand(0) &&
            insn->IsRegister32Operand(1) &&
            insn->GetRegisterOperand(0) == insn->GetRegisterOperand(1)) {
          unsigned long long imask = GetRegisterDefMask(insn);
          InstructionEntry *prev = insn->prevInstruction();
          while (prev) {
            unsigned long long pmask = GetRegisterDefMask(prev);
            if (pmask == REG_ALL)  // insn with unknown side effects, stop
              break;

            if (pmask) {
              if (imask == pmask) {
                Trace(1, "Found redundant zero-extend:");
                if (tracing_level() > 0) {
                  MaoEntry *curr = prev;
                  while (curr != insn) {
                    curr->PrintEntry(stderr);
                    curr = curr->next();
                  }
                  fprintf(stderr, "  -->");
                  insn->PrintEntry(stderr);
                }  // tracing
                redundants.push_back(insn);
                break;
              }  // def and use match
              if (imask & pmask)  // some matching register parts are define
                break;
            }  // found mask for current instruction, going upward
            prev = prev->prevInstruction();
            if (prev == first)  // reached top of basic block
              break;
          }  // while previous instructions
        }  // Starting from a sign-extend move
      }  // Entries
    }  // BB

    // Now delete all the redundant ones.
    for (std::list<InstructionEntry *>::iterator it = redundants.begin();
         it != redundants.end(); ++it) {
      mao_->DeleteEntry(*it);
    }
  }

 private:
  MaoUnit   *mao_;
};


// External Entry Point
//
void PerformZeroExtensionElimination(MaoUnit *mao, const CFG *cfg) {
  ZeroExtentElimPass zee(mao, cfg);
  zee.set_timed();
  zee.DoElim();
}
