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
#include "Mao.h"

namespace {
PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(NOPKILL, "Removes all NOPs in the code", 0) {
};

// --------------------------------------------------------------------
// Pass
// --------------------------------------------------------------------
class NopKillerElimPass : public MaoFunctionPass {
 public:
  NopKillerElimPass(MaoOptionMap *options, MaoUnit *mao, Function *func)
      : MaoFunctionPass("NOPKILL", options, mao, func) { }

  // Find these patterns in a function:
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
  bool Go() {
    int total = 0;
    FORALL_FUNC_ENTRY(function_, iter) {
      if (iter->IsInstruction()) {
        InstructionEntry *insn = iter->AsInstruction();

        if (insn->op() == OP_nop) {
          MarkInsnForDelete(insn);
          total++;
          continue;
        }

        if (insn->op() == OP_xchg &&
            insn->IsRegisterOperand(0) &&
            insn->IsRegisterOperand(1) &&
            insn->GetRegisterOperand(0) == insn->GetRegisterOperand(1)) {
          total++;
          MarkInsnForDelete(insn);
          continue;
        }
      }

      if (iter->IsDirective()) {
        DirectiveEntry *insn = iter->AsDirective();
        if (insn->op() == DirectiveEntry::P2ALIGN) {
          MarkInsnForDelete(*iter);
          total++;
          continue;
        }
      }
    }

    if (total)
      Trace(1, "Killed %d Nop-like constructs", total);
    return true;
  }
};

REGISTER_PLUGIN_FUNC_PASS("NOPKILL", NopKillerElimPass)
}  // namespace
