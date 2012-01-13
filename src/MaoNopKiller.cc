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

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(NOPKILL, "Removes all NOPs in the code", 0) {
};

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
    std::list<MaoEntry *> redundants;

   FORALL_FUNC_ENTRY(function_, entry) {
      if (entry->IsInstruction()) {
        InstructionEntry *insn = entry->AsInstruction();

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

      if (entry->IsDirective()) {
        DirectiveEntry *insn = entry->AsDirective();
        if (insn->op() == DirectiveEntry::P2ALIGN) {
          redundants.push_back(insn);
          continue;
        }
      }
    }

    // Now delete all the redundant ones.
    for (std::list<MaoEntry *>::iterator it = redundants.begin();
         it != redundants.end(); ++it) {
      TraceC(1, "Remove: ");
      if (tracing_level() > 0)
        (*it)->PrintEntry(stderr);
      unit_->DeleteEntry(*it);
    }
    return true;
  }
};


REGISTER_FUNC_PASS("NOPKILL", NopKillerElimPass)

}  // namespace
