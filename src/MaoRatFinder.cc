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

// Rat Finder Pass
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"

#include <set>

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(RATFINDER, 0) {
};

class RatFinderPass : public MaoFunctionPass {
 public:
  RatFinderPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("RATFINDER", options, mao, function) { }

  // RAT finder: For each basic block, this pass looks
  // at the defined registers, and checks if there are any writes to registers
  // who have only been partially written before. This access pattern might
  // cause stalls in the pipeline. See "Intel 64 and IA-32 Architecture
  // Optimization Reference Manual" for "Partial Register Stalls".
  // Trace level 1: Prints out the basic block which have possible
  //                RAT stalls.
  // Trace level 2: Prints out each instruction that triggers a possible
  //                RAT stall.
  // Trace level 3: Prints out a list of defined registers for each
  //                basic block.
  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);

    FORALL_CFG_BB(cfg, it) {
      MaoEntry *first = (*it)->GetFirstInstruction();
      if (!first) continue;

      // Hold the list of defined registers in this basic block. The subregs
      // defined are also stored here.
      BitString all_defined_gpp_regs_mask = BitString(0x0ull, 0x0ull, 0x0ull,
                                                      0x0ull);

      // Number of found RAT stall possibilities for this basic block
      int num_rat_stall_possibilites = 0;

      FORALL_BB_ENTRY(it, entry) {
        if (!(*entry)->IsInstruction()) continue;
        InstructionEntry *insn = (*entry)->AsInstruction();

        // Keep track of all the defined registers in the basic block.
        // TODO(martint): code should only handle GPP registers
        // When we add a new entry, check if a child register entry was defined
        // before. If so, this is a possible RAT stall.

        // Get the registers that are defined by the current instruction.
        // Normally this is only one register/instruction
        std::set<const reg_entry *> defined_gpp_regs = GetDefinedRegister(insn);
        // For each defined registers:
        for (std::set<const reg_entry *>::const_iterator iter =
                 defined_gpp_regs.begin();
             iter != defined_gpp_regs.end();
             ++iter) {
          const reg_entry *defined_reg = *iter;

          // Since 32-bit writes in 64-bit mode are automatically
          // zero extended to the 64-bit registers, we upgrade
          // the register name it defines.
          if (unit_->Is64BitMode()) {
            // Upgrade eXX register to rXX registers!
            // TODO(martint): change to to avoid strcmp
            if (strcmp(defined_reg->reg_name, "eax") == 0) {
              defined_reg = GetRegFromName("rax");
            } else if (strcmp(defined_reg->reg_name, "ebx") == 0) {
              defined_reg = GetRegFromName("rbx");
            } else if (strcmp(defined_reg->reg_name, "ecx") == 0) {
              defined_reg = GetRegFromName("rcx");
            } else if (strcmp(defined_reg->reg_name, "edx") == 0) {
              defined_reg = GetRegFromName("rdx");
            } else if (strcmp(defined_reg->reg_name, "edi") == 0) {
              defined_reg = GetRegFromName("rdi");
            } else if (strcmp(defined_reg->reg_name, "esi") == 0) {
              defined_reg = GetRegFromName("rsi");
            } else if (strcmp(defined_reg->reg_name, "ebp") == 0) {
              defined_reg = GetRegFromName("rbp");
            } else if (strcmp(defined_reg->reg_name, "esp") == 0) {
              defined_reg = GetRegFromName("rsp");
            }
          }

          // Check if the current write will update a register
          // that have only partially been updated before. If so,
          // this is a possible RAT stall (Register Alias Table Stall)
          if (IsPossibleRAT(defined_reg, &all_defined_gpp_regs_mask)) {
            num_rat_stall_possibilites++;
            if (tracing_level() >= 2) {
              Trace(2, "Possible RAT stall: ");
              insn->PrintEntry(stderr);
            }
          }
          // Now, add the current register to the list of defined
          // registers.
          all_defined_gpp_regs_mask = all_defined_gpp_regs_mask
              | GetMaskForRegister(defined_reg->reg_name);
        }  // defined_gpp_regs
      }  // Entries

      if (num_rat_stall_possibilites > 0) {
        Trace(1, "Found %d RAT stall possibilities in basic block",
              num_rat_stall_possibilites);
        // Print the basic block, given the instruction.
        if (tracing_level() >= 1) {
          FORALL_BB_ENTRY(it, entry) {
            (*entry)->PrintEntry(stderr);
          }
        }
      }

      // Print out all the registers defined in this basic block
      if (tracing_level() >= 3) {
        PrintRegisterDefMask(stderr, all_defined_gpp_regs_mask, "Defined ");
      }
    }  // BB

    return true;
  }
 private:
  bool IsPossibleRAT(const reg_entry *reg,
                   BitString *regs_mask) const {
    // Check if the mask includes one childreg definition of reg.
    BitString reg_mask = GetMaskForRegister(reg->reg_name);
    // First condition: The registers (and subregisters) must have been written
    //                  to before in this basic block.
    if ( !((reg_mask & (*regs_mask)) ==
           BitString(0x0ull, 0x0ull, 0x0ull, 0x0ull))) {
      // Second condition: Check if the full register was already written to.
      if ( !((reg_mask & (*regs_mask)) == reg_mask) ) {
        return true;
      }
    }
    return false;
  }
};


// External Entry Point
//
void InitRATFinder() {
  RegisterFunctionPass(
      "RATFINDER",
      MaoFunctionPassManager::GenericPassCreator<RatFinderPass>);
}
