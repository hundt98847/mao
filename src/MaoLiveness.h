//
// Copyright 2009 Google Inc.
//
// This program is free software; you can redistribute it and/or
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
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA.

#ifndef MAOLIVENESS_H_
#define MAOLIVENESS_H_

#include <set>

#include "MaoCFG.h"
#include "MaoDataFlow.h"
#include "MaoUnit.h"
#include "MaoUtil.h"

// Livness
//   Data Represention = Keep one bit for each register
//   gen                  : any use (not def before in the same BB)
//   kill                 : any def before any use in bb
//   transfer function    : in = (out-kill) U gen
//   confluence op        : out = U in_s
//   start state          : out_final = {}


// An implementation of the Liveness data-flow problem.
class Liveness : public DFProblem {
 public:
  Liveness(MaoUnit *unit,
           Function *function,
           const CFG *cfg);
  // Returns the live registers of a given instruction.
  // The information is stored in a bitstring, indexed by register number.
  // A set bit means the register is live.
  BitString GetLive(const BasicBlock& bb, const InstructionEntry& insn);
 private:
  BitString CreateGenSet(const BasicBlock& bb);
  BitString CreateKillSet(const BasicBlock& bb);

  BitString GetInitialEntryState() {return BitString(num_bits_);}

  BitString Confluence(const std::set<BitString *> &dataset) const {
    return Union(dataset);
  }
};



#endif  // MAOLIVENESS_H_
