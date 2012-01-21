//
// Copyright 2012 Google Inc.
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

// Convert inc|dec reg to add|sub -1|1, reg (the reverse is
// done in MaoAdd2Inc.cc)
//
// Note that there is a subtle dependence which is not being
// handled by this pass.
//
//  inc/dec only write a subset of the flag registers
//  add/sub overwrite all flags.
//
//  inc/dec therefore introduce a dependence on previous
//  writes to the flags register.
//
// This is not handled in this pass, assumption is that
// compilers won't model the flags at this level of granularity
// anyways, so this is more a theoreritical concern.
//
#include "Mao.h"

namespace {
PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(INC2ADD, "Convert inc|dec reg to add|sub 1,reg", 0)
  {};

// --------------------------------------------------------------------
// Pass
// --------------------------------------------------------------------
class Inc2AddPass : public MaoFunctionPass {
 public:
  Inc2AddPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
    : MaoFunctionPass("INC2ADD", options, mao, function) { }

  // Look for these patterns:
  //     inc|dec reg
  // where reg can be any register specification, e.g., ah, al, ax, eax, rax
  // for whichever registers support these forms.
  //
  bool Go() {
    // Iterate over all BBs, all entries which are instructions.
    // Find instructions that have 1 operand and a register as
    // the 1st operand.
    // Then, convert this instruction to an add or sub of 1 to that
    // register.
    //
    FORALL_FUNC_ENTRY(function_, iter) {
      if (!iter->IsInstruction()) continue;
      InstructionEntry *insn = iter->AsInstruction();

      if (insn->NumOperands() != 1 ||
          !insn->IsRegisterOperand(0))
        continue;

      if (insn->op() == OP_inc || insn->op() == OP_dec) {
        InstructionEntry *i = insn->op() == OP_inc ?
          unit_->CreateAdd(function_) : unit_->CreateSub(function_);
        i->instruction()->operands = 2;
        i->SetImmediateIntOperand(0, 32, 1);
        i->SetOperand(1, insn, 0);

        insn->LinkBefore(i);
        MarkInsnForDelete(insn);
        TraceReplace(1, insn, i);
      }
    }

    return true;
  }
};

REGISTER_PLUGIN_FUNC_PASS("INC2ADD", Inc2AddPass )
} // namespace
