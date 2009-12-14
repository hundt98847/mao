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

#ifndef MAODATAFLOW_H_
#define MAODATAFLOW_H_

#include <map>
#include <set>

#include "MaoCFG.h"
#include "MaoUnit.h"
#include "MaoUtil.h"

class DFProblem {
 public:
  enum DFProblemDirection {
    DF_Forward,
    DF_Backward,
  };

  // Create a new data-flow problem.
  DFProblem(MaoUnit *unit,
            Function *function,
            const CFG *cfg,
            enum DFProblemDirection direction);

  virtual ~DFProblem() {}

  // Solve the problem instance.
  bool Solve();

 protected:
  // Accessors to query about the solution.
  BitString GetInSet(const BasicBlock& bb) const {
    MAO_ASSERT(direction_ == DF_Forward);
    return df_solution_.find(&bb)->second;
  }

  BitString GetOutSet(const BasicBlock& bb) const {
    MAO_ASSERT(direction_ == DF_Backward);
    return df_solution_.find(&bb)->second;
  };

  // Functions needed by the solver. Must be implemented
  // by the actual problem.
  virtual BitString CreateGenSet(const BasicBlock &bb) = 0;
  virtual BitString CreateKillSet(const BasicBlock &bb) = 0;
  virtual BitString GetInitialEntryState() = 0;
  virtual BitString Transfer(const BitString& inset,
                             const BitString& gen,
                             const BitString& kill) const;
  virtual BitString Confluence(const std::set<BitString *> &dataset) const = 0;

  // Utility functions that can be used in problem instance to implement
  // Confluence()
  BitString Union(const std::set<BitString *> &dataset) const;
  BitString Intersect(const std::set<BitString *> &dataset) const;

  // The number of bits each bitstring has.
  // Should be set in the constructor.
  int num_bits_;

  MaoUnit  *unit_;
  Function *function_;
  const CFG  *cfg_;

  // Only true when the problem is solved.
  bool solved_;

 private:
  // For backwards problem, save the output sets.
  // For forward problems, save the input sets.
  std::map<const BasicBlock *, BitString> df_solution_;

  // Forward or backward problem?
  enum DFProblemDirection direction_;

  // Max number of iterations to try when looking for convergance.
  static const int kMaxNumberOfIterations = 1000;

  // Helper functions to help debugging.
  void DumpState(std::map<const BasicBlock *, BitString> in_map,
                 std::map<const BasicBlock *, BitString> out_map);
};



#endif  // MAODATAFLOW_H_
