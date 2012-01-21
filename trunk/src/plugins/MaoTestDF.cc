//
// Copyright 2009 Google Inc.
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

// Zero Extension Elimination
#include "Mao.h"
#include "MaoPlugin.h"

namespace {

PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(TESTDF, "Implements example analysis that uses MAO's "\
                   "dataflow analysis framework", 2) {
  OPTION_BOOL("liveness", true, "Run liveness analysis."),
  OPTION_BOOL("reachingdef", true, "Run reaching def. analysis."),
};

class TestDataFlowPass : public MaoFunctionPass {
 public:
  TestDataFlowPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("TESTDF", options, mao, function),
        liveness_(GetOptionBool("liveness")),
        reachingdef_(GetOptionBool("reachingdef")) {
    MAO_ASSERT_MSG(liveness_ || reachingdef_, "TESTDF has nothing to do.");
  }

  bool Go() {
    Trace(1, "Entering TESTDF for function %s:", function_->name().c_str());

    CFG *cfg = CFG::GetCFG(unit_, function_);

    if (liveness_) {
      Trace(1, "Test liveness:");
      // Create the problem instance
      Liveness liveness = Liveness(unit_,
                                   function_,
                                   cfg);

      // Now we can solve it.
      liveness.Solve();

      // Print the live registers for each instruction.
      FORALL_CFG_BB(cfg, it) {
        const BasicBlock *bb = *it;
        // Loop over the instructions.
        FORALL_BB_ENTRY(it, entry) {
          if (entry->IsInstruction()) {
            InstructionEntry *insn = entry->AsInstruction();
            std::string insn_str;
            insn->ToString(&insn_str);
            fprintf(stderr, "insn: %s\n", insn_str.c_str());
            BitString live_regs = liveness.GetLive(*bb, *insn);
            fprintf(stderr, "live: ");
            for (int i = 0; i < live_regs.number_of_bits(); ++i) {
              if (live_regs.Get(i)) {
                if (i < GetNumberOfRegisters())
                  fprintf(stderr, "%s ", GetRegName(i));
                else
                  fprintf(stderr, "* ");
              }
            }
            fprintf(stderr, "\n");
          } else {
            std::string s;
            entry->ToString(&s);
            fprintf(stderr, "entry: %s\n", s.c_str());
          }
        }
      }
    }


    if (reachingdef_) {
      Trace(1, "Test reaching defs:");
      // Create the problem instance
      ReachingDefs rd_problem = ReachingDefs(unit_,
                                             function_,
                                             cfg);
      // Now we can solve it.
      rd_problem.Solve();

      // Print out the results!
      // For each instruction, print out the reaching definitions.
      FORALL_CFG_BB(cfg, it) {
        const BasicBlock *bb = *it;
        // Loop over the instructions.
        FORALL_BB_ENTRY(it, entry) {
          if (!entry->IsInstruction()) continue;
          InstructionEntry *insn = entry->AsInstruction();
          std::string insn_str;
          insn->ToString(&insn_str);
           fprintf(stderr, "\ninsn: %s\n", insn_str.c_str());
          // Get the list of definitions:
          // TODO(martint): Create macros to iterate over defined registers.
          BitString used_registers = GetRegisterUseMask(insn, true);
          // Loop over them:
          for (int reg_num = 0; reg_num < used_registers.number_of_bits();
               ++reg_num) {
            if (used_registers.Get(reg_num)) {
              fprintf(stderr, "Uses: %s\n", GetRegName(reg_num));
              // Regnum was defined!
              std::list<Definition> defs = rd_problem.GetReachingDefs(*bb,
                                                                      *insn,
                                                                      reg_num);
              if (defs.size() == 0) {
                fprintf(stderr, "%5s: No definitions found\n",
                        GetRegName(reg_num));
              }
              for (std::list<Definition>::const_iterator iter = defs.begin();
                   iter != defs.end();
                   ++iter) {
                std::string insn_str;
                MAO_ASSERT(iter->register_number() == reg_num);
                iter->instruction()->ToString(&insn_str);
                fprintf(stderr, "%5s: Defined in bb:%s inst:%s\n",
                        GetRegName(reg_num),
                        iter->bb()->label(),
                        insn_str.c_str());
              }
            }
          }
        }
      }
    }
    return true;
  }
 private:
  bool liveness_;
  bool reachingdef_;
};

REGISTER_PLUGIN_FUNC_PASS("TESTDF", TestDataFlowPass)
}  // namespace
