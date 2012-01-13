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
#include <set>

Liveness::Liveness(MaoUnit *unit,
                   Function *function,
                   const CFG *cfg)
    : DFProblem(unit, function, cfg, DF_Backward) {
  // TODO(martint): Now GetRegisterDefMask() returns 256, no matter
  // how many registers there are. This should be fixed so we dont
  // waste space. Since we cant compare strings of different length,
  // we hardcode the num_bits_ to 256 here.
  // num_bits_ = GetNumberOfRegisters()
  num_bits_ = 256;
}

// Gen set for Liveness:
//  - The set of variables used in bb before any assignment.
BitString Liveness::CreateGenSet(const BasicBlock& bb) {
  BitString current_set(num_bits_);  // Defaults to no regisers.
  // Move backwards. remove defs, then add uses
  for (ReverseEntryIterator entry = bb.RevEntryBegin();
       entry != bb.RevEntryEnd(); ++entry) {
    if ((*entry)->IsInstruction()) {
      InstructionEntry *insn = (*entry)->AsInstruction();
      BitString def_mask = GetRegisterDefMask(insn, true);
      BitString use_mask = GetRegisterUseMask(insn, true);
      current_set = Transfer(current_set, use_mask, def_mask);
    }
  }
  return current_set;
}

// Kill set for Liveness:
//  - The set of variables assigned a value in bb before any use.
BitString Liveness::CreateKillSet(const BasicBlock& bb) {
  BitString current_set(num_bits_);  // Defaults to no regisers.
  for (ReverseEntryIterator entry = bb.RevEntryBegin();
       entry != bb.RevEntryEnd(); ++entry) {
    if ((*entry)->IsInstruction()) {
      InstructionEntry *insn = (*entry)->AsInstruction();
      BitString def_mask = GetRegisterDefMask(insn, true);
      BitString use_mask = GetRegisterUseMask(insn, true);
      current_set = Transfer(current_set, def_mask, use_mask);
    }
  }
  return current_set;
}


// Return all the registers that are live AFTER the given instruction.
BitString Liveness::GetLive(const BasicBlock& bb,
                            const InstructionEntry& insn) {
  MAO_ASSERT(solved_);
  // Start by getting the end of the bb, then move backwards until we
  // reach insn
  BitString current_set = GetOutSet(bb);

  // Use iterators!
  for (ReverseEntryIterator entry = bb.RevEntryBegin();
       entry != bb.RevEntryEnd(); ++entry) {
    if ((*entry)->IsInstruction()) {
      InstructionEntry *curr_insn = (*entry)->AsInstruction();
      // Stop when we reach the current instruction
      if (curr_insn == &insn)
        break;
      // remove defs, then add uses
      BitString def_mask = GetRegisterDefMask(curr_insn, true);
      BitString use_mask = GetRegisterUseMask(curr_insn, true);
      current_set = Transfer(current_set, use_mask, def_mask);
    }
  }
  return current_set;
}
