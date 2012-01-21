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

// Add Add identifier
// TODO(martint): Make sure that the eflags of the first insn are not used.
#include "Mao.h"

namespace {

PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(ADDADD, "A peephole optimization that removes redundant "\
                   "add instructions in certain cases", 0) {
};

class AddAddElimPass : public MaoFunctionPass {
 public:
  AddAddElimPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("ADDADD", options, mao, function),
        emask_(GetMaskForRegister(GetRegFromName("eflags"))) { }


  // Add add  pattern finder:
  //     add/sub rX, IMM1
  //     ()*
  //     add/sub rX, IMM2
  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);

    Trace(3, "Iterate over all basic blocks in function %s",
          function_->name().c_str());
    FORALL_CFG_BB(cfg, it) {
      MaoEntry *first = (*it)->GetFirstInstruction();
      if (!first) continue;

      // The algorithm used is to identify an addi/subi instruction
      // and then move upwards (using prev()) until we either
      // find a new redundant addi/subi instruction, or
      // an instruction that breaks the pattern.
      FORALL_BB_ENTRY(it, entry) {
        // Only check instructions
        if (!entry->IsInstruction()) continue;

        InstructionEntry *insn = entry->AsInstruction();

        // The first instruction can be the last instruction of
        // the pattern we are looking for.
        if (first == insn) continue;

        // This is possibly the end of the pattern. Start looking
        // at previuos entries.
        if (IsAddIOrSubI(insn)) {
          // Get the def mask of the instructions
          BitString imask = GetRegisterDefMask(insn);

          InstructionEntry *prev = insn->prevInstruction();
          while (prev) {
            BitString pmask = GetRegisterDefMask(prev);
            if (pmask.IsUndef()) {  // insn with unknown side effects, break
              break;
            }
            // Check if the this instruction ends the pattern
            if (IsAddIOrSubI(prev)) {
              if (insn->GetRegisterOperand(1) == prev->GetRegisterOperand(1)) {
                Trace(2, "Addi/Subi pattern identified.");
                if (tracing_level() >= 2) {
                  (*it)->Print(stderr, prev, insn);
                }
                // Solve the trivial case when there is no instruction between
                // the adds/subs, and both of the expressions are constant
                // numbers.
                if (insn->prev() == prev) {
                  if (!UpdateImmediate(prev, insn)) {
                    MAO_ASSERT_MSG(false,
                                   "Unable to update immediate value.");
                  }
                  unit_->DeleteEntry(prev);
                  Trace(2, "Removed redundant add/sub instruction and updated "
                        "immediate value.");
                }
                break;
              }
            }
            // The instruction did not end the pattern, now check if
            // we should continue looking up or not.

            // There is a conflict in the defs
            if ((!(pmask & imask).IsNull()) && !((pmask & imask) == emask_ )) {
              break;
            }
            // The register is used here. In order to remove any of the add/sub
            // instruction, this will probably need to be updated. The simple
            // solution is to stop check here and look for another pattern
            // TODO(martint): Check if there is any use of the register here!

            // Check for instruction we dont handle.
            if (prev->IsPredicated() ||  // bail on cmoves...
                prev->op() == OP_bswap ||
                prev->op() == OP_call  ||
                prev->op() == OP_lcall) {  // bail on these, don't understand em
              break;
            }

            // Stop at the top of the bsic block.
            if (prev == first)
              break;
            prev = prev->prevInstruction();
          }  // prev
        }  // IsAddIOrSubI()
      }  // Entries
    }  // BB
    return true;
  }
 private:
  bool IsAddIOrSubI(InstructionEntry *insn) {
    return ((insn->op() == OP_add || insn->op() == OP_sub)  &&
            insn->NumOperands() == 2 &&
            insn->IsImmediateIntOperand(0) &&
            insn->IsRegisterOperand(1));
  }

  // Update the imm value in inst2 to the hold the sum of inst1 and inst2
  // Currently only handles simple immediate values. If the update was done
  // return True, otherwise False.
  // TODO(martint): Support more types of immediate values
  // TODO(martint): Use the MaoDefs to find out possible op for immedate
  //                values instead of always using index 0.
  bool UpdateImmediate(InstructionEntry *inst1, InstructionEntry *inst2) {
    if (inst1->NumOperands() < 1 || inst2->NumOperands() < 1) {
      return false;
    }
    if (!(inst1->IsImmediateIntOperand(0) && inst2->IsImmediateIntOperand(0))) {
      return false;
    }

    // Now get the immediate value
    expressionS *imm1 = inst1->instruction()->op[0].imms;
    expressionS *imm2 = inst2->instruction()->op[0].imms;

    // supported variants:
    if (imm1->X_op == O_constant && imm2->X_op == O_constant) {
      imm2->X_add_number = imm1->X_add_number + imm2->X_add_number;
      return true;
    }

    if (imm1->X_op == O_symbol && imm2->X_op == O_constant) {
      imm2->X_op = O_symbol;
      imm2->X_add_number = imm1->X_add_number + imm2->X_add_number;
      imm2->X_add_symbol = imm1->X_add_symbol;
      return true;
    }

    if (imm1->X_op == O_symbol && imm2->X_op == O_symbol) {
      imm2->X_op = O_add;
      imm2->X_add_number = imm1->X_add_number + imm2->X_add_number;
      imm2->X_op_symbol  = imm1->X_add_symbol;
      return true;
    }


    if (imm1->X_op == O_constant && imm2->X_op == O_symbol) {
      imm2->X_add_number = imm1->X_add_number + imm2->X_add_number;
      return true;
    }

    return false;
  }

  // Stores the mask for the e-flag.
  const BitString emask_;
};

REGISTER_PLUGIN_FUNC_PASS("ADDADD", AddAddElimPass)
}  // namespace
