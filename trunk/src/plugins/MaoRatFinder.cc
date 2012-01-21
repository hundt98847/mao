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

#include "Mao.h"
#include <set>

namespace {

PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(RATFINDER, "Finds potential RAT stalls in the code", 0) {
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
  //
  // Trace level 1: Prints out the basic block which have possible
  //                RAT stalls.
  // Trace level 2: Prints out each instruction that triggers a possible
  //                RAT stall.
  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_);

    // Used registers
    const reg_entry *r_eax = GetRegFromName("eax");
    const reg_entry *r_ebx = GetRegFromName("ebx");
    const reg_entry *r_ecx = GetRegFromName("ecx");
    const reg_entry *r_edx = GetRegFromName("edx");
    const reg_entry *r_edi = GetRegFromName("edi");
    const reg_entry *r_esi = GetRegFromName("esi");
    const reg_entry *r_ebp = GetRegFromName("ebp");
    const reg_entry *r_esp = GetRegFromName("esp");
    const reg_entry *r_r8d = GetRegFromName("r8d");
    const reg_entry *r_r9d = GetRegFromName("r9d");
    const reg_entry *r_r10d = GetRegFromName("r10d");
    const reg_entry *r_r11d = GetRegFromName("r11d");
    const reg_entry *r_r12d = GetRegFromName("r12d");
    const reg_entry *r_r13d = GetRegFromName("r13d");
    const reg_entry *r_r14d = GetRegFromName("r14d");
    const reg_entry *r_r15d = GetRegFromName("r15d");
    const reg_entry *r_rax = GetRegFromName("rax");
    const reg_entry *r_rbx = GetRegFromName("rbx");
    const reg_entry *r_rcx = GetRegFromName("rcx");
    const reg_entry *r_rdx = GetRegFromName("rdx");
    const reg_entry *r_rdi = GetRegFromName("rdi");
    const reg_entry *r_rsi = GetRegFromName("rsi");
    const reg_entry *r_rbp = GetRegFromName("rbp");
    const reg_entry *r_rsp = GetRegFromName("rsp");
    const reg_entry *r_r8  = GetRegFromName("r8");
    const reg_entry *r_r9  = GetRegFromName("r9");
    const reg_entry *r_r10 = GetRegFromName("r10");
    const reg_entry *r_r11 = GetRegFromName("r11");
    const reg_entry *r_r12 = GetRegFromName("r12");
    const reg_entry *r_r13 = GetRegFromName("r13");
    const reg_entry *r_r14 = GetRegFromName("r14");
    const reg_entry *r_r15 = GetRegFromName("r15");

    FORALL_CFG_BB(cfg, it) {
      MaoEntry *first = (*it)->GetFirstInstruction();
      if (!first) continue;

      // Number of found RAT stall possibilities for this basic block
      int num_rat_stall_possibilites = 0;

      FORALL_BB_ENTRY(it, entry) {
        if (!entry->IsInstruction()) continue;
        InstructionEntry *current_insn = entry->AsInstruction();

        // Get the registers that are defined by the current instruction.
        // Normally this is only one register/instruction
        std::set<const reg_entry *> defined_regs =
            GetDefinedRegisters(current_insn);
        // For each defined registers:
        for (std::set<const reg_entry *>::const_iterator iter =
                 defined_regs.begin();
             iter != defined_regs.end();
             ++iter) {
          const reg_entry *defined_reg = *iter;

          // Since 32-bit writes in 64-bit mode are automatically
          // zero extended to the 64-bit registers, we upgrade
          // the register definition.
          if (unit_->Is64BitMode()) {
            // Upgrade eXX register to rXX registers!
            if      (defined_reg == r_eax)  defined_reg = r_rax;
            else if (defined_reg == r_ebx)  defined_reg = r_rbx;
            else if (defined_reg == r_ecx)  defined_reg = r_rcx;
            else if (defined_reg == r_edx)  defined_reg = r_rdx;
            else if (defined_reg == r_edi)  defined_reg = r_rdi;
            else if (defined_reg == r_esi)  defined_reg = r_rsi;
            else if (defined_reg == r_ebp)  defined_reg = r_rbp;
            else if (defined_reg == r_esp)  defined_reg = r_rsp;
            else if (defined_reg == r_r8d)  defined_reg = r_r8;
            else if (defined_reg == r_r9d)  defined_reg = r_r9;
            else if (defined_reg == r_r10d) defined_reg = r_r10;
            else if (defined_reg == r_r11d) defined_reg = r_r11;
            else if (defined_reg == r_r12d) defined_reg = r_r12;
            else if (defined_reg == r_r13d) defined_reg = r_r13;
            else if (defined_reg == r_r14d) defined_reg = r_r14;
            else if (defined_reg == r_r15d) defined_reg = r_r15;
          }

          // If this is a "small" register, check for writes later in the
          // that might cause a RAT stall.
          if (!( GetParentRegs(defined_reg) ==
                 BitString(256, 4, 0x0ull, 0x0ull, 0x0ull, 0x0ull))) {
            // Loop through the remaining instructions in the basic block.
            InstructionEntry *next = current_insn->nextInstruction();
            MaoEntry *last = (*it)->GetLastInstruction();
            while (next) {
              // Loop over all the defined registers.
              std::set<const reg_entry *> used_next_regs =
                  GetUsedRegisters(next);
              for (std::set<const reg_entry *>::const_iterator n_iter =
                       used_next_regs.begin();
                   n_iter != used_next_regs.end();
                   ++n_iter) {
                const reg_entry *used_next_reg = *n_iter;
                // Check if defined_next_reg is a parent of defained_reg.
                // Then this is a possible RAT stall.
                if (IsParent(used_next_reg, defined_reg)) {
                  num_rat_stall_possibilites++;
                  if (tracing_level() >= 2) {
                    Trace(2, "Possible RAT stall: ");
                    current_insn->PrintEntry(stderr);
                    break;
                  }
                }
              }  // while(next)
              // Exit conditions:
              // At the end of basic block.
              if (next == last)
                break;
              // We defined the registers again
              if (IsRegDefined(next, defined_reg))
                break;
              // We have already found a RAT possibility
              if (num_rat_stall_possibilites > 0)
                break;
              next = next->nextInstruction();
            }
          }
        }  // defined_regs
      }  // Entries

      if (num_rat_stall_possibilites > 0) {
        Trace(1, "Found %d RAT stall possibilities in basic block",
              num_rat_stall_possibilites);
        // Print the basic block, given the instruction.
        if (tracing_level() >= 1) {
          FORALL_BB_ENTRY(it, entry) {
            entry->PrintEntry(stderr);
          }
        }
      }
    }  // BB
    return true;
  }
};

REGISTER_PLUGIN_FUNC_PASS("RATFINDER", RatFinderPass)
}  // namespace
