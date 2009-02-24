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
      MaoUnit::EntryIterator entry = (*it)->BeginEntries();
      MaoUnit::EntryIterator end = (*it)->EndEntries();
      for (; entry != end; ++entry) {
        MaoEntry *e = (*entry);
        if (!e->Type() == MaoEntry::INSTRUCTION)
          continue;
        InstructionEntry *insn = (InstructionEntry *) e;
        if (insn->op() != OP_mov)
          continue;
        if (insn->IsRegisterOperand(0) &&
            insn->IsRegisterOperand(1) &&
            !strcmp(insn->GetRegisterOperand(0), insn->GetRegisterOperand(1))) {
          fprintf(stderr, "Working on:");
          e->PrintEntry(stderr);
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
