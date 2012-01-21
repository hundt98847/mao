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
#include "Mao.h"

namespace {

PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(MISSDISP, "A peephole optimization pass to find an add "\
                   "followed by a move without displacement", 0) {
};

class MissDispElimPass : public MaoFunctionPass {
 public:
  MissDispElimPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("MISSDISP", options, mao, function) { }

  // Find these patterns in a single basic block:
  //    add    $0x8,%rax
  //    mov    (%rax),%rax
  //
  // can be replaced by:
  //
  //   mov    0x8(%rax),%rax
  //
  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);

    FORALL_CFG_BB(cfg,it) {
      FORALL_BB_ENTRY(it,entry) {
        if (!entry->IsInstruction()) continue;
        InstructionEntry *insn = entry->AsInstruction();

        if (insn->op() == OP_add &&
            insn->IsImmediateOperand(0) &&
            insn->IsRegisterOperand(1) &&
            insn->nextInstruction()) {
          InstructionEntry *next = insn->nextInstruction();
          if (next->IsOpMov() &&
              next->IsRegisterOperand(1) &&
              next->IsMemOperand(0) &&
              next->GetBaseRegister() &&
              next->GetBaseRegister() == next->GetRegisterOperand(1) &&
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
    return true;
  }
};

REGISTER_PLUGIN_FUNC_PASS("MISSDISP", MissDispElimPass)
}  // namespace
