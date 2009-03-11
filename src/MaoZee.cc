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

  bool IsZeroExtent(InstructionEntry *insn) {
    if (insn->IsOpMov() &&
        insn->IsRegister32Operand(0) &&
        insn->IsRegister32Operand(1) &&
        insn->GetRegisterOperand(0) == insn->GetRegisterOperand(1))
      return true;
    return false;
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
      MaoEntry *first = (*it)->GetFirstInstruction();
      if (!first) continue;

      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();
        if (first == insn) continue;

        if (IsZeroExtent(insn)) {
          BitString imask = GetRegisterDefMask(insn);
          InstructionEntry *prev = insn->prevInstruction();
          while (prev) {
            BitString pmask = GetRegisterDefMask(prev);
            if (pmask.IsUndef())  // insn with unknown side effects, break
              break;

            if (RegistersContained(pmask, imask)) {  // def and use match
              if (prev->IsPredicated() ||  // bail on cmoves...
                  prev->op() == OP_bswap ||
                  prev->op() == OP_call  ||
                  prev->op() == OP_lcall)  // bail on these, don't understand em
                break;

              Trace(1, "Found redundant zero-extend:");
              if (tracing_level() > 0)
                (*it)->Print(stderr, prev, insn);
              redundants.push_back(insn);
              break;
            }

            BitString ip = imask & pmask;
            if (ip.IsNonNull()) {
              if (prev->op() == OP_movq &&
                  RegistersOverlap(prev->GetRegisterOperand(1),
                                   insn->GetRegisterOperand(1))) {
                Trace(1, "Overlap");
                (*it)->Print(stderr, prev, insn);
              }
              break; // some matching register parts are define
            }

            if (prev == first)  // reached top of basic block
              break;
            prev = prev->prevInstruction();
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
