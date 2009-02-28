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

// Redundant Test Elimination
//
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(REDMEMMOV, 0) {
};

class RedMemMovElimPass : public MaoPass {
 public:
  RedMemMovElimPass(MaoUnit *mao, const CFG *cfg) :
    MaoPass("REDMEMMOV", mao->mao_options(), MAO_OPTIONS(REDMEMMOV), true),
    mao_(mao), cfg_(cfg) {
  }

  // Find these patterns in a single basic block:
  //
  //  movq    24(%rsp), %rdx
  //  ... no def for that memory (check 5 instructions)
  //  movq    24(%rsp), %rcx
  //
  void DoElim() {
    FORALL_CFG_BB(cfg_,it) {
      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();
//        insn->PrintEntry(stderr);

        if (insn->IsOpMov() &&
            insn->IsRegisterOperand(1) &&
            insn->IsMemOperand(0)) {
          int checked = 0;
          unsigned long long mask = GetRegisterDefMask(insn);
          mask |= GetMaskForRegister(insn->GetBaseRegister());
          mask |= GetMaskForRegister(insn->GetIndexRegister());

//          fprintf(stderr, "\t\t");
//          PrintRegisterDefMask(mask, stderr);
//          fprintf(stderr, "\n");

          InstructionEntry *next = insn->nextInstruction();
          while (checked < 5 && next) {
            if (next->IsControlTransfer() ||
                next->IsCall() ||
                next->IsReturn())
              break;
            unsigned long long defs = GetRegisterDefMask(next);
//            next->PrintEntry(stderr);
//            fprintf(stderr, "\t\t");
//            PrintRegisterDefMask(defs, stderr);
//            fprintf(stderr, "\n");

            if (defs == 0LL || defs == REG_ALL)
              break;  // defines something other than registers

            if (next->IsOpMov() &&
                next->IsRegisterOperand(1) &&
                next->IsMemOperand(0)) {
              // now we have a second movl mem, reg
              // need to check whether two mem operands are the same.
              if (insn->CompareMemOperand(0, next, 0)) {
                fprintf(stderr, "*** Found two insns with same mem op\n");
                insn->PrintEntry(stderr);
                insn = insn->nextInstruction();
                while (insn != next) {
                  insn->PrintEntry(stderr);
                  insn = insn->nextInstruction();
                }
                next->PrintEntry(stderr);
              }
            }
            if (defs & mask)
              break;  // target register gets redefined.

            ++checked;
            next = next->nextInstruction();
          }
        }
      }
    }
  }

 private:
  MaoUnit   *mao_;
  const CFG *cfg_;
};


// External Entry Point
//
void PerformRedundantMemMoveElimination(MaoUnit *mao, const CFG *cfg) {
  RedMemMovElimPass redmemmov(mao, cfg);
  redmemmov.set_timed();
  redmemmov.DoElim();
}
