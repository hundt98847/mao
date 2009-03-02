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

// Missing Displacement Optimization
//
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(MISSDISP, 0) {
};

class MissDispElimPass : public MaoPass {
 public:
  MissDispElimPass(MaoUnit *mao, const CFG *cfg) :
    MaoPass("MISSDISP", mao->mao_options(), MAO_OPTIONS(MISSDISP), true),
    mao_(mao), cfg_(cfg) {
  }

  // Find these patterns in a single basic block:
  //    add    $0x8,%rax
  //    mov    (%rax),%rax
  //
  // can be replaced by:
  //
  //   mov    0x8(%rax),%rax
  //
  void DoElim() {
    FORALL_CFG_BB(cfg_,it) {
      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();

        if (insn->op() == OP_add &&
            insn->IsImmediateOperand(0) &&
            insn->IsRegisterOperand(1) &&
            insn->nextInstruction()) {
          InstructionEntry *next = insn->nextInstruction();
          if (next->IsOpMov() &&
              next->IsRegisterOperand(1) &&
              next->IsMemOperand(0) &&
              !strcmp(next->GetBaseRegister(), next->GetRegisterOperand(1)) &&
              next->GetRegisterOperand(1) == insn->GetRegisterOperand(1)) {
            Trace(1, "Found missing disp");
            if (tracing_level() > 0) {
              insn->PrintEntry(stderr);
              next->PrintEntry(stderr);
            }
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
void PerformMissDispElimination(MaoUnit *mao, const CFG *cfg) {
  MissDispElimPass missdisp(mao, cfg);
  missdisp.set_timed();
  missdisp.DoElim();
}
