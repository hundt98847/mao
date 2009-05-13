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

class RedTestElimPass : public MaoFunctionPass {
 public:
  RedTestElimPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("REDTEST", options, mao, function) { }

  // Find patterns like these in a single basic block:
  //
  //   subl     xxx, %r15d
  //   ... instructions not setting flags
  //   testl    %r15d, %r15d
  //
  //   addl     xxx, %r15d
  //   ... instructions not setting flags
  //   testl    %r15d, %r15d
  //
  // subl/addl/others set all the flags that test is testing for.
  // The test instruction is therefore redundant
  //
  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);

    std::list<InstructionEntry *> redundants;

    FORALL_CFG_BB(cfg,it) {
      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();

        if (insn->op() == OP_test &&
            insn->IsRegisterOperand(0) &&
            insn->IsRegisterOperand(1) &&
            insn->GetRegisterOperand(0) == insn->GetRegisterOperand(1) &&
            insn->prevInstruction()) {
          InstructionEntry *prev = insn->prevInstruction();

          while (prev && prev->IsOpMov()) {
            // check for re-def's
            if (prev->IsRegisterOperand(1) &&
                RegistersOverlap(prev->GetRegisterOperand(1),
                                 insn->GetRegisterOperand(1))) {
              prev = NULL;
              break;
            }
            prev = prev->prevInstruction();
          }
          if (prev)
            if (prev->op() == OP_sub || prev->op() == OP_add ||
                prev->op() == OP_and || prev->op() == OP_or ||
                prev->op() == OP_xor ||
                prev->op() == OP_sal || prev->op() == OP_sar ||
                prev->op() == OP_shl || prev->op() == OP_shr ||
                prev->op() == OP_sbb) {
              int op_index = prev->NumOperands() > 1 ? 1 : 0;
              if (prev->IsRegisterOperand(op_index) &&
                  prev->GetRegisterOperand(op_index) ==
                  insn->GetRegisterOperand(0)) {
                redundants.push_back(insn);

                Trace(1, "Found %s/test seq", prev->op_str());
                if (tracing_level() > 0)
                  (*it)->Print(stderr, prev,insn);
              }
            }
	}
      }
    }

    // Now delete all the redundant ones.
    for (std::list<InstructionEntry *>::iterator it = redundants.begin();
         it != redundants.end(); ++it) {
      unit_->DeleteEntry(*it);
    }
    return true;
  }
};


// External Entry Point
//
void InitRedundantTestElimination() {
  RegisterFunctionPass(
      "REDTEST",
      MaoFunctionPassManager::GenericPassCreator<RedTestElimPass>);
}
