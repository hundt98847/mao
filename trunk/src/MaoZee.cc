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
MAO_OPTIONS_DEFINE(ZEE, 1) {
  OPTION_BOOL("vcg", false, "Dump VCG file"),
};

class ZeroExtentElimPass : public MaoPass {
 public:
  ZeroExtentElimPass(MaoUnit *mao, const CFG *cfg) :
    MaoPass("ZEE", mao->mao_options(), MAO_OPTIONS(ZEE), true),
    mao_(mao), cfg_(cfg) {
    dump_vcg_ = GetOptionBool("vcg");
  }

  // Redundant zero extend elimination. Find pattern:
  //     movl reg32, same-reg32
  //
  // then search in same basic block for a sign
  // extending def reg32
  //
  void DoElim() {
    FORALL_CFG_BB(cfg_,it) {
      FORALL_BB_ENTRY(it,entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();

        if (insn->IsOpMov() &&
            insn->IsRegister32Operand(0) &&
            insn->IsRegister32Operand(1) &&
            insn->GetRegisterOperand(0) == insn->GetRegisterOperand(1)) {
          bool foundTrivalCase = false;

          InstructionEntry *prev = insn->prevInstruction();
          if (prev) {
            unsigned long long pmask = GetRegisterDefMask(prev);
            if (pmask && pmask != REG_ALL) {
              unsigned long long imask = GetRegisterDefMask(insn);
              if (imask == pmask) {
                Trace(1, "Found redundant zero-extend:");
                if (tracing_level() > 0) {
                  prev->PrintEntry(stderr);
                  insn->PrintEntry(stderr);
                }
                foundTrivalCase = true;
              }
            }
          }
          if (!foundTrivalCase) {
            Trace(1, "Found non-trivial zero-extent:");
            if (tracing_level() > 0) {
              insn->PrintEntry(stderr);
            }
          }
        }
      }
    }
  }

 private:
  MaoUnit   *mao_;
  const CFG *cfg_;
  bool       dump_vcg_ :1 ;
};


// External Entry Point
//
void PerformZeroExtensionElimination(MaoUnit *mao, const CFG *cfg) {
  ZeroExtentElimPass zee(mao, cfg);
  zee.set_timed();
  zee.DoElim();
}
