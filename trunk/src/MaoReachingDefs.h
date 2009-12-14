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

#ifndef MAOREACHINGDEFS_H_
#define MAOREACHINGDEFS_H_

#include <list>
#include <map>
#include <set>
#include <utility>

#include "MaoCFG.h"
#include "MaoDataFlow.h"
#include "MaoUnit.h"
#include "MaoUtil.h"

// Reaching Defs
//   Data Represention = For each register, keep one bit for each
//                       basic block is it defined in.
//   gen                  : def sets bit for this bb
//   kill                 : def sets bit for all other bbs
//   transfer function    : out = (in - kill) U gen
//   confluence op        : in = U out_p
//   start state          : in_first = {}

class Definition {
 public:
  Definition(const InstructionEntry *entry,
             const BasicBlock *bb,
             int register_number)
      : entry_(entry), bb_(bb), register_number_(register_number) {}
  const InstructionEntry *entry() const { return entry_; }
  const BasicBlock *bb() const { return bb_; }
  int register_number() const { return register_number_; }
 private:
  const InstructionEntry *entry_;
  const BasicBlock *bb_;
  int register_number_;
};

class ReachingDefs : public DFProblem {
 public:
  ReachingDefs(MaoUnit *unit,
               Function *function,
               const CFG *cfg);

  // Public API
  // Get all the reaching definitions at the given instruction.
  std::list<Definition> GetAllReachingDefs(const BasicBlock& bb,
                                           const InstructionEntry& insn);
  // Return the definitions for register reg_number at the given instruction.
  std::list<Definition> GetReachingDefs(const BasicBlock& bb,
                                        const InstructionEntry& insn,
                                        int reg_number) const;
 private:
  BitString CreateGenSet(const BasicBlock& bb);
  BitString CreateKillSet(const BasicBlock& bb);

  BitString GetInitialEntryState() {return BitString(num_bits_);}

  BitString Confluence(const std::set<BitString *> &dataset) const {
    return Union(dataset);
  }

  // Return all the registers defined in the basic block.
  BitString GetDefs(const BasicBlock& bb) const;

  // Return the last instruction in the basic block that defines the
  // register reg_num, and start looking at entry start_entry. Returns NULL if
  // no definition is found.
  const InstructionEntry *GetDefiningInstruction(const BasicBlock& bb,
                                                 int reg_number,
                                                 MaoEntry *start_entry) const;

  // Helper function that returns the definitions at insn in bb.
  BitString GetReachingDefsAtInstruction(const BasicBlock& bb,
                                         const InstructionEntry& insn) const;


  // The supporting data structures needed:
  typedef std::pair<const BasicBlock *, int> IndexMapKey;
  typedef std::map<IndexMapKey, int> IndexMap;
  typedef std::map<int, IndexMapKey> RevIndexMap;
  //typedef std::map<int, BitString> DefsMap;
  typedef std::vector<BitString> DefsMap;

  // Map from <basicblock, registernumber> -> index in bitstring
  // Needed during the solver.
  IndexMap index_map_;

  // Map from index in bitstring to <basicblock, registernumber>.
  // Needed when we query the solution.
  RevIndexMap rev_index_map_;

  // Map from register_number to a bitstring pointing to all location
  // in the function where this register is defined.
  // Used in CreateKillSet() and when we query the solution
  // for the definitions of a particular register.
  DefsMap defs_map_;

  void CreateIndexMaps();
  DefsMap CreateDefsMap(const ReachingDefs::IndexMap& index_map);

  void DumpIndexMap(const IndexMap& index_map) const;
  void DumpRevIndexMap(const RevIndexMap& rev_index_map) const;
  void DumpDefsMap(const DefsMap& index_map) const;

  // Wrapper to access defs_map_. Need to provide a default
  // string if it not in the map.
  BitString GetAllDefsInFunction(int reg_num) const {
    MAO_ASSERT(reg_num < (int)defs_map_.size());
    return defs_map_[reg_num];
  }
};

#endif  // MAOREACHINGDEFS_H_
