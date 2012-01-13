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

#include <map>
#include <set>

#include "Mao.h"

DFProblem::DFProblem(MaoUnit *unit,
                     Function *function,
                     const CFG  *cfg,
                     enum DFProblemDirection direction)
    : unit_(unit), function_(function), cfg_(cfg), solved_(false),
      direction_(direction) {}

bool DFProblem::Solve() {
  // TODO(martint): Updated code to use Trace() if possible.
  fprintf(stderr, "Inside DFProblem::Solve\n");

  // Terminology used inside this function:
  //    In order to create names that work in both forward
  //    and backwards problem the names "entry" and "exit" are used
  //    instead of "in" and "out" when describing the state.
  //    For forward problems, the in set is called the entry set,
  //    and for backward problems, the out set is called the entry set.

  // Algorithm used to solve the DF problem:
  // for i <- 1 to N
  //    initialize node i
  // while (sets are still changing)
  //   for i <- 1 to N
  //     recompute sets at node i

  MAO_ASSERT_MSG(!solved_, "Problem is already solved.");
  MAO_ASSERT(function_);
  MAO_ASSERT(cfg_);

  // TODO(martint): Only one is actually needed if we are low on memory.
  std::map<const BasicBlock *, BitString> entry_map;
  std::map<const BasicBlock *, BitString> exit_map;

  std::map<const BasicBlock *, BitString> gen_map;
  std::map<const BasicBlock *, BitString> kill_map;

  // Do not try to solve a problem that has zero-length bit sets.
  if (num_bits_ > 0) {
    bool dirty;
    int num_iterations = 0;

    // Initialize all to empty sets.
    FORALL_CFG_BB(cfg_, it) {
      const BasicBlock *bb = *it;
      // Generate initial values for our states and gen/kill sets.
      entry_map[bb] = GetInitialEntryState();
      gen_map[bb] = CreateGenSet(*bb);
      kill_map[bb] = CreateKillSet(*bb);
      exit_map[bb] = Transfer(entry_map[bb],
                              gen_map[bb],
                              kill_map[bb]);
    }

    // Loop until convergance.
    do {
      dirty = false;
      // TODO(martint): Think about the order you iterate on.
      FORALL_CFG_BB(cfg_, it) {
        const BasicBlock *bb = *it;
        std::set<BitString *> confluence_set;
        if (direction_ == DF_Backward) {
          // For backwards problems, confluence_set = successors.
          for (BasicBlock::ConstEdgeIterator eiter = bb->BeginOutEdges();
               eiter != bb->EndOutEdges(); ++eiter) {
            MAO_ASSERT((*eiter)->source() == *it);
            MAO_ASSERT((*eiter)->dest() != NULL);
            confluence_set.insert(&exit_map[(*eiter)->dest()]);
          }
        } else if (direction_ == DF_Forward) {
          // For forward problems, confluence_set = predecessors.
          for (BasicBlock::ConstEdgeIterator eiter = bb->BeginInEdges();
               eiter != bb->EndInEdges(); ++eiter) {
            MAO_ASSERT((*eiter)->dest() == *it);
            MAO_ASSERT((*eiter)->source() != NULL);
            confluence_set.insert(&exit_map[(*eiter)->source()]);
          }
        } else {
          // Problem should either be DF_Backward or DF_Forward.
          MAO_ASSERT(false);
        }
        if (confluence_set.size() > 0) {
          BitString entry_new = Confluence(confluence_set);
          // Update if changed.
          if (!(entry_new == entry_map[bb])) {
            entry_map[bb] = entry_new;
            dirty = true;

            // Calculate the new exit state.
            BitString exit_new = Transfer(entry_map[bb],
                                          gen_map[bb],
                                          kill_map[bb]);
            // No need to compare the new value, just overwrite the old
            // value even if its identical to the previous value.
            exit_map[bb] = exit_new;
          }
        }
      }
      ++num_iterations;
      MAO_ASSERT(num_iterations <= kMaxNumberOfIterations);
    } while (dirty);

    fprintf(stderr, "Num iterations :%d\n", num_iterations);
  }
  // Save the result.
  df_solution_ = entry_map;
  solved_ = true;
  return solved_;
}

// Debug function to print out internal state on stderr.
void DFProblem::DumpState(std::map<const BasicBlock *, BitString> in_map,
                          std::map<const BasicBlock *, BitString> out_map) {
  // Print out the state at a given moment:
  //  - function name
  // Loop over basic blocks and print
  //  - basic block name
  //  - in and out state

  fprintf(stderr, "function : %s\n", function_->name().c_str());
  // Loop over function and generate gen sets
  FORALL_CFG_BB(cfg_, it) {
    const BasicBlock *bb = *it;
    if (!in_map.find(bb)->second.IsNull()
        || !out_map.find(bb)->second.IsNull()) {
      fprintf(stderr, "bb: %s\n", bb->label());
    }
    if (!in_map.find(bb)->second.IsNull()) {
      fprintf(stderr, "in : ");
      in_map.find(bb)->second.Print();
      fprintf(stderr, "\n");
    }
    if (!out_map.find(bb)->second.IsNull()) {
      fprintf(stderr, "out: ");
      out_map.find(bb)->second.Print();
      fprintf(stderr, "\n");
    }
  }
}

BitString DFProblem::Transfer(const BitString& inset,
                              const BitString& gen,
                              const BitString& kill) const {
  // out = gen U ( in - kill)
  return gen | (inset - kill);
}

// Dataset holds the successors.
BitString DFProblem::Union(const std::set<BitString *> &dataset) const {
  // out = U{p=succ()} live[p]
  MAO_ASSERT(dataset.size() > 0);
  std::set<BitString *>::const_iterator iter = dataset.begin();
  BitString out_set = **iter;
  ++iter;
  while (iter != dataset.end()) {
    out_set = out_set | **iter;
    ++iter;
  }
  return out_set;
}

// Dataset holds the successors.
BitString DFProblem::Intersect(const std::set<BitString *> &dataset) const {
  // out = I{p=succ()} live[p]
  MAO_ASSERT(dataset.size() > 0);
  std::set<BitString *>::const_iterator iter = dataset.begin();
  BitString out_set = **iter;
  ++iter;
  while (iter != dataset.end()) {
    out_set = out_set & **iter;
    ++iter;
  }
  return out_set;
}
