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
MAO_OPTIONS_DEFINE(REDTEST, 0) {
};

class RedTestElimPass : public MaoPass {
 public:
  RedTestElimPass(MaoUnit *mao, const CFG *cfg) :
    MaoPass("REDTEST", mao->mao_options(), MAO_OPTIONS(REDTEST), true),
    mao_(mao), cfg_(cfg) {
  }

  // Find these patterns in a single basic block:
  //
  //   subl     xxx, %r15d
  //   testl    %r15d, %r15d
  //
  //   addl     xxx, %r15d
  //   testl    %r15d, %r15d
  //
  // subl/addl set all the flags that test is testing for.
  // The test instruction is therefore redundant
  //
  void DoElim() {
    FORALL_CFG_BB(cfg_,it) {
      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();

        if ((insn->op() == OP_sub || insn->op() == OP_add) &&
            insn->IsRegisterOperand(1) &&
            insn->nextInstruction()) {
          InstructionEntry *next = insn->nextInstruction();
          if (next->op() == OP_test &&
              next->IsRegisterOperand(0) &&
              next->IsRegisterOperand(1) &&
              next->GetRegisterOperand(0) == next->GetRegisterOperand(1) &&
              next->GetRegisterOperand(0) == insn->GetRegisterOperand(1))
            fprintf(stderr, "*** Found %s/test seq\n",
                    insn->GetOp());
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
void PerformRedundantTestElimination(MaoUnit *mao, const CFG *cfg) {
  RedTestElimPass redtest(mao, cfg);
  redtest.set_timed();
  redtest.DoElim();
}
