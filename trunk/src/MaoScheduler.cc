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

// Scheduler that minimizes effects such as reservation station bottlenecks
//
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"
#include "MaoUtil.h"


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(SCHEDULER, 0) {
};

#define MAX_REGS 256

#define NO_DEP 0
#define TRUE_DEP 1
#define OUTPUT_DEP 2
#define ANTI_DEP 4

class SchedulerPass : public MaoFunctionPass {
 public:
  class DependenceDag {
   public:
    DependenceDag (int num_instructions, std::string *insn_str) {
      dag_insn_str_ = insn_str;
      num_instructions_ = num_instructions;
      adj_matrix_ = new char[num_instructions*num_instructions];
      memset (adj_matrix_, 0, num_instructions*num_instructions);
    }

    ~DependenceDag () {
      delete adj_matrix_;
    }

    void AddEdge (int u, int v, int type) {
      adj_matrix_[u*num_instructions_+v] |= type;
    }
    void Print (FILE *file) {
      fprintf (file, "#instructions = %d\n", num_instructions_);
      for (int i=0; i<num_instructions_; i++) {
        fprintf (file, "%s -> ", dag_insn_str_[i].c_str());
        for (int j=0; j<num_instructions_; j++) {
          if(adj_matrix_[i*num_instructions_ + j] != NO_DEP)
            fprintf (file, "%s[%d],  ", dag_insn_str_[j].c_str(), adj_matrix_[i*num_instructions_ + j]);
        }
        fprintf (file, "\n");
      }
    }

   private:
    int num_instructions_;
    char *adj_matrix_;
    std::string *dag_insn_str_;
  };

  SchedulerPass(MaoOptionMap *options, MaoUnit *mao, Function *func)
      : MaoFunctionPass("SCHEDULER", options, mao, func) { }

  bool Go() {
    std::list<MaoEntry *> redundants;
    CFG *cfg = CFG::GetCFG (unit_, function_);
    int last_writer[MAX_REGS];

    FORALL_CFG_BB(cfg, bb_iterator) {
      int insns_in_bb = 0;
      FORALL_BB_ENTRY(bb_iterator, entry_iter) {
        MaoEntry *entry = *entry_iter;
        if (!entry->IsInstruction())
          continue;
        insns_in_bb++;
      }

      insn_str_ = new std::string[insns_in_bb];
      DependenceDag *dag = new DependenceDag(insns_in_bb, insn_str_);
      insns_in_bb = 0;
      memset (last_writer,0xFF, MAX_REGS*sizeof(int));
      FORALL_BB_ENTRY(bb_iterator, entry_iter) {
        MaoEntry *entry = *entry_iter;
        if (!entry->IsInstruction())
          continue;
        InstructionEntry *insn = entry->AsInstruction();
        insn->ToString(&insn_str_[insns_in_bb]);
        Trace (2, "Instruction %d: %s\n", insns_in_bb, insn_str_[insns_in_bb].c_str());
        insn_map_[insn_str_[insns_in_bb].c_str()] = insns_in_bb;


        BitString src_regs_mask = GetSrcRegisters (insn);
        BitString dest_regs_mask = GetDestRegisters (insn);
        int index=0;
        while ((index = src_regs_mask.NextSetBit(index)) != -1) {
          Trace (2, "src reg = %d", index);
          if (last_writer[index] >=0)
            dag->AddEdge (last_writer[index], insns_in_bb, TRUE_DEP);
          index++;
        }

        index=0;
        while ((index = dest_regs_mask.NextSetBit(index)) != -1) {
          Trace (2, "dest reg = %d", index);
          if (last_writer[index] >=0)
            dag->AddEdge (last_writer[index], insns_in_bb, OUTPUT_DEP);
          last_writer[index]=insns_in_bb;
          index++;
        }

        insns_in_bb++;

      }
      Trace (2, "Dag for new bb:");
      dag->Print(stderr);
      delete [] insn_str_;
    }

    return true;
  }

 private:
  std::map<const char*, int, ltstr> insn_map_;
  std::string *insn_str_;
  BitString GetSrcRegisters (InstructionEntry *insn);
  BitString GetDestRegisters (InstructionEntry *insn);



};
BitString SchedulerPass::GetSrcRegisters (InstructionEntry *insn) {
  int operands = insn->NumOperands();
  BitString mask;
  for (int i = 0; i < operands; i++) {
    if (!insn->IsRegisterOperand (i))
      continue;
    const reg_entry *reg = insn->GetRegisterOperand(i);
    mask = mask | GetMaskForRegister(reg->reg_name);
  }
  if (insn->HasBaseRegister()) {
    const reg_entry *reg = insn->GetBaseRegister();
    mask = mask | GetMaskForRegister(reg->reg_name);
  }
  if (insn->HasIndexRegister()) {
    const reg_entry *reg = insn->GetIndexRegister();
    mask = mask | GetMaskForRegister(reg->reg_name);
  }
  return mask;

}

BitString SchedulerPass::GetDestRegisters (InstructionEntry *insn) {
  BitString def_mask = GetRegisterDefMask(insn);
  return def_mask;
}

// External Entry Point
//

void InitScheduler() {
  RegisterFunctionPass(
      "SCHEDULER",
      MaoFunctionPassManager::GenericPassCreator<SchedulerPass>);
}
