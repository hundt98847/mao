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

class LongInstructionsSplitPass : public MaoPass {
 public:
  LongInstructionsSplitPass(MaoUnit *mao,
                            Function *func,
                            MaoRelaxer::SizeMap *sizes) :
    MaoPass("LISPLIT", mao->mao_options(), MAO_OPTIONS(LISPLIT), false,
            CFG::GetCFG(mao, func)),
    mao_(mao), func_(func), sizes_(sizes) {
    length_ = GetOptionInt("length");
    fill_ = GetOptionInt("fill");
  }

  // Split instructions which have a very long encoding.
  // On certain processors, sequences of such instructions can result in
  // a stall in the decoder.
  //
  // For now, only search for such sequences.
  bool Go() {
    if (!enabled()) return true;
    std::list<InstructionEntry *> split_points;

    FORALL_CFG_BB(cfg(),it) {
      FORALL_BB_ENTRY(it,entry) {
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
        InstructionEntry *nop = mao_->CreateNop();
        (*it)->LinkAfter(nop);
      }
    }

    return true;
  }

 private:
  MaoUnit   *mao_;
  Function  *func_;
  MaoRelaxer::SizeMap *sizes_;
  int        length_;
  int        fill_;
};


// External Entry Point
//
void PerformLongInstructionSplit(MaoUnit *mao, Function *function) {
  LongInstructionsSplitPass lipass(mao,
                                   function,
                                   MaoRelaxer::GetSizeMap(mao, function));
  lipass.set_timed();
  lipass.Go();
}
