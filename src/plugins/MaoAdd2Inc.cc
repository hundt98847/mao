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

// Convert add|sub -1|1,reg to inc|dec reg (the reverse is
// done in MaoInc2Add.cc)
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
MAO_DEFINE_OPTIONS(ADD2INC, "Convert add|sub 1,reg to inc|dec reg", 0)
  {};

// --------------------------------------------------------------------
// Pass
// --------------------------------------------------------------------
class Add2IncPass : public MaoFunctionPass {
 public:
  Add2IncPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
    : MaoFunctionPass("ADD2INC", options, mao, function) { }

  // Look for these patterns:
  //     add/sub 1, reg
  // where reg can be any register specification, e.g., ah, al, ax, eax, rax
  // for whichever registers support these forms.
  //
  bool Go() {
    // Iterate over all BBs, all entries which are instructions.
    // Find instructions that have 2 operands, an immediate as
    // the 1st operand, and a regiser as the second operand.
    // Then, if the immediate is 1 and opcodes are OP_add or
    // OP_sub, replace the instructions with an inc or dec
    // instruction.
    //
    FORALL_FUNC_ENTRY(function_, iter) {
      if (!iter->IsInstruction()) continue;
      InstructionEntry *insn = iter->AsInstruction();

      if (insn->NumOperands() != 2 ||
          !insn->IsImmediateIntOperand(0) ||
          !insn->IsRegisterOperand(1))
        continue;

      if (insn->op() == OP_add && insn->GetImmediateIntValue(0) == 1) {
        InstructionEntry *i = unit_->CreateIncFromOperand(function_, insn, 1);
        insn->LinkBefore(i);
        MarkInsnForDelete(insn);
        TraceReplace(1, insn, i);
      }
      if (insn->op() == OP_sub && insn->GetImmediateIntValue(0) == 1) {
        InstructionEntry *i = unit_->CreateDecFromOperand(function_, insn, 1);
        insn->LinkBefore(i);
        MarkInsnForDelete(insn);
        TraceReplace(1, insn, i);
      }
    }

    return true;
  }
};

REGISTER_PLUGIN_FUNC_PASS("ADD2INC", Add2IncPass )
} // namespace
