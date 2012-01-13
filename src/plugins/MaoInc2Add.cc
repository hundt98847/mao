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
MAO_DEFINE_OPTIONS(INC2Add, "Convert inc|dec reg to add|sub 1,reg", 0)
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
    CFG *cfg = CFG::GetCFG(unit_, function_);

    // Container for instructions to remove. In order
    // to make things simple and to avoid messing up
    // iterators, we collect instructions in a first
    // pass and delete them in a second loop.
    //
    std::list<InstructionEntry *> redundants;

    // Iterate over all BBs, all entries which are instructions.
    // Find instructions that have 1 operand and a register as
    // the 1st operand.
    // Then, convert this instruction to an add or sub of 1 to that
    // register.
    //
    FORALL_CFG_BB(cfg, it) {
      FORALL_BB_ENTRY(it, entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();

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
          redundants.push_back(insn);
          TraceReplace(insn, i);
        }
      }
    }

    // Now delete all the collected redundant instructions.
    for (std::list<InstructionEntry *>::iterator it = redundants.begin();
         it != redundants.end(); ++it) {
      unit_->DeleteEntry(*it);
    }

    return true;
  }

 private:
  // Helper function to dump modified insn's.
  void TraceReplace(InstructionEntry *before, InstructionEntry *after) {
    TraceC(1, "*** Replaced: ");
    if (tracing_level() > 0) before->PrintEntry(stderr);
    TraceC(1, "*** With    : ");
    if (tracing_level() > 0) after->PrintEntry(stderr);
  }
};

extern "C" {
  void MaoInit() {
    REGISTER_FUNC_PASS("INC2ADD", Inc2AddPass )
  }
}

} // namespace
