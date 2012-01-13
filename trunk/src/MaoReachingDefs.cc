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

#include "Mao.h"
#include <list>
#include <map>
#include <set>

ReachingDefs::ReachingDefs(MaoUnit *unit,
                           Function *function,
                           const CFG *cfg)
    : DFProblem(unit, function, cfg, DF_Forward) {

  // Create the following maps:
  //  - <bb,reg> -> bitstring index.
  //  - bitstring index -> <bb,reg>
  CreateIndexMaps();
  MAO_ASSERT(index_map_.size() == rev_index_map_.size());

  // This is the number of bits needed in the bitstring.
  num_bits_ = index_map_.size();

  // Create a map that maps from register -> bitstring.
  // The bitstring lists all the definitions of the register in the whole
  // function..
  defs_map_ = CreateDefsMap(index_map_);
}

// Gen set for reaching defs:
//  - The defs inside this basic block.
BitString ReachingDefs::CreateGenSet(const BasicBlock& bb) {
  BitString current_set(num_bits_);
  BitString defined = GetDefs(bb);
  for (int regnum = 0; regnum < defined.number_of_bits(); ++regnum) {
    if (defined.Get(regnum)) {
      int index = index_map_.find(std::make_pair(&bb, regnum))->second;
      current_set.Set(index);
    }
  }
  return current_set;
}

// Kill set for reaching defs:
//  - Each def in the bb, kills the defs in all the other bbs.
BitString ReachingDefs::CreateKillSet(const BasicBlock& bb) {
  BitString current_set(num_bits_);
  BitString defined = GetDefs(bb);
  for (int regnum = 0; regnum < defined.number_of_bits(); ++regnum) {
    if (defined.Get(regnum)) {
      // We should not kill the definition inside this bb, only all the others.
      int index = index_map_.find(std::make_pair(&bb, regnum))->second;
      BitString reg_string = GetAllDefsInFunction(regnum);
      MAO_ASSERT(reg_string.Get(index) == true);
      reg_string.Clear(index);
      current_set = current_set | reg_string;
    }
  }
  return current_set;
}

BitString ReachingDefs::GetDefs(const BasicBlock& bb) const {
  BitString defined;
  // Find out all the defs in this basic block.
  for (EntryIterator entry = bb.EntryBegin();
       entry != bb.EntryEnd();
       ++entry) {
    if ((*entry)->IsInstruction()) {
      InstructionEntry *insn = (*entry)->AsInstruction();
      defined = defined | GetRegisterDefMask(insn, true);
    }
  }
  return defined;
}

// Create maps that maps between <bb,reg> to bitstring index
// and the other way.
// index_map_ is used during the solving phase.
// rev_index_map_ is used during the querying phase.
void ReachingDefs::CreateIndexMaps() {
  int current_index = 0;
  // Loop over all basic blocks:
  FORALL_CFG_BB(cfg_, it) {
    const BasicBlock *bb = *it;
    // Look for definitions in bb.
    BitString defined = GetDefs(*bb);
    // loop over the defined registers and update the map.
    for (int regnum = 0; regnum < defined.number_of_bits(); ++regnum) {
      if (defined.Get(regnum)) {
        index_map_[std::make_pair(bb, regnum)] = current_index;
        rev_index_map_[current_index] = std::make_pair(bb, regnum);
        ++current_index;
      }
    }
  }
}

void ReachingDefs::DumpIndexMap(const IndexMap& index_map) const {
  for (IndexMap::const_iterator iter = index_map.begin();
      iter != index_map.end();
      ++iter) {
    // Print the key:
    IndexMapKey key = iter->first;
    fprintf(stderr, "(%s,%s) -> %d \n", key.first->label(),
            GetRegName(key.second), iter->second);
  }
}

void ReachingDefs::DumpRevIndexMap(const RevIndexMap& rev_index_map) const {
  for (RevIndexMap::const_iterator iter = rev_index_map.begin();
      iter != rev_index_map.end();
      ++iter) {
    // Print the key:
    IndexMapKey key = iter->second;
    fprintf(stderr, "%d -> (%s,%s)\n", iter->first, key.first->label(),
            GetRegName(key.second));
  }
}

ReachingDefs::DefsMap ReachingDefs::CreateDefsMap(
    const ReachingDefs::IndexMap& index_map) {

  DefsMap defs_map;

  // TODO(martint): Update 256 to the actual number of registers once
  // the MaoDefs code has been updated.
  for(int i = 0; i <  256; ++i) {
    defs_map.push_back(BitString(num_bits_));
  }

  for (IndexMap::const_iterator iter = index_map.begin();
      iter != index_map.end();
      ++iter) {
    IndexMapKey key = iter->first;
    // bb:     key.first,
    // reg:    key.second
    // index:  iter->second
    // check if it already exists in the map
    MAO_ASSERT(iter->second < num_bits_);
    defs_map[key.second].Set(iter->second);
  }
  return defs_map;
}


