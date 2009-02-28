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

  void DoElim() {
    for (CFG::BBVector::const_iterator it = cfg_->Begin();
         it != cfg_->End(); ++it) {
      SectionEntryIterator entry = (*it)->EntryBegin();
      for (; entry != (*it)->EntryEnd(); ++entry) {
        if ((*entry)->Type() != MaoEntry::INSTRUCTION)
          continue;
        InstructionEntry *insn = (InstructionEntry *) (*entry);
        insn->PrintEntry(stderr);


        // Find this pattern:
        //   subl     xxx, %r15d
        //   testl    %r15d, %r15d
        if ((insn->op() == OP_sub || insn->op() == OP_add ||
             insn->op() == OP_adc) &&
            insn->IsRegisterOperand(1) &&
            insn->next() &&
            (insn->next()->Type() == MaoEntry::INSTRUCTION)) {
              InstructionEntry *next = (InstructionEntry*) insn->next();
              if (next->op() == OP_test &&
                  next->IsRegisterOperand(0) && next->IsRegisterOperand(1) &&
                  !strcmp(next->GetRegisterOperand(0), next->GetRegisterOperand(1)) &&
                  !strcmp(next->GetRegisterOperand(0), insn->GetRegisterOperand(1)))
              fprintf(stderr, "*** Found %s/test seq\n",
                      insn->GetOp());
        }

        // Find this pattern:
        //  movq    24(%rsp), %rdx
        //  ... no def for that memory (check 5 instructions)
        //  movq    24(%rsp), %rcx
        if (insn->IsOpMov() &&
            insn->IsRegisterOperand(1) &&
            insn->IsMemOperand(0)) {
          int checked = 0;

          InstructionEntry *next = insn->nextInsn();
          while (checked < 5 && next) {
            if (next->IsControlTransfer() ||
                next->IsCall() ||
                next->IsReturn())
              break;
            unsigned long long defs = GetRegisterDefMask(next);
            if (defs == 0LL || defs == REG_ALL)
              break;  // defines something other than registers
            if (next->IsOpMov() &&
                next->IsRegisterOperand(1) &&
                next->IsMemOperand(0)) {
              // now we have a second movl mem, reg
              // need to check whether two mem operands are the same.
              if (insn->CompareMemOperand(0, next, 0)) {
                fprintf(stderr, "*** Found two insns with same mem op\n");
                insn->PrintEntry(stderr);
                next->PrintEntry(stderr);
              }
            }
            ++checked;
            next = next->nextInsn();
          }
        }

        // Find this pattern:
        //   testl    %r15d, %r15d
        //   je    .L251
#if 0
        if (insn->op() == OP_test &&
            insn->IsRegisterOperand(0) && insn->IsRegisterOperand(1) &&
            !strcmp(insn->GetRegisterOperand(0), insn->GetRegisterOperand(1)) &&
            insn->next() &&
            (insn->next()->Type() == MaoEntry::INSTRUCTION) &&
            ((InstructionEntry*)insn->next())->op() == OP_je) {
          fprintf(stderr, "*** Found test/je seq\n");
        }
#endif

        //insn->PrintEntry(stderr);
        //unsigned long long mask = GetRegisterDefMask(insn);
        //fprintf(stderr, "\t\t\t\treg-defs: ");
        //if (mask != (unsigned long long) REG_ALL) {
        //  PrintRegisterDefMask(mask, stderr);
        //  fprintf(stderr, "\n");
        //} else {
        //  fprintf(stderr, "all\n");
        // }
        if (insn->op() != OP_mov)
          continue;
        if (insn->IsRegister32Operand(0) && insn->IsRegister32Operand(1) &&
            !strcmp(insn->GetRegisterOperand(0), insn->GetRegisterOperand(1))) {
          // found a movl reg32, same-reg32 instruction
          //
          fprintf(stderr, "*** Found zero-extent:");
          insn->PrintEntry(stderr);
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
  zee.DoElim();
}
