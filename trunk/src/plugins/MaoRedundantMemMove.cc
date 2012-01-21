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

// Redundant Load Elimination
//
#include "Mao.h"

namespace {
PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(REDMOV, "Eliminates redundant memory moves", 1) {
  OPTION_INT("lookahead", 6, "Look ahead limit for pattern matcher")
};

// --------------------------------------------------------------------
// Pass
// --------------------------------------------------------------------
class RedMemMovElimPass : public MaoFunctionPass {
 public:
  RedMemMovElimPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("REDMOV", options, mao, function) {
    look_ahead_ = GetOptionInt("lookahead");
  }

  // Find these patterns in a single basic block:
  //
  //  movq    24(%rsp), %rdx
  //  ... no def for this memory,
  //  ... no def for the right hand side register %rdx,
  //  ... check as many as 'lookahead' instructions.
  //  movq    24(%rsp), %rcx
  //
  // If this patterns is found, the last instruction can be
  // changed to:
  //  movq    %rdx, %rcx
  // which has a shorter encoding and avoids a 2nd memory
  // reference.
  //
  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);

    FORALL_CFG_BB(cfg,it) {
      FORALL_BB_ENTRY(it,entry) {
        if (!entry->IsInstruction()) continue;
        InstructionEntry *insn = entry->AsInstruction();

        if (insn->IsOpMov() &&
            insn->IsRegisterOperand(1) &&
            insn->IsMemOperand(0)) {
          if (insn->GetBaseRegister() == GetIP())
            continue;
          int checked = 0;
          BitString mask = GetRegisterDefMask(insn);

          // eliminate this pattern:
          //     movq    (%rax), %rax
          BitString base_index_mask =
            GetMaskForRegister(insn->GetBaseRegister()) |
            GetMaskForRegister(insn->GetIndexRegister());

          if ((mask & base_index_mask).IsNonNull()) continue;
          mask = mask | base_index_mask;

          InstructionEntry *next = insn->nextInstruction();
          while (checked < look_ahead_ && next) {
            if (next->IsControlTransfer() ||
                next->IsCall() ||
                next->IsReturn())
              break;
            // Check if instruction writes to memory.
            // TODO(martint): Implement a safe method in
            // the entry class that checks for memory writes.
            if (next->NumOperands() >= 1
                && next->IsMemOperand(next->NumOperands()-1)) {
              break;
            }

            BitString defs = GetRegisterDefMask(next);
            if (defs.IsNull() || defs.IsUndef())
              break;  // defines something other than registers

            if (next->IsOpMov() &&
                next->op() == insn->op() &&
                next->IsRegisterOperand(1) &&
                next->IsMemOperand(0)) {
              // now we have a second movl mem, reg
              // need to check whether two mem operands are the same.
              if (insn->CompareMemOperand(0, next, 0)) {
                Trace(1, "Found two insns with same mem op");
                if (tracing_level() > 0) {
                  insn->PrintEntry(stderr);
                  InstructionEntry *i2 = insn->nextInstruction();
                  while (i2 != next) {
                    i2->PrintEntry(stderr);
                    i2 = i2->nextInstruction();
                  }
                  next->PrintEntry(stderr);
                }

                // The memory load is redundant and
                // can be replaced by a register.
                // Now set next->op(0) to insn->op(1)
                next->SetOperand(0, insn, 1);
                if (tracing_level() > 0) {
                  fprintf( stderr, " -->");
                  next->PrintEntry(stderr);
                }
              }
            }
            if ((defs & mask).IsNonNull()) {
              break;  // target register gets redefined.
            }

            ++checked;
            next = next->nextInstruction();
          }
        }
      }
    }
    return true;
  }

 private:
  int        look_ahead_;
};

REGISTER_PLUGIN_FUNC_PASS("REDMOV", RedMemMovElimPass)
}  // namespace
