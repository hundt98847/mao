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

// random nop insertion - nopinizer
//
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(NOPIN, 3) {
  OPTION_INT("seed",    17, "Seed for random number generation"),
  OPTION_INT("density", 11, "Density for inserts, random, 1 / 'density' insn"),
  OPTION_INT("thick",   3,  "How many nops in a row, random, 1 / 'thick'"),
};

class NopInizerPass : public MaoPass {
 public:
  NopInizerPass(MaoUnit *mao, Function *func) :
    MaoPass("NOPIN", mao->mao_options(), MAO_OPTIONS(NOPIN), false,
            func->cfg()),
    mao_(mao), func_(func) {
    seed_ = GetOptionInt("seed");
    density_ = GetOptionInt("density");
    thick_ = GetOptionInt("thick");

    if (enabled()) {
      srand(seed_);
      Trace(1, "Nopinizer! Seed: %d, dense: %d, thick: %d",
            seed_, density_, thick_);
    }
  }

  // Randomly insert nops into the code stream, based
  // on some distribution density.
  //
  bool Go() {
    if (!enabled()) return true;

    int count_down = (int) (1.0 * density_ * (rand() / (RAND_MAX + 1.0)));
    FORALL_FUNC_ENTRY(func_,entry) {
      if (!entry->IsInstruction())
        continue;

      if (count_down) {
        --count_down;
      } else {
        int num = (int) (1.0 * thick_ * (rand() / (RAND_MAX + 1.0)));
        for (int i = 0; i < num; i++) {
          InstructionEntry *nop = mao_->CreateNop();
          entry->LinkBefore(nop);
        }
        count_down = (int) (1.0 * density_ * (rand() / (RAND_MAX + 1.0)));
        TraceC(1, "Inserted %d nops, before:", num);
        if (tracing_level() > 0)
          entry->PrintEntry(stderr);
      }
    }
    if (cfg())
      cfg()->InvalidateCFG(func_);
    return true;
  }

 private:
  MaoUnit        *mao_;
  Function       *func_;
  int             seed_;
  int             density_;
  int             thick_;
};


// External Entry Point
//

void PerformNopinizer(MaoUnit *mao, Function *func) {
  NopInizerPass nopin(mao, func);
  nopin.set_timed();
  nopin.Go();
}
