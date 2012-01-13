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
#include "Mao.h"

namespace {

PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(NOPIN, "Inserts nops randomly", 3) {
  OPTION_INT("seed",    17, "Seed for random number generation"),
  OPTION_INT("density", 11, "Density for inserts, random, 1 / 'density' insn"),
  OPTION_INT("thick",   3,  "How many nops in a row, random, 1 / 'thick'"),
};

class NopInizerPass : public MaoFunctionPass {
 public:
  NopInizerPass(MaoOptionMap *options, MaoUnit *mao, Function *func)
      : MaoFunctionPass("NOPIN", options, mao, func) {
    seed_ = GetOptionInt("seed");
    density_ = GetOptionInt("density");
    thick_ = GetOptionInt("thick");

    srand(seed_);
    Trace(1, "Nopinizer! Seed: %d, dense: %d, thick: %d",
          seed_, density_, thick_);
  }

  // Randomly insert nops into the code stream, based
  // on some distribution density.
  //
  bool Go() {
    int count_down = (int) (1.0 * density_ * (rand() / (RAND_MAX + 1.0)));
    FORALL_FUNC_ENTRY(function_,entry) {
      if (!entry->IsInstruction())
        continue;
      MaoEntry *prev_entry =  entry->prev();
      if (prev_entry->IsInstruction()) {
        InstructionEntry *prev_ins = prev_entry->AsInstruction();
        //lock appears as separate instruction but in reality is a prefix
        //Inserting nops between lock and the next instruction is bad as
        //'lock nop' is an illegal instruction sequence
        if (prev_ins->IsLock())
          continue;
      }

      if (count_down) {
        --count_down;
      } else {
        int num = (int) (1.0 * thick_ * (rand() / (RAND_MAX + 1.0)));
        for (int i = 0; i < num; i++) {
          InstructionEntry *nop = unit_->CreateNop(function_);
          entry->LinkBefore(nop);
        }
        count_down = (int) (1.0 * density_ * (rand() / (RAND_MAX + 1.0)));
        TraceC(1, "Inserted %d nops, before:", num);
        if (tracing_level() > 0)
          entry->PrintEntry(stderr);
      }
    }

    CFG::InvalidateCFG(function_);
    return true;
  }

 private:
  int             seed_;
  int             density_;
  int             thick_;
};

REGISTER_PLUGIN_FUNC_PASS("NOPIN", NopInizerPass)
}  // namespace