void ReachingDefs::DumpDefsMap(const DefsMap& defs_map) const {
  for (int reg_num = 0; reg_num < 256; ++reg_num) {
    fprintf(stderr, "%s -> ", GetRegName(reg_num));
    defs_map_[reg_num].Print();
  }
}

// Helper function that returns the definitions at insn in bb
BitString ReachingDefs::GetReachingDefsAtInstruction(
    const BasicBlock& bb,
    const InstructionEntry& insn) const {
  BitString current_set = GetInSet(bb);

  // Use iterators!
  for (EntryIterator entry = bb.EntryBegin();
       entry != bb.EntryEnd(); ++entry) {
    if ((*entry)->IsInstruction()) {
      InstructionEntry *curr_insn = (*entry)->AsInstruction();
      // Stop when we reach the current instruction
      if (curr_insn == &insn)
        break;

      // if we have a def, remove all other defs to this variable,
      // then add the current.
      BitString def_mask = GetRegisterDefMask(curr_insn, true);
      MAO_ASSERT(def_mask.number_of_bits() == 256);
      for (int regnum = 0; regnum < def_mask.number_of_bits(); ++regnum) {
         if (def_mask.Get(regnum)) {
           // We should not kill the definition inside this bb, only all the
           // definitions from other bbs.
           int index = index_map_.find(std::make_pair(&bb, regnum))->second;
           BitString reg_string = GetAllDefsInFunction(regnum);
           MAO_ASSERT(reg_string.Get(index) == true);
           reg_string.Clear(index);
           current_set = current_set - reg_string;
           current_set.Set(index);
         }
       }
    }
  }
  return current_set;
}


// Return all the definitions(as BitString) that reach this instruction.
std::list<Definition>
ReachingDefs::GetAllReachingDefs(const BasicBlock& bb,
                                 const InstructionEntry& insn) {
  MAO_ASSERT(solved_);
  // Get the definition string at the instruction.
  BitString current_set = GetReachingDefsAtInstruction(bb, insn);

  // Now we can create the result set.
  std::list<Definition> defs;
  for (int i = 0; i < current_set.number_of_bits(); ++i) {
    if (current_set.Get(i)) {
      IndexMapKey key = rev_index_map_[i];
      const InstructionEntry *ie =
          GetDefiningInstruction(*key.first,
                                 key.second,
                                 key.first->last_entry());
      if (ie == NULL) {
        fprintf(stderr, "Failure, unable to find defining instruction.\n");
      } else {
        Definition def(ie, key.first, key.second);
        defs.push_back(def);
      }
    }
  }
  return defs;
}


// Return a list of the definitions (instructions/basicblocks) of reg_number at
// insn in bb.
std::list<Definition>
ReachingDefs::GetReachingDefs(const BasicBlock& bb,
                              const InstructionEntry& insn,
                              int reg_number) const {
  MAO_ASSERT(solved_);

  // Get the definition string at the instruction.
  BitString current_set = GetReachingDefsAtInstruction(bb, insn);
  BitString reg_string = GetAllDefsInFunction(reg_number);
  BitString current_set_for_reg = current_set & reg_string;

  // Resturn list of definitions.
  std::list<Definition> defs;
  for (int i = 0; i < current_set_for_reg.number_of_bits(); ++i) {
    if (current_set_for_reg.Get(i)) {
      IndexMapKey key = rev_index_map_.find(i)->second;
      const InstructionEntry *ie = NULL;
      if (key.first == &bb) {
        // If the definition is found in the same basic block, make
        // sure we start looking for the defining instruction above
        // the current instruction.
        ie = GetDefiningInstruction(bb,
                                    reg_number,
                                    insn.prev());
      } else {
        // If the definition is found in another basic block, make sure
        // we start looking from the last instruction in that basic block.
        ie = GetDefiningInstruction(*key.first,
                                    key.second,
                                    key.first->last_entry());
      }
      if (ie == NULL) {
        fprintf(stderr, "Failure, unable to find defining instruction.\n");
      } else {
        defs.push_back(Definition(ie, key.first, key.second));
      }
    }
  }
  return defs;
}


// Find the instructions that define reg_number in basic block, starting
// at the entry start_entry. Return NULL if no instruction defines the reg.
const InstructionEntry *
ReachingDefs::GetDefiningInstruction(const BasicBlock& bb,
                                     int reg_number,
                                     MaoEntry *start_entry) const {
  for (ReverseEntryIterator entry =
           ReverseEntryIterator(start_entry);
       entry != bb.RevEntryEnd(); ++entry) {
    if ((*entry)->IsInstruction()) {
      InstructionEntry *curr_insn = (*entry)->AsInstruction();
      BitString defined = GetRegisterDefMask(curr_insn, true);
      if (defined.Get(reg_number)) {
        return curr_insn;
      }
    }
  }
  return NULL;
}
