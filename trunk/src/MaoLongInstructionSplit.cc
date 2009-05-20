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
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"
#include "MaoRelax.h"


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(LISPLIT, 2) {
  OPTION_INT("length", 8, "Instruction is considered 'long' if encoding"
             " has this size or longer"),
  OPTION_INT("fill", 1, "Fill in that many nops")
};

class LongInstructionsSplitPass : public MaoFunctionPass {
 public:
  LongInstructionsSplitPass(MaoOptionMap *options, MaoUnit *mao,
                            Function *func)
      : MaoFunctionPass("LISPLIT", options, mao, func), sizes_(NULL) {
    length_ = GetOptionInt("length");
    fill_ = GetOptionInt("fill");
  }

  // Split instructions which have a very long encoding.
  // On certain processors, sequences of such instructions can result in
  // a stall in the decoder.
  //
  // For now, only search for such sequences.
  bool Go() {
    std::list<InstructionEntry *> split_points;

    CFG *cfg = CFG::GetCFG(unit_, function_);
    sizes_ = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());

    FORALL_CFG_BB(cfg, it) {
      FORALL_BB_ENTRY(it, entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();

        int size = (*sizes_)[insn];
        if (size >= length_) {
          InstructionEntry *next = insn->nextInstruction();
          if (next && ((*sizes_)[next] >= length_)) {
            Trace(1, "Sizes: %d, %d", size, (*sizes_)[next]);
            if (tracing_level() > 0) {
              insn->PrintEntry(stderr);
              next->PrintEntry(stderr);
            }
          }
          split_points.push_back(insn);
        }
      }  // Entries
    }  // BB

    // Now insert nops
    for (std::list<InstructionEntry *>::iterator it = split_points.begin();
         it != split_points.end(); ++it) {
      for (int i = 0; i < fill_; i++) {
        InstructionEntry *nop = unit_->CreateNop(function_);
        (*it)->LinkAfter(nop);
      }
    }

    return true;
  }

 private:
  MaoEntryIntMap *sizes_;
  int        length_;
  int        fill_;
};


// External Entry Point
//
void InitLongInstructionSplit() {
  RegisterFunctionPass(
      "LISPLIT",
      MaoFunctionPassManager::GenericPassCreator<LongInstructionsSplitPass>);
}
