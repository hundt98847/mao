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

// nop killer
//
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(NOPKILL, 0) {
};

class NopKillerElimPass : public MaoPass {
 public:
  NopKillerElimPass(MaoUnit *mao, const CFG *cfg) :
    MaoPass("NOPKILL", mao->mao_options(), MAO_OPTIONS(NOPKILL), false),
    mao_(mao), cfg_(cfg) {
  }

  // Find these patterns in a single basic block:
  //
  //   nop
  //   nopl
  //   xchg %ax, %ax
  //   xchg %eax, %eax
  //   xchg %rax, %rax
  //   .p2align ...
  //
  // and kill them all
  //
  void DoElim() {
    if (!enabled()) return;
    std::list<MaoEntry *> redundants;

    FORALL_CFG_BB(cfg_,it) {
      FORALL_BB_ENTRY(it,entry) {
        if ((*entry)->IsInstruction()) {
          InstructionEntry *insn = (*entry)->AsInstruction();

          if (insn->op() == OP_nop) {
            redundants.push_back(insn);
            continue;
          }

          if (insn->op() == OP_xchg &&
              insn->IsRegisterOperand(0) &&
              insn->IsRegisterOperand(1) &&
              insn->GetRegisterOperand(0) == insn->GetRegisterOperand(1)) {
            redundants.push_back(insn);
            continue;
          }
        }

        if ((*entry)->IsDirective()) {
          DirectiveEntry *insn = (*entry)->AsDirective();
          if (insn->op() == DirectiveEntry::P2ALIGN) {
            redundants.push_back(insn);
            continue;
          }
        }
      }
    }

    // Now delete all the redundant ones.
    for (std::list<MaoEntry *>::iterator it = redundants.begin();
         it != redundants.end(); ++it) {
      Trace(1, "Removing nop");
      if (tracing_level() > 0)
        (*it)->PrintEntry(stderr);
      mao_->DeleteEntry(*it);
    }
  }

 private:
  MaoUnit   *mao_;
  const CFG *cfg_;
};


// External Entry Point
//

void PerformNopKiller(MaoUnit *mao, const CFG *cfg) {
  NopKillerElimPass nopkill(mao, cfg);
  nopkill.set_timed();
  nopkill.DoElim();
}
